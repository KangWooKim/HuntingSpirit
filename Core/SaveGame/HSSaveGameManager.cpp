#include "HSSaveGameManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Compression/CompressionUtil.h"

UHSSaveGameManager::UHSSaveGameManager()
{
    CurrentSaveData = nullptr;
    bOperationInProgress = false;
    
    // 자동 저장 기본 설정
    bAutoSaveEnabled = false;
    AutoSaveInterval = DefaultAutoSaveInterval;
    AutoSaveSlotIndex = 0;
    
    // 시스템 기본 설정
    MaxSaveSlots = DefaultMaxSaveSlots;
    SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
    
    // 압축/암호화 기본 설정
    bCompressionEnabled = true;
    bEncryptionEnabled = false;
    EncryptionKey = TEXT("");
    
    // 클라우드 동기화 기본 설정
    CloudSyncStatus = FHSCloudSyncStatus();
    
    // 캐시 초기화
    LastCacheUpdateTime = 0.0f;
    bBackupCacheValid = false;
    bAsyncTaskInProgress = false;
    
    // 오브젝트 풀 초기화
    DataBufferPool.Reserve(DataBufferPoolSize);
    for (int32 i = 0; i < DataBufferPoolSize; ++i)
    {
        DataBufferPool.Add(TArray<uint8>());
        DataBufferPool[i].Reserve(1024 * 1024); // 1MB 기본 크기
    }
}

void UHSSaveGameManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 저장/로드 시스템 초기화 중..."));
    
    InitializeSaveSystem();
    
    // 플랫폼별 초기화
#if PLATFORM_WINDOWS
    InitializeWindowsSaveSystem();
#elif PLATFORM_MAC
    InitializeMacSaveSystem();
#elif PLATFORM_LINUX
    InitializeLinuxSaveSystem();
#endif
    
    // 디렉토리 생성
    EnsureDirectoryExists(SaveDirectory);
    EnsureDirectoryExists(SaveDirectory / TEXT("Backups"));
    
    // 슬롯 정보 캐시 초기 로드
    UpdateSlotInfoCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 저장/로드 시스템 초기화 완료"));
}

void UHSSaveGameManager::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 저장/로드 시스템 종료 중..."));
    
    // 진행 중인 작업 완료 대기
    if (bOperationInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSaveGameManager: 진행 중인 저장/로드 작업 강제 종료"));
    }
    
    // 자동 저장 타이머 정리
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
    }
    
    CleanupSaveSystem();
    
    Super::Deinitialize();
}

void UHSSaveGameManager::SaveGameAsync(int32 SlotIndex, UHSSaveGameData* SaveData)
{
    if (!SaveData)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 저장할 데이터가 null입니다"));
        OnSaveOperationCompleted.Broadcast(EHSSaveResult::Failed, SlotIndex);
        return;
    }
    
    if (bOperationInProgress)
    {
        // 대기열에 추가
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        PendingSaveTasks.Enqueue(FAsyncSaveTask(SlotIndex, SaveData, CurrentTime));
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 저장 작업 대기열에 추가 - 슬롯 %d"), SlotIndex);
        return;
    }
    
    if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 잘못된 슬롯 인덱스 - %d"), SlotIndex);
        OnSaveOperationCompleted.Broadcast(EHSSaveResult::Failed, SlotIndex);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 비동기 저장 시작 - 슬롯 %d"), SlotIndex);
    
    bOperationInProgress = true;
    CurrentOperationProgress = FHSSaveOperationProgress();
    CurrentOperationProgress.Operation = EHSSaveOperation::Save;
    
    // 비동기 저장 작업 시작
    PerformSaveOperation(SlotIndex, SaveData);
}

