#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "HSSaveGameData.h"
#include "HSSaveGameManager.generated.h"

UENUM(BlueprintType)
enum class EHSSaveOperation : uint8
{
    Save,
    Load,
    Delete,
    Backup,
    Restore
};

UENUM(BlueprintType)
enum class EHSSaveResult : uint8
{
    Success,
    Failed,
    InProgress,
    NotFound,
    Corrupted,
    AccessDenied,
    DiskFull
};

USTRUCT(BlueprintType)
struct FHSSaveSlotInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    int32 SlotIndex = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    FString SlotName;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    int32 PlayerLevel = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    int32 TotalPlayTime = 0; // 초 단위

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    FDateTime SaveDate;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    bool bIsValid = true;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    bool bIsAutosave = false;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    float FileSizeMB = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Save Slot")
    int32 SaveDataVersion = 1;

    FHSSaveSlotInfo()
    {
        SlotIndex = 0;
        PlayerLevel = 1;
        TotalPlayTime = 0;
        SaveDate = FDateTime::Now();
        bIsValid = true;
        bIsAutosave = false;
        FileSizeMB = 0.0f;
        SaveDataVersion = 1;
    }
};

USTRUCT(BlueprintType)
struct FHSSaveOperationProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Save Progress")
    EHSSaveOperation Operation = EHSSaveOperation::Save;

    UPROPERTY(BlueprintReadWrite, Category = "Save Progress")
    float ProgressPercent = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Save Progress")
    FString CurrentStep;

    UPROPERTY(BlueprintReadWrite, Category = "Save Progress")
    float ElapsedTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Save Progress")
    float EstimatedRemainingTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Save Progress")
    bool bIsCompleted = false;

    FHSSaveOperationProgress()
    {
        Operation = EHSSaveOperation::Save;
        ProgressPercent = 0.0f;
        ElapsedTime = 0.0f;
        EstimatedRemainingTime = 0.0f;
        bIsCompleted = false;
    }
};

USTRUCT(BlueprintType)
struct FHSBackupInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    FString BackupID;

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    int32 OriginalSlotIndex = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    FDateTime BackupDate;

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    FString BackupReason;

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    float FileSizeMB = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    bool bIsCompressed = false;

    UPROPERTY(BlueprintReadWrite, Category = "Backup")
    bool bIsEncrypted = false;

    FHSBackupInfo()
    {
        OriginalSlotIndex = 0;
        BackupDate = FDateTime::Now();
        FileSizeMB = 0.0f;
        bIsCompressed = false;
        bIsEncrypted = false;
    }
};

USTRUCT(BlueprintType)
struct FHSCloudSyncStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Cloud Sync")
    bool bIsEnabled = false;

    UPROPERTY(BlueprintReadWrite, Category = "Cloud Sync")
    bool bIsSyncing = false;

    UPROPERTY(BlueprintReadWrite, Category = "Cloud Sync")
    FDateTime LastSyncTime;

    UPROPERTY(BlueprintReadWrite, Category = "Cloud Sync")
    FString CloudProvider;

    UPROPERTY(BlueprintReadWrite, Category = "Cloud Sync")
    FString LastError;

    UPROPERTY(BlueprintReadWrite, Category = "Cloud Sync")
    int32 ConflictCount = 0;

    FHSCloudSyncStatus()
    {
        bIsEnabled = false;
        bIsSyncing = false;
        LastSyncTime = FDateTime::MinValue();
        ConflictCount = 0;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveOperationCompleted, EHSSaveResult, Result, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveOperationProgress, const FHSSaveOperationProgress&, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAutoSaveTriggered, int32, SlotIndex, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveIntegrityCheckCompleted, bool, bAllValid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCloudSyncStatusChanged, const FHSCloudSyncStatus&, Status);