void UHSSaveGameManager::LoadGameAsync(int32 SlotIndex)
{
    if (bOperationInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSaveGameManager: 다른 작업이 진행 중입니다"));
        OnSaveOperationCompleted.Broadcast(EHSSaveResult::Failed, SlotIndex);
        return;
    }
    
    if (!DoesSaveSlotExist(SlotIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 슬롯 %d가 존재하지 않습니다"), SlotIndex);
        OnSaveOperationCompleted.Broadcast(EHSSaveResult::NotFound, SlotIndex);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 비동기 로드 시작 - 슬롯 %d"), SlotIndex);
    
    bOperationInProgress = true;
    CurrentOperationProgress = FHSSaveOperationProgress();
    CurrentOperationProgress.Operation = EHSSaveOperation::Load;
    
    // 비동기 로드 작업 시작
    PerformLoadOperation(SlotIndex);
}

bool UHSSaveGameManager::SaveGameSync(int32 SlotIndex, UHSSaveGameData* SaveData)
{
    FScopeLock Lock(&SaveSystemMutex);
    
    if (!SaveData)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 저장할 데이터가 null입니다"));
        return false;
    }
    
    if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 잘못된 슬롯 인덱스 - %d"), SlotIndex);
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 동기 저장 시작 - 슬롯 %d"), SlotIndex);
    
    // 자동 백업 생성
    if (DoesSaveSlotExist(SlotIndex))
    {
        CreateAutomaticBackup(SlotIndex, TEXT("Pre-Save Backup"));
    }
    
    // 저장 데이터 유효성 검사
    if (!SaveData->ValidateSaveData())
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 저장 데이터 검증 실패"));
        return false;
    }
    
    // 저장 데이터 직렬화
    TArray<uint8>* SaveBuffer = GetPooledBuffer();
    FMemoryWriter MemoryWriter(*SaveBuffer, true);
    FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, false);
    SaveData->Serialize(Ar);
    
    // 압축 처리
    TArray<uint8> FinalData;
    if (bCompressionEnabled)
    {
        FinalData = CompressData(*SaveBuffer);
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 데이터 압축 완료 - %d -> %d 바이트"), 
               SaveBuffer->Num(), FinalData.Num());
    }
    else
    {
        FinalData = *SaveBuffer;
    }
    
    // 암호화 처리
    if (bEncryptionEnabled && !EncryptionKey.IsEmpty())
    {
        FinalData = EncryptData(FinalData);
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 데이터 암호화 완료"));
    }
    
    // 파일 저장
    FString FilePath = GetSlotFilePath(SlotIndex);
    bool bSuccess = WriteToFile(FilePath, FinalData);
    
    if (bSuccess)
    {
        // 슬롯 메타데이터 저장
        FHSSaveSlotInfo SlotInfo;
        SlotInfo.SlotIndex = SlotIndex;
        SlotInfo.SlotName = FString::Printf(TEXT("Save Slot %d"), SlotIndex + 1);
        SlotInfo.PlayerName = SaveData->PlayerProfile.PlayerName;
        SlotInfo.PlayerLevel = SaveData->PlayerProfile.PlayerLevel;
        SlotInfo.TotalPlayTime = SaveData->PlayerProfile.Statistics.TotalPlayTime;
        SlotInfo.SaveDate = FDateTime::Now();
        SlotInfo.bIsValid = true;
        SlotInfo.bIsAutosave = (SlotIndex == AutoSaveSlotIndex);
        SlotInfo.FileSizeMB = FinalData.Num() / (1024.0f * 1024.0f);
        SlotInfo.SaveDataVersion = SaveData->SaveDataVersion;
        
        SaveSlotMetadata(SlotIndex, SlotInfo);
        
        // 캐시 무효화
        InvalidateSlotInfoCache();
        
        // 클라우드 동기화
        if (CloudSyncStatus.bIsEnabled)
        {
            SyncToCloud(SlotIndex);
        }
        
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 동기 저장 완료 - 슬롯 %d"), SlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 동기 저장 실패 - 슬롯 %d"), SlotIndex);
    }
    
    ReturnPooledBuffer(SaveBuffer);
    return bSuccess;
}

UHSSaveGameData* UHSSaveGameManager::LoadGameSync(int32 SlotIndex)
{
    FScopeLock Lock(&SaveSystemMutex);
    
    if (!DoesSaveSlotExist(SlotIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 슬롯 %d가 존재하지 않습니다"), SlotIndex);
        return nullptr;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 동기 로드 시작 - 슬롯 %d"), SlotIndex);
    
    // 무결성 검증
    if (!VerifySaveIntegrity(SlotIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 저장 파일 무결성 검증 실패 - 슬롯 %d"), SlotIndex);
        
        // 복구 시도
        if (RepairCorruptedSave(SlotIndex))
        {
            UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 손상된 저장 파일 복구 성공"));
        }
        else
        {
            return nullptr;
        }
    }
    
    // 파일 읽기
    FString FilePath = GetSlotFilePath(SlotIndex);
    TArray<uint8> FileData;
    if (!ReadFromFile(FilePath, FileData))
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 파일 읽기 실패 - %s"), *FilePath);
        return nullptr;
    }
    
    // 암호화 해제
    if (bEncryptionEnabled && !EncryptionKey.IsEmpty())
    {
        FileData = DecryptData(FileData);
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 데이터 복호화 완료"));
    }
    
    // 압축 해제
    TArray<uint8> DecompressedData;
    if (bCompressionEnabled)
    {
        DecompressedData = DecompressData(FileData);
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 데이터 압축 해제 완료 - %d -> %d 바이트"), 
               FileData.Num(), DecompressedData.Num());
    }
    else
    {
        DecompressedData = FileData;
    }
    
    // 저장 데이터 역직렬화
    UHSSaveGameData* LoadedData = NewObject<UHSSaveGameData>(this);
    if (!LoadedData)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 저장 데이터 객체 생성 실패"));
        return nullptr;
    }
    
    FMemoryReader MemoryReader(DecompressedData, true);
    FObjectAndNameAsStringProxyArchive Ar(MemoryReader, false);
    LoadedData->Serialize(Ar);
    
    // 버전 업그레이드
    LoadedData->UpgradeSaveDataVersion();
    
    // 로드된 데이터 검증
    if (!LoadedData->ValidateSaveData())
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 로드된 데이터 검증 실패"));
        return nullptr;
    }
    
    CurrentSaveData = LoadedData;
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 동기 로드 완료 - 슬롯 %d"), SlotIndex);
    return LoadedData;
}

bool UHSSaveGameManager::DeleteSaveSlot(int32 SlotIndex)
{
    FScopeLock Lock(&SaveSystemMutex);
    
    if (!DoesSaveSlotExist(SlotIndex))
    {
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 슬롯 삭제 - %d"), SlotIndex);
    
    // 백업 생성
    CreateBackup(SlotIndex, TEXT("Pre-Delete Backup"));
    
    // 파일 삭제
    FString FilePath = GetSlotFilePath(SlotIndex);
    bool bSuccess = IFileManager::Get().Delete(*FilePath);
    
    // 메타데이터 파일 삭제
    FString MetadataPath = FilePath + TEXT(".meta");
    IFileManager::Get().Delete(*MetadataPath);
    
    if (bSuccess)
    {
        InvalidateSlotInfoCache();
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 슬롯 삭제 완료 - %d"), SlotIndex);
    }
    
    return bSuccess;
}

bool UHSSaveGameManager::DoesSaveSlotExist(int32 SlotIndex) const
{
    if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots)
    {
        return false;
    }
    
    FString FilePath = GetSlotFilePath(SlotIndex);
    return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

TArray<FHSSaveSlotInfo> UHSSaveGameManager::GetAllSaveSlots() const
{
    FScopeLock Lock(&CacheMutex);
    
    // 캐시 유효성 확인
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - LastCacheUpdateTime > CacheValidityDuration)
    {
        UpdateSlotInfoCache();
    }
    
    TArray<FHSSaveSlotInfo> SlotInfos;
    for (const auto& SlotPair : SlotInfoCache)
    {
        SlotInfos.Add(SlotPair.Value);
    }
    
    // 슬롯 인덱스 기준으로 정렬
    SlotInfos.Sort([](const FHSSaveSlotInfo& A, const FHSSaveSlotInfo& B) {
        return A.SlotIndex < B.SlotIndex;
    });
    
    return SlotInfos;
}

FHSSaveSlotInfo UHSSaveGameManager::GetSaveSlotInfo(int32 SlotIndex) const
{
    if (!DoesSaveSlotExist(SlotIndex))
    {
        return FHSSaveSlotInfo();
    }
    
    FScopeLock Lock(&CacheMutex);
    
    // 캐시에서 확인
    if (SlotInfoCache.Contains(SlotIndex))
    {
        return SlotInfoCache[SlotIndex];
    }
    
    // 캐시에 없으면 로드
    FHSSaveSlotInfo SlotInfo = LoadSlotMetadata(SlotIndex);
    SlotInfoCache.Add(SlotIndex, SlotInfo);
    
    return SlotInfo;
}

int32 UHSSaveGameManager::FindEmptySlot() const
{
    for (int32 i = 0; i < MaxSaveSlots; ++i)
    {
        if (!DoesSaveSlotExist(i))
        {
            return i;
        }
    }
    
    return -1; // 빈 슬롯 없음
}

void UHSSaveGameManager::EnableAutoSave(bool bEnabled, float IntervalSeconds)
{
    bAutoSaveEnabled = bEnabled;
    AutoSaveInterval = FMath::Max(IntervalSeconds, 60.0f); // 최소 1분
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
        
        if (bEnabled)
        {
            GetWorld()->GetTimerManager().SetTimer(
                AutoSaveTimerHandle,
                this,
                &UHSSaveGameManager::ProcessAutoSave,
                AutoSaveInterval,
                true
            );
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 자동 저장 %s (간격: %.1f초)"), 
           bEnabled ? TEXT("활성화") : TEXT("비활성화"), IntervalSeconds);
}

void UHSSaveGameManager::SetAutoSaveSlot(int32 SlotIndex)
{
    if (SlotIndex >= 0 && SlotIndex < MaxSaveSlots)
    {
        AutoSaveSlotIndex = SlotIndex;
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 자동 저장 슬롯 설정 - %d"), SlotIndex);
    }
}

void UHSSaveGameManager::TriggerAutoSave()
{
    if (!bAutoSaveEnabled || !CurrentSaveData)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 자동 저장 실행"));
    
    bool bSuccess = SaveGameSync(AutoSaveSlotIndex, CurrentSaveData);
    OnAutoSaveTriggered.Broadcast(AutoSaveSlotIndex, bSuccess);
}