/**
 * 저장/로드 시스템 관리자 - 비동기 저장/로드, 자동 저장, 백업 시스템
 * 현업 최적화: 압축, 암호화, 클라우드 동기화, 무결성 검증
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSSaveGameManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSSaveGameManager();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 기본 저장/로드 ===
    UFUNCTION(BlueprintCallable, Category = "Save System", CallInEditor)
    void SaveGameAsync(int32 SlotIndex, UHSSaveGameData* SaveData);

    UFUNCTION(BlueprintCallable, Category = "Save System", CallInEditor)
    void LoadGameAsync(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool SaveGameSync(int32 SlotIndex, UHSSaveGameData* SaveData);

    UFUNCTION(BlueprintCallable, Category = "Save System")
    UHSSaveGameData* LoadGameSync(int32 SlotIndex);

    // === 슬롯 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool DeleteSaveSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool DoesSaveSlotExist(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Save System")
    TArray<FHSSaveSlotInfo> GetAllSaveSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Save System")
    FHSSaveSlotInfo GetSaveSlotInfo(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Save System")
    int32 FindEmptySlot() const;

    // === 자동 저장 ===
    UFUNCTION(BlueprintCallable, Category = "Auto Save")
    void EnableAutoSave(bool bEnabled, float IntervalSeconds = 300.0f);

    UFUNCTION(BlueprintCallable, Category = "Auto Save")
    void SetAutoSaveSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Auto Save")
    void TriggerAutoSave();

    UFUNCTION(BlueprintCallable, Category = "Auto Save")
    bool IsAutoSaveEnabled() const { return bAutoSaveEnabled; }

    UFUNCTION(BlueprintCallable, Category = "Auto Save")
    float GetAutoSaveInterval() const { return AutoSaveInterval; }

    // === 백업 시스템 ===
    UFUNCTION(BlueprintCallable, Category = "Backup System")
    bool CreateBackup(int32 SlotIndex, const FString& Reason = TEXT("Manual Backup"));

    UFUNCTION(BlueprintCallable, Category = "Backup System")
    bool RestoreFromBackup(const FString& BackupID, int32 TargetSlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Backup System")
    TArray<FHSBackupInfo> GetAvailableBackups(int32 SlotIndex = -1) const;

    UFUNCTION(BlueprintCallable, Category = "Backup System")
    bool DeleteBackup(const FString& BackupID);

    UFUNCTION(BlueprintCallable, Category = "Backup System")
    void CleanupOldBackups(int32 MaxBackupsToKeep = 10);

    // === 무결성 검증 ===
    UFUNCTION(BlueprintCallable, Category = "Integrity")
    void VerifyAllSaveIntegrity();

    UFUNCTION(BlueprintCallable, Category = "Integrity")
    bool VerifySaveIntegrity(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Integrity")
    bool RepairCorruptedSave(int32 SlotIndex);

    // === 압축 및 암호화 ===
    UFUNCTION(BlueprintCallable, Category = "Compression")
    void EnableCompression(bool bEnabled) { bCompressionEnabled = bEnabled; }

    UFUNCTION(BlueprintCallable, Category = "Encryption")
    void EnableEncryption(bool bEnabled, const FString& EncryptionKey = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Compression")
    bool IsCompressionEnabled() const { return bCompressionEnabled; }

    UFUNCTION(BlueprintCallable, Category = "Encryption")
    bool IsEncryptionEnabled() const { return bEncryptionEnabled; }

    // === 클라우드 동기화 ===
    UFUNCTION(BlueprintCallable, Category = "Cloud Sync")
    void EnableCloudSync(bool bEnabled, const FString& Provider = TEXT("Steam"));

    UFUNCTION(BlueprintCallable, Category = "Cloud Sync")
    void SyncToCloud(int32 SlotIndex = -1);

    UFUNCTION(BlueprintCallable, Category = "Cloud Sync")
    void SyncFromCloud(int32 SlotIndex = -1);

    UFUNCTION(BlueprintCallable, Category = "Cloud Sync")
    FHSCloudSyncStatus GetCloudSyncStatus() const { return CloudSyncStatus; }

    // === 진행 상황 조회 ===
    UFUNCTION(BlueprintPure, Category = "Save System")
    bool IsOperationInProgress() const { return bOperationInProgress; }

    UFUNCTION(BlueprintPure, Category = "Save System")
    FHSSaveOperationProgress GetCurrentOperationProgress() const { return CurrentOperationProgress; }

    UFUNCTION(BlueprintPure, Category = "Save System")
    UHSSaveGameData* GetCurrentSaveData() const { return CurrentSaveData; }

    // === 설정 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetMaxSaveSlots(int32 MaxSlots) { MaxSaveSlots = FMath::Clamp(MaxSlots, 1, 100); }

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetSaveDirectory(const FString& Directory);

    UFUNCTION(BlueprintPure, Category = "Settings")
    int32 GetMaxSaveSlots() const { return MaxSaveSlots; }

    UFUNCTION(BlueprintPure, Category = "Settings")
    FString GetSaveDirectory() const { return SaveDirectory; }

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Save Events")
    FOnSaveOperationCompleted OnSaveOperationCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Save Events")
    FOnSaveOperationProgress OnSaveOperationProgress;

    UPROPERTY(BlueprintAssignable, Category = "Auto Save Events")
    FOnAutoSaveTriggered OnAutoSaveTriggered;

    UPROPERTY(BlueprintAssignable, Category = "Integrity Events")
    FOnSaveIntegrityCheckCompleted OnSaveIntegrityCheckCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Cloud Sync Events")
    FOnCloudSyncStatusChanged OnCloudSyncStatusChanged;

protected:
    // === 내부 상태 ===
    UPROPERTY()
    TObjectPtr<UHSSaveGameData> CurrentSaveData;

    UPROPERTY()
    bool bOperationInProgress;

    UPROPERTY()
    FHSSaveOperationProgress CurrentOperationProgress;

    // === 자동 저장 설정 ===
    UPROPERTY()
    bool bAutoSaveEnabled;

    UPROPERTY()
    float AutoSaveInterval;

    UPROPERTY()
    int32 AutoSaveSlotIndex;

    // === 시스템 설정 ===
    UPROPERTY()
    int32 MaxSaveSlots;

    UPROPERTY()
    FString SaveDirectory;

    UPROPERTY()
    bool bCompressionEnabled;

    UPROPERTY()
    bool bEncryptionEnabled;

    UPROPERTY()
    FString EncryptionKey;

    // === 클라우드 동기화 ===
    UPROPERTY()
    FHSCloudSyncStatus CloudSyncStatus;

    // === 내부 함수 ===
    void PerformSaveOperation(int32 SlotIndex, UHSSaveGameData* SaveData);
    void PerformLoadOperation(int32 SlotIndex);
    
    void UpdateOperationProgress(float Progress, const FString& Step);
    void CompleteOperation(EHSSaveResult Result, int32 SlotIndex);
    
    // 파일 I/O
    bool WriteToFile(const FString& FilePath, const TArray<uint8>& Data);
    bool ReadFromFile(const FString& FilePath, TArray<uint8>& OutData);
    
    // 압축 및 암호화
    TArray<uint8> CompressData(const TArray<uint8>& Data) const;
    TArray<uint8> DecompressData(const TArray<uint8>& CompressedData) const;
    TArray<uint8> EncryptData(const TArray<uint8>& Data) const;
    TArray<uint8> DecryptData(const TArray<uint8>& EncryptedData) const;
    
    // 슬롯 관리
    FString GetSlotFilePath(int32 SlotIndex) const;
    FString GetBackupFilePath(const FString& BackupID) const;
    
    // 백업 관리
    FString GenerateBackupID() const;
    void CreateAutomaticBackup(int32 SlotIndex, const FString& Reason);
    
    // 무결성 검증
    bool ValidateSaveFile(const FString& FilePath) const;
    uint32 CalculateChecksum(const TArray<uint8>& Data) const;
    
    // 클라우드 동기화 내부
    void PerformCloudUpload(int32 SlotIndex);
    void PerformCloudDownload(int32 SlotIndex);
    bool ResolveCloudConflict(int32 SlotIndex);
    
    // 자동 저장 타이머
    FTimerHandle AutoSaveTimerHandle;
    void ProcessAutoSave();
    
    // 슬롯 정보 캐시 (성능 최적화)
    mutable TMap<int32, FHSSaveSlotInfo> SlotInfoCache;
    mutable float LastCacheUpdateTime;
    
    // 백업 정보 캐시
    mutable TArray<FHSBackupInfo> BackupInfoCache;
    mutable bool bBackupCacheValid;
    
    // 비동기 작업 관리
    struct FAsyncSaveTask
    {
        int32 SlotIndex;
        TObjectPtr<UHSSaveGameData> SaveData;
        float StartTime;
        
        FAsyncSaveTask() : SlotIndex(0), SaveData(nullptr), StartTime(0.0f) {}
        FAsyncSaveTask(int32 InSlotIndex, UHSSaveGameData* InSaveData, float InStartTime)
            : SlotIndex(InSlotIndex), SaveData(InSaveData), StartTime(InStartTime) {}
    };
    
    TQueue<FAsyncSaveTask> PendingSaveTasks;
    bool bAsyncTaskInProgress;
    
    // 오브젝트 풀링 (성능 최적화)
    TArray<TArray<uint8>> DataBufferPool;
    TArray<uint8>* GetPooledBuffer();
    void ReturnPooledBuffer(TArray<uint8>* Buffer);
    
    // 상수
    static constexpr int32 DefaultMaxSaveSlots = 10;
    static constexpr float DefaultAutoSaveInterval = 300.0f; // 5분
    static constexpr int32 MaxBackupsPerSlot = 5;
    static constexpr float CacheValidityDuration = 10.0f; // 10초
    static constexpr int32 DataBufferPoolSize = 5;

private:
    // 내부 유틸리티
    void InitializeSaveSystem();
    void CleanupSaveSystem();
    void EnsureDirectoryExists(const FString& Directory);
    
    // 파일 시스템 헬퍼
    int64 GetFileSize(const FString& FilePath) const;
    FDateTime GetFileModificationTime(const FString& FilePath) const;
    bool CopyFile(const FString& SourcePath, const FString& DestPath) const;
    
    // 에러 처리
    void HandleSaveError(const FString& ErrorMessage, int32 SlotIndex);
    void LogSaveSystemEvent(const FString& Event, const FString& Details = TEXT(""));
    
    // 메타데이터 관리
    bool SaveSlotMetadata(int32 SlotIndex, const FHSSaveSlotInfo& SlotInfo);
    FHSSaveSlotInfo LoadSlotMetadata(int32 SlotIndex) const;
    
    // 캐시 관리
    void UpdateSlotInfoCache() const;
    void InvalidateSlotInfoCache();
    void UpdateBackupInfoCache() const;
    void InvalidateBackupInfoCache();
    
    // 플랫폼별 구현
#if PLATFORM_WINDOWS
    void InitializeWindowsSaveSystem();
#elif PLATFORM_MAC
    void InitializeMacSaveSystem();
#elif PLATFORM_LINUX
    void InitializeLinuxSaveSystem();
#endif
    
    // 스레드 세이프 처리
    mutable FCriticalSection SaveSystemMutex;
    mutable FCriticalSection CacheMutex;
};