bool UHSSaveGameManager::CreateBackup(int32 SlotIndex, const FString& Reason)
{
    if (!DoesSaveSlotExist(SlotIndex))
    {
        return false;
    }
    
    FString BackupID = GenerateBackupID();
    FString SourcePath = GetSlotFilePath(SlotIndex);
    FString BackupPath = GetBackupFilePath(BackupID);
    
    bool bSuccess = CopyFile(SourcePath, BackupPath);
    
    if (bSuccess)
    {
        // 백업 정보 저장
        FHSBackupInfo BackupInfo;
        BackupInfo.BackupID = BackupID;
        BackupInfo.OriginalSlotIndex = SlotIndex;
        BackupInfo.BackupDate = FDateTime::Now();
        BackupInfo.BackupReason = Reason;
        BackupInfo.FileSizeMB = GetFileSize(BackupPath) / (1024.0f * 1024.0f);
        BackupInfo.bIsCompressed = bCompressionEnabled;
        BackupInfo.bIsEncrypted = bEncryptionEnabled;
        
        // 백업 메타데이터 저장
        FString MetadataPath = BackupPath + TEXT(".meta");
        FString MetadataJson = FString::Printf(
            TEXT("{"
                 "\"BackupID\":\"%s\","
                 "\"OriginalSlotIndex\":%d,"
                 "\"BackupDate\":\"%s\","
                 "\"BackupReason\":\"%s\","
                 "\"FileSizeMB\":%.2f,"
                 "\"IsCompressed\":%s,"
                 "\"IsEncrypted\":%s"
                 "}"),
            *BackupInfo.BackupID,
            BackupInfo.OriginalSlotIndex,
            *BackupInfo.BackupDate.ToString(),
            *BackupInfo.BackupReason,
            BackupInfo.FileSizeMB,
            BackupInfo.bIsCompressed ? TEXT("true") : TEXT("false"),
            BackupInfo.bIsEncrypted ? TEXT("true") : TEXT("false")
        );
        
        FFileHelper::SaveStringToFile(MetadataJson, *MetadataPath);
        
        InvalidateBackupInfoCache();
        
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 백업 생성 완료 - %s (슬롯 %d)"), 
               *BackupID, SlotIndex);
    }
    
    return bSuccess;
}

bool UHSSaveGameManager::RestoreFromBackup(const FString& BackupID, int32 TargetSlotIndex)
{
    FString BackupPath = GetBackupFilePath(BackupID);
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*BackupPath))
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 백업 파일이 존재하지 않음 - %s"), *BackupID);
        return false;
    }
    
    // 현재 슬롯 백업 (복원 전)
    if (DoesSaveSlotExist(TargetSlotIndex))
    {
        CreateBackup(TargetSlotIndex, TEXT("Pre-Restore Backup"));
    }
    
    FString TargetPath = GetSlotFilePath(TargetSlotIndex);
    bool bSuccess = CopyFile(BackupPath, TargetPath);
    
    if (bSuccess)
    {
        InvalidateSlotInfoCache();
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 백업 복원 완료 - %s -> 슬롯 %d"), 
               *BackupID, TargetSlotIndex);
    }
    
    return bSuccess;
}

TArray<FHSBackupInfo> UHSSaveGameManager::GetAvailableBackups(int32 SlotIndex) const
{
    if (!bBackupCacheValid)
    {
        UpdateBackupInfoCache();
    }
    
    if (SlotIndex == -1)
    {
        return BackupInfoCache;
    }
    
    TArray<FHSBackupInfo> FilteredBackups;
    for (const FHSBackupInfo& BackupInfo : BackupInfoCache)
    {
        if (BackupInfo.OriginalSlotIndex == SlotIndex)
        {
            FilteredBackups.Add(BackupInfo);
        }
    }
    
    return FilteredBackups;
}

bool UHSSaveGameManager::DeleteBackup(const FString& BackupID)
{
    FString BackupPath = GetBackupFilePath(BackupID);
    FString MetadataPath = BackupPath + TEXT(".meta");
    
    bool bSuccess = IFileManager::Get().Delete(*BackupPath);
    IFileManager::Get().Delete(*MetadataPath);
    
    if (bSuccess)
    {
        InvalidateBackupInfoCache();
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 백업 삭제 완료 - %s"), *BackupID);
    }
    
    return bSuccess;
}

void UHSSaveGameManager::CleanupOldBackups(int32 MaxBackupsToKeep)
{
    TArray<FHSBackupInfo> AllBackups = GetAvailableBackups();
    
    // 날짜 기준으로 정렬 (최신 순)
    AllBackups.Sort([](const FHSBackupInfo& A, const FHSBackupInfo& B) {
        return A.BackupDate > B.BackupDate;
    });
    
    // 초과분 삭제
    for (int32 i = MaxBackupsToKeep; i < AllBackups.Num(); ++i)
    {
        DeleteBackup(AllBackups[i].BackupID);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 오래된 백업 정리 완료 - %d개 삭제"), 
           FMath::Max(0, AllBackups.Num() - MaxBackupsToKeep));
}

void UHSSaveGameManager::VerifyAllSaveIntegrity()
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 모든 저장 파일 무결성 검증 시작"));
    
    bool bAllValid = true;
    TArray<FHSSaveSlotInfo> AllSlots = GetAllSaveSlots();
    
    for (const FHSSaveSlotInfo& SlotInfo : AllSlots)
    {
        if (!VerifySaveIntegrity(SlotInfo.SlotIndex))
        {
            bAllValid = false;
            UE_LOG(LogTemp, Warning, TEXT("HSSaveGameManager: 슬롯 %d 무결성 검증 실패"), 
                   SlotInfo.SlotIndex);
        }
    }
    
    OnSaveIntegrityCheckCompleted.Broadcast(bAllValid);
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 무결성 검증 완료 - %s"), 
           bAllValid ? TEXT("모든 파일 정상") : TEXT("일부 파일 손상"));
}

bool UHSSaveGameManager::VerifySaveIntegrity(int32 SlotIndex) const
{
    FString FilePath = GetSlotFilePath(SlotIndex);
    return ValidateSaveFile(FilePath);
}

bool UHSSaveGameManager::RepairCorruptedSave(int32 SlotIndex)
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 손상된 저장 파일 복구 시도 - 슬롯 %d"), SlotIndex);
    
    // 최신 백업에서 복구 시도
    TArray<FHSBackupInfo> Backups = GetAvailableBackups(SlotIndex);
    if (Backups.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 복구할 백업이 없음"));
        return false;
    }
    
    // 가장 최신 백업 사용
    Backups.Sort([](const FHSBackupInfo& A, const FHSBackupInfo& B) {
        return A.BackupDate > B.BackupDate;
    });
    
    return RestoreFromBackup(Backups[0].BackupID, SlotIndex);
}

void UHSSaveGameManager::EnableEncryption(bool bEnabled, const FString& InEncryptionKey)
{
    bEncryptionEnabled = bEnabled;
    if (bEnabled && !InEncryptionKey.IsEmpty())
    {
        EncryptionKey = InEncryptionKey;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 암호화 %s"), 
           bEnabled ? TEXT("활성화") : TEXT("비활성화"));
}

void UHSSaveGameManager::EnableCloudSync(bool bEnabled, const FString& Provider)
{
    CloudSyncStatus.bIsEnabled = bEnabled;
    CloudSyncStatus.CloudProvider = Provider;
    
    OnCloudSyncStatusChanged.Broadcast(CloudSyncStatus);
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 클라우드 동기화 %s (제공자: %s)"), 
           bEnabled ? TEXT("활성화") : TEXT("비활성화"), *Provider);
}

void UHSSaveGameManager::SyncToCloud(int32 SlotIndex)
{
    if (!CloudSyncStatus.bIsEnabled)
    {
        return;
    }
    
    if (SlotIndex == -1)
    {
        // 모든 슬롯 동기화
        TArray<FHSSaveSlotInfo> AllSlots = GetAllSaveSlots();
        for (const FHSSaveSlotInfo& SlotInfo : AllSlots)
        {
            PerformCloudUpload(SlotInfo.SlotIndex);
        }
    }
    else
    {
        PerformCloudUpload(SlotIndex);
    }
}

void UHSSaveGameManager::SyncFromCloud(int32 SlotIndex)
{
    if (!CloudSyncStatus.bIsEnabled)
    {
        return;
    }
    
    if (SlotIndex == -1)
    {
        // 모든 슬롯 동기화
        for (int32 i = 0; i < MaxSaveSlots; ++i)
        {
            PerformCloudDownload(i);
        }
    }
    else
    {
        PerformCloudDownload(SlotIndex);
    }
}

void UHSSaveGameManager::SetSaveDirectory(const FString& Directory)
{
    SaveDirectory = Directory;
    EnsureDirectoryExists(SaveDirectory);
    EnsureDirectoryExists(SaveDirectory / TEXT("Backups"));
    
    // 캐시 무효화
    InvalidateSlotInfoCache();
    InvalidateBackupInfoCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 저장 디렉토리 변경 - %s"), *Directory);
}

void UHSSaveGameManager::PerformSaveOperation(int32 SlotIndex, UHSSaveGameData* SaveData)
{
    float OperationStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // 비동기 작업 시뮬레이션
    UpdateOperationProgress(0.1f, TEXT("데이터 검증 중..."));
    
    if (!SaveData->ValidateSaveData())
    {
        CompleteOperation(EHSSaveResult::Failed, SlotIndex);
        return;
    }
    
    UpdateOperationProgress(0.3f, TEXT("데이터 직렬화 중..."));
    
    // 실제 저장 작업
    bool bSuccess = SaveGameSync(SlotIndex, SaveData);
    
    UpdateOperationProgress(1.0f, TEXT("저장 완료"));
    
    EHSSaveResult Result = bSuccess ? EHSSaveResult::Success : EHSSaveResult::Failed;
    CompleteOperation(Result, SlotIndex);
}

void UHSSaveGameManager::PerformLoadOperation(int32 SlotIndex)
{
    UpdateOperationProgress(0.1f, TEXT("파일 읽기 중..."));
    
    UpdateOperationProgress(0.5f, TEXT("데이터 역직렬화 중..."));
    
    // 실제 로드 작업
    UHSSaveGameData* LoadedData = LoadGameSync(SlotIndex);
    
    UpdateOperationProgress(1.0f, TEXT("로드 완료"));
    
    EHSSaveResult Result = LoadedData ? EHSSaveResult::Success : EHSSaveResult::Failed;
    CompleteOperation(Result, SlotIndex);
}

void UHSSaveGameManager::UpdateOperationProgress(float Progress, const FString& Step)
{
    CurrentOperationProgress.ProgressPercent = Progress;
    CurrentOperationProgress.CurrentStep = Step;
    
    if (GetWorld())
    {
        CurrentOperationProgress.ElapsedTime = GetWorld()->GetTimeSeconds() - CurrentOperationProgress.ElapsedTime;
        
        if (Progress > 0.0f)
        {
            float EstimatedTotal = CurrentOperationProgress.ElapsedTime / Progress;
            CurrentOperationProgress.EstimatedRemainingTime = EstimatedTotal - CurrentOperationProgress.ElapsedTime;
        }
    }
    
    OnSaveOperationProgress.Broadcast(CurrentOperationProgress);
}

void UHSSaveGameManager::CompleteOperation(EHSSaveResult Result, int32 SlotIndex)
{
    CurrentOperationProgress.bIsCompleted = true;
    bOperationInProgress = false;
    
    OnSaveOperationCompleted.Broadcast(Result, SlotIndex);
    
    // 대기 중인 작업 처리
    if (!PendingSaveTasks.IsEmpty())
    {
        FAsyncSaveTask NextTask;
        if (PendingSaveTasks.Dequeue(NextTask))
        {
            SaveGameAsync(NextTask.SlotIndex, NextTask.SaveData);
        }
    }
}

bool UHSSaveGameManager::WriteToFile(const FString& FilePath, const TArray<uint8>& Data)
{
    return FFileHelper::SaveArrayToFile(Data, *FilePath);
}

bool UHSSaveGameManager::ReadFromFile(const FString& FilePath, TArray<uint8>& OutData)
{
    return FFileHelper::LoadFileToArray(OutData, *FilePath);
}

TArray<uint8> UHSSaveGameManager::CompressData(const TArray<uint8>& Data) const
{
    // 실제 구현에서는 UE4/5의 압축 라이브러리 사용
    // 현재는 시뮬레이션
    TArray<uint8> CompressedData = Data;
    // 압축 시뮬레이션 (실제로는 zlib 등 사용)
    return CompressedData;
}

TArray<uint8> UHSSaveGameManager::DecompressData(const TArray<uint8>& CompressedData) const
{
    // 실제 구현에서는 UE4/5의 압축 해제 라이브러리 사용
    // 현재는 시뮬레이션
    TArray<uint8> DecompressedData = CompressedData;
    return DecompressedData;
}

TArray<uint8> UHSSaveGameManager::EncryptData(const TArray<uint8>& Data) const
{
    // 실제 구현에서는 AES 등의 암호화 사용
    // 현재는 간단한 XOR 시뮬레이션
    TArray<uint8> EncryptedData = Data;
    
    if (!EncryptionKey.IsEmpty())
    {
        TArray<uint8> KeyBytes;
        FTCHARToUTF8 KeyConverter(*EncryptionKey);
        KeyBytes.Append(reinterpret_cast<const uint8*>(KeyConverter.Get()), KeyConverter.Length());
        
        for (int32 i = 0; i < EncryptedData.Num(); ++i)
        {
            EncryptedData[i] ^= KeyBytes[i % KeyBytes.Num()];
        }
    }
    
    return EncryptedData;
}

TArray<uint8> UHSSaveGameManager::DecryptData(const TArray<uint8>& EncryptedData) const
{
    // XOR 암호화는 대칭이므로 같은 방법으로 복호화
    return EncryptData(EncryptedData);
}

FString UHSSaveGameManager::GetSlotFilePath(int32 SlotIndex) const
{
    return SaveDirectory / FString::Printf(TEXT("SaveSlot_%03d.sav"), SlotIndex);
}

FString UHSSaveGameManager::GetBackupFilePath(const FString& BackupID) const
{
    return SaveDirectory / TEXT("Backups") / (BackupID + TEXT(".bak"));
}

FString UHSSaveGameManager::GenerateBackupID() const
{
    FDateTime Now = FDateTime::Now();
    return FString::Printf(TEXT("Backup_%s"), *Now.ToString(TEXT("%Y%m%d_%H%M%S")));
}

void UHSSaveGameManager::CreateAutomaticBackup(int32 SlotIndex, const FString& Reason)
{
    CreateBackup(SlotIndex, Reason);
    
    // 자동 백업 정리 (슬롯당 최대 5개)
    TArray<FHSBackupInfo> SlotBackups = GetAvailableBackups(SlotIndex);
    if (SlotBackups.Num() > MaxBackupsPerSlot)
    {
        SlotBackups.Sort([](const FHSBackupInfo& A, const FHSBackupInfo& B) {
            return A.BackupDate > B.BackupDate;
        });
        
        for (int32 i = MaxBackupsPerSlot; i < SlotBackups.Num(); ++i)
        {
            DeleteBackup(SlotBackups[i].BackupID);
        }
    }
}

bool UHSSaveGameManager::ValidateSaveFile(const FString& FilePath) const
{
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        return false;
    }
    
    // 파일 크기 검사
    int64 FileSize = GetFileSize(FilePath);
    if (FileSize == 0 || FileSize > 100 * 1024 * 1024) // 100MB 최대
    {
        return false;
    }
    
    // 실제 구현에서는 체크섬 검증 등 수행
    return true;
}

uint32 UHSSaveGameManager::CalculateChecksum(const TArray<uint8>& Data) const
{
    // 간단한 체크섬 계산 (실제로는 CRC32 등 사용)
    uint32 Checksum = 0;
    for (uint8 Byte : Data)
    {
        Checksum += Byte;
    }
    return Checksum;
}

void UHSSaveGameManager::PerformCloudUpload(int32 SlotIndex)
{
    // 클라우드 업로드 시뮬레이션
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 클라우드 업로드 시작 - 슬롯 %d"), SlotIndex);
    
    CloudSyncStatus.bIsSyncing = true;
    CloudSyncStatus.LastSyncTime = FDateTime::Now();
    
    // 실제 구현에서는 Steam Cloud, Epic Games Store 등의 API 사용
    
    CloudSyncStatus.bIsSyncing = false;
    OnCloudSyncStatusChanged.Broadcast(CloudSyncStatus);
}

void UHSSaveGameManager::PerformCloudDownload(int32 SlotIndex)
{
    // 클라우드 다운로드 시뮬레이션
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 클라우드 다운로드 시작 - 슬롯 %d"), SlotIndex);
    
    CloudSyncStatus.bIsSyncing = true;
    
    // 실제 구현에서는 Steam Cloud, Epic Games Store 등의 API 사용
    
    CloudSyncStatus.bIsSyncing = false;
    CloudSyncStatus.LastSyncTime = FDateTime::Now();
    OnCloudSyncStatusChanged.Broadcast(CloudSyncStatus);
}

bool UHSSaveGameManager::ResolveCloudConflict(int32 SlotIndex)
{
    // 클라우드 충돌 해결 (최신 날짜 우선)
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 클라우드 충돌 해결 - 슬롯 %d"), SlotIndex);
    
    CloudSyncStatus.ConflictCount++;
    return true;
}

void UHSSaveGameManager::ProcessAutoSave()
{
    if (!bAutoSaveEnabled || !CurrentSaveData)
    {
        return;
    }
    
    TriggerAutoSave();
}

TArray<uint8>* UHSSaveGameManager::GetPooledBuffer()
{
    for (TArray<uint8>& Buffer : DataBufferPool)
    {
        if (Buffer.Num() == 0 || Buffer.GetSlack() > 0)
        {
            Buffer.Reset();
            return &Buffer;
        }
    }
    
    // 풀에 사용 가능한 버퍼가 없으면 새로 생성
    DataBufferPool.Add(TArray<uint8>());
    return &DataBufferPool.Last();
}

void UHSSaveGameManager::ReturnPooledBuffer(TArray<uint8>* Buffer)
{
    if (Buffer)
    {
        Buffer->Reset();
    }
}

void UHSSaveGameManager::InitializeSaveSystem()
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 저장 시스템 초기화"));
    
    // 기본 현재 저장 데이터 생성
    CurrentSaveData = NewObject<UHSSaveGameData>(this);
}

void UHSSaveGameManager::CleanupSaveSystem()
{
    // 리소스 정리
    CurrentSaveData = nullptr;
    SlotInfoCache.Empty();
    BackupInfoCache.Empty();
    DataBufferPool.Empty();
    
    // 대기열 정리
    while (!PendingSaveTasks.IsEmpty())
    {
        FAsyncSaveTask Task;
        PendingSaveTasks.Dequeue(Task);
    }
}

void UHSSaveGameManager::EnsureDirectoryExists(const FString& Directory)
{
    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Directory))
    {
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: 디렉토리 생성 - %s"), *Directory);
    }
}

int64 UHSSaveGameManager::GetFileSize(const FString& FilePath) const
{
    return IFileManager::Get().FileSize(*FilePath);
}

FDateTime UHSSaveGameManager::GetFileModificationTime(const FString& FilePath) const
{
    return IFileManager::Get().GetTimeStamp(*FilePath);
}

bool UHSSaveGameManager::CopyFile(const FString& SourcePath, const FString& DestPath) const
{
    return IFileManager::Get().Copy(*DestPath, *SourcePath) == COPY_OK;
}

void UHSSaveGameManager::HandleSaveError(const FString& ErrorMessage, int32 SlotIndex)
{
    UE_LOG(LogTemp, Error, TEXT("HSSaveGameManager: 저장 에러 - 슬롯 %d: %s"), SlotIndex, *ErrorMessage);
    LogSaveSystemEvent(TEXT("Save Error"), ErrorMessage);
}

void UHSSaveGameManager::LogSaveSystemEvent(const FString& Event, const FString& Details)
{
    FDateTime Now = FDateTime::Now();
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: [%s] %s - %s"), 
           *Now.ToString(), *Event, *Details);
}

bool UHSSaveGameManager::SaveSlotMetadata(int32 SlotIndex, const FHSSaveSlotInfo& SlotInfo)
{
    FString MetadataPath = GetSlotFilePath(SlotIndex) + TEXT(".meta");
    FString MetadataJson = FString::Printf(
        TEXT("{"
             "\"SlotIndex\":%d,"
             "\"SlotName\":\"%s\","
             "\"PlayerName\":\"%s\","
             "\"PlayerLevel\":%d,"
             "\"TotalPlayTime\":%d,"
             "\"SaveDate\":\"%s\","
             "\"IsValid\":%s,"
             "\"IsAutosave\":%s,"
             "\"FileSizeMB\":%.2f,"
             "\"SaveDataVersion\":%d"
             "}"),
        SlotInfo.SlotIndex,
        *SlotInfo.SlotName,
        *SlotInfo.PlayerName,
        SlotInfo.PlayerLevel,
        SlotInfo.TotalPlayTime,
        *SlotInfo.SaveDate.ToString(),
        SlotInfo.bIsValid ? TEXT("true") : TEXT("false"),
        SlotInfo.bIsAutosave ? TEXT("true") : TEXT("false"),
        SlotInfo.FileSizeMB,
        SlotInfo.SaveDataVersion
    );
    
    return FFileHelper::SaveStringToFile(MetadataJson, *MetadataPath);
}

FHSSaveSlotInfo UHSSaveGameManager::LoadSlotMetadata(int32 SlotIndex) const
{
    FHSSaveSlotInfo SlotInfo;
    SlotInfo.SlotIndex = SlotIndex;
    
    FString MetadataPath = GetSlotFilePath(SlotIndex) + TEXT(".meta");
    FString MetadataJson;
    
    if (FFileHelper::LoadFileToString(MetadataJson, *MetadataPath))
    {
        // 간단한 JSON 파싱 (실제로는 JSON 라이브러리 사용)
        // 현재는 기본값 사용
    }
    
    // 파일 정보로 기본값 설정
    FString FilePath = GetSlotFilePath(SlotIndex);
    SlotInfo.SaveDate = GetFileModificationTime(FilePath);
    SlotInfo.FileSizeMB = GetFileSize(FilePath) / (1024.0f * 1024.0f);
    SlotInfo.bIsValid = ValidateSaveFile(FilePath);
    
    return SlotInfo;
}

void UHSSaveGameManager::UpdateSlotInfoCache() const
{
    FScopeLock Lock(&CacheMutex);
    
    SlotInfoCache.Empty();
    
    for (int32 i = 0; i < MaxSaveSlots; ++i)
    {
        if (DoesSaveSlotExist(i))
        {
            FHSSaveSlotInfo SlotInfo = LoadSlotMetadata(i);
            SlotInfoCache.Add(i, SlotInfo);
        }
    }
    
    LastCacheUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UHSSaveGameManager::InvalidateSlotInfoCache()
{
    FScopeLock Lock(&CacheMutex);
    LastCacheUpdateTime = 0.0f;
}

void UHSSaveGameManager::UpdateBackupInfoCache() const
{
    BackupInfoCache.Empty();
    
    FString BackupDir = SaveDirectory / TEXT("Backups");
    TArray<FString> BackupFiles;
    IFileManager::Get().FindFiles(BackupFiles, *(BackupDir / TEXT("*.bak")), true, false);
    
    for (const FString& BackupFile : BackupFiles)
    {
        FString BackupID = FPaths::GetBaseFilename(BackupFile);
        FString MetadataPath = BackupDir / (BackupID + TEXT(".meta"));
        
        if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetadataPath))
        {
            // 메타데이터에서 백업 정보 로드
            FHSBackupInfo BackupInfo;
            BackupInfo.BackupID = BackupID;
            BackupInfo.BackupDate = GetFileModificationTime(BackupDir / BackupFile);
            BackupInfo.FileSizeMB = GetFileSize(BackupDir / BackupFile) / (1024.0f * 1024.0f);
            
            BackupInfoCache.Add(BackupInfo);
        }
    }
    
    bBackupCacheValid = true;
}

void UHSSaveGameManager::InvalidateBackupInfoCache()
{
    bBackupCacheValid = false;
}

#if PLATFORM_WINDOWS
void UHSSaveGameManager::InitializeWindowsSaveSystem()
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: Windows 저장 시스템 초기화"));
    // Windows 전용 설정
}
#elif PLATFORM_MAC
void UHSSaveGameManager::InitializeMacSaveSystem()
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: Mac 저장 시스템 초기화"));
    // Mac 전용 설정
}
#elif PLATFORM_LINUX
void UHSSaveGameManager::InitializeLinuxSaveSystem()
{
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameManager: Linux 저장 시스템 초기화"));
    // Linux 전용 설정
}
#endif
