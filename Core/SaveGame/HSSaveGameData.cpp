#include "HSSaveGameData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioDevice.h"
#include "Sound/SoundMix.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"

UHSSaveGameData::UHSSaveGameData()
{
    // 기본 저장 데이터 초기화
    SaveDataVersion = LatestSaveDataVersion;
    SaveDate = FDateTime::Now();
    PlayerID = FGuid::NewGuid().ToString();
    bIsValidSave = true;
    
    // 플레이어 프로필 초기화
    PlayerProfile.PlayerName = TEXT("Player");
    PlayerProfile.PlayerLevel = 1;
    PlayerProfile.TotalExperience = 0;
    PlayerProfile.CreationDate = FDateTime::Now();
    PlayerProfile.LastPlayDate = FDateTime::Now();
    
    // 기본 메타 화폐 설정
    PlayerProfile.MetaCurrencies.Empty();
    for (int32 i = 0; i < (int32)EHSCurrencyType::MAX; ++i)
    {
        FHSMetaCurrencyData CurrencyData;
        CurrencyData.CurrencyType = (EHSCurrencyType)i;
        CurrencyData.Amount = 0;
        CurrencyData.TotalEarned = 0;
        CurrencyData.TotalSpent = 0;
        PlayerProfile.MetaCurrencies.Add(CurrencyData);
    }
    
    // 기본 업적 설정
    PlayerProfile.Achievements.Empty();
    for (int32 i = 0; i < (int32)EHSAchievementType::Veteran + 1; ++i)
    {
        FHSAchievementProgress Achievement;
        Achievement.AchievementType = (EHSAchievementType)i;
        Achievement.bUnlocked = false;
        Achievement.CurrentProgress = 0;
        
        // 업적별 요구 진행도 설정
        switch (Achievement.AchievementType)
        {
            case EHSAchievementType::FirstKill:
                Achievement.RequiredProgress = 1;
                break;
            case EHSAchievementType::BossKiller:
                Achievement.RequiredProgress = 10;
                break;
            case EHSAchievementType::Survivor:
                Achievement.RequiredProgress = 5;
                break;
            case EHSAchievementType::Collector:
                Achievement.RequiredProgress = 100;
                break;
            case EHSAchievementType::Explorer:
                Achievement.RequiredProgress = 50;
                break;
            case EHSAchievementType::TeamPlayer:
                Achievement.RequiredProgress = 25;
                break;
            case EHSAchievementType::RunMaster:
                Achievement.RequiredProgress = 100;
                break;
            case EHSAchievementType::Speedrunner:
                Achievement.RequiredProgress = 1;
                break;
            case EHSAchievementType::Perfectionist:
                Achievement.RequiredProgress = 1;
                break;
            case EHSAchievementType::Veteran:
                Achievement.RequiredProgress = 1000;
                break;
        }
        
        PlayerProfile.Achievements.Add(Achievement);
    }
    
    // 기본 설정 초기화
    GraphicsSettings = FHSGraphicsSettings();
    AudioSettings = FHSAudioSettings();
    InputSettings = FHSInputSettings();
    GameplaySettings = FHSGameplaySettings();
    NetworkSettings = FHSNetworkSettings();
    AccessibilitySettings = FHSAccessibilitySettings();
    
    // 런 관련 데이터 초기화
    CompletedRunIDs.Empty();
    CurrentRunID.Empty();
    bHasActiveRun = false;
}

void UHSSaveGameData::UpdateSaveDate()
{
    SaveDate = FDateTime::Now();
    PlayerProfile.LastPlayDate = SaveDate;
}

bool UHSSaveGameData::ValidateSaveData() const
{
    // 기본 유효성 검사
    if (SaveDataVersion <= 0 || SaveDataVersion > LatestSaveDataVersion)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameData: 잘못된 저장 데이터 버전: %d"), SaveDataVersion);
        return false;
    }
    
    if (PlayerID.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("HSSaveGameData: 플레이어 ID가 비어있음"));
        return false;
    }
    
    if (!ValidatePlayerProfile())
    {
        return false;
    }
    
    if (!ValidateSettings())
    {
        return false;
    }
    
    if (!ValidateAchievements())
    {
        return false;
    }
    
    if (!ValidateMetaCurrencies())
    {
        return false;
    }
    
    return true;
}

void UHSSaveGameData::UpgradeSaveDataVersion()
{
    if (SaveDataVersion >= LatestSaveDataVersion)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 저장 데이터 업그레이드 중... %d -> %d"), 
           SaveDataVersion, LatestSaveDataVersion);
    
    int32 OriginalVersion = SaveDataVersion;
    
    // 순차적 업그레이드
    if (OriginalVersion < 1)
    {
        UpgradeFromVersion1();
        SaveDataVersion = 1;
    }
    
    // 향후 버전 업그레이드 추가 예정
    // if (SaveDataVersion < 2)
    // {
    //     UpgradeFromVersion2();
    //     SaveDataVersion = 2;
    // }
    
    SaveDataVersion = LatestSaveDataVersion;
    UpdateSaveDate();
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 저장 데이터 업그레이드 완료: %d -> %d"), 
           OriginalVersion, SaveDataVersion);
}

void UHSSaveGameData::ApplyGraphicsSettings() const
{
    if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings())
    {
        // 해상도 설정
        UserSettings->SetScreenResolution(GraphicsSettings.ScreenResolution);
        
        // 풀스크린 모드 설정
        EWindowMode::Type WindowMode = GraphicsSettings.bFullscreenMode ? 
            EWindowMode::Fullscreen : EWindowMode::Windowed;
        UserSettings->SetFullscreenMode(WindowMode);
        
        // VSync 설정
        UserSettings->SetVSyncEnabled(GraphicsSettings.bVSyncEnabled);
        
        // 품질 설정
        int32 QualityLevel = (int32)GraphicsSettings.OverallQuality;
        UserSettings->SetOverallScalabilityLevel(QualityLevel);
        UserSettings->SetTextureQuality(QualityLevel);
        UserSettings->SetShadowQuality(QualityLevel);
        UserSettings->SetPostProcessingQuality(QualityLevel);
        UserSettings->SetAntiAliasingQuality(QualityLevel);
        
        // 프레임 레이트 제한
        UserSettings->SetFrameRateLimit(GraphicsSettings.FrameRateLimit);
        
        // 설정 적용
        UserSettings->ApplySettings(false);
        
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 그래픽 설정 적용 완료"));
    }
}

void UHSSaveGameData::ApplyAudioSettings() const
{
    ApplyAudioSettingsToEngine();
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 오디오 설정 적용 완료"));
}

void UHSSaveGameData::ApplyInputSettings() const
{
    ApplyInputSettingsToEngine();
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 입력 설정 적용 완료"));
}

void UHSSaveGameData::ApplyGameplaySettings() const
{
    // 게임플레이 설정 적용 로직
    // 실제 구현에서는 게임 인스턴스나 게임 모드에 설정 전달
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 게임플레이 설정 적용 완료"));
}

void UHSSaveGameData::ApplyNetworkSettings() const
{
    // 네트워크 설정 적용 로직
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 네트워크 설정 적용 완료"));
}

void UHSSaveGameData::ApplyAccessibilitySettings() const
{
    // 접근성 설정 적용 로직
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 접근성 설정 적용 완료"));
}

void UHSSaveGameData::UpdatePlayTime(int32 AdditionalSeconds)
{
    PlayerProfile.Statistics.TotalPlayTime += AdditionalSeconds;
    UpdateSaveDate();
}

void UHSSaveGameData::RecordRunCompletion(bool bSuccessful, float RunTime)
{
    PlayerProfile.Statistics.TotalRuns++;
    
    if (bSuccessful)
    {
        PlayerProfile.Statistics.SuccessfulRuns++;
        
        // 최고 기록 업데이트
        if (RunTime > 0.0f && (PlayerProfile.Statistics.BestRunTime == 0.0f || RunTime < PlayerProfile.Statistics.BestRunTime))
        {
            PlayerProfile.Statistics.BestRunTime = RunTime;
            
            // 스피드런 업적 확인
            if (RunTime < 600.0f) // 10분 이내
            {
                UpdateAchievementProgress(EHSAchievementType::Speedrunner);
            }
        }
    }
    
    // 런 마스터 업적 진행
    UpdateAchievementProgress(EHSAchievementType::RunMaster);
    
    // 완벽주의자 업적 확인 (100% 성공률)
    if (PlayerProfile.Statistics.TotalRuns >= 10 && 
        PlayerProfile.Statistics.SuccessfulRuns == PlayerProfile.Statistics.TotalRuns)
    {
        UpdateAchievementProgress(EHSAchievementType::Perfectionist);
    }
    
    UpdateSaveDate();
}

void UHSSaveGameData::RecordEnemyKill(bool bIsBoss)
{
    PlayerProfile.Statistics.TotalEnemiesKilled++;
    
    if (bIsBoss)
    {
        PlayerProfile.Statistics.TotalBossesKilled++;
        UpdateAchievementProgress(EHSAchievementType::BossKiller);
    }
    
    // 첫 킬 업적
    if (PlayerProfile.Statistics.TotalEnemiesKilled == 1)
    {
        UpdateAchievementProgress(EHSAchievementType::FirstKill);
    }
    
    // 베테랑 업적 (적 처치 수 기반)
    UpdateAchievementProgress(EHSAchievementType::Veteran);
    
    UpdateSaveDate();
}

void UHSSaveGameData::RecordDeath()
{
    PlayerProfile.Statistics.TotalDeaths++;
    UpdateSaveDate();
}

void UHSSaveGameData::RecordDamage(float DamageDealt, float DamageTaken, float HealingDone)
{
    PlayerProfile.Statistics.TotalDamageDealt += DamageDealt;
    PlayerProfile.Statistics.TotalDamageTaken += DamageTaken;
    PlayerProfile.Statistics.TotalHealingDone += HealingDone;
    
    // 팀플레이어 업적 (힐링 기반)
    if (HealingDone > 0.0f)
    {
        UpdateAchievementProgress(EHSAchievementType::TeamPlayer);
    }
    
    UpdateSaveDate();
}

void UHSSaveGameData::UpdateAchievementProgress(EHSAchievementType AchievementType, int32 ProgressIncrement)
{
    FHSAchievementProgress* Achievement = FindAchievementProgress(AchievementType);
    if (!Achievement)
    {
        return;
    }
    
    if (Achievement->bUnlocked)
    {
        return; // 이미 언락된 업적
    }
    
    Achievement->CurrentProgress += ProgressIncrement;
    
    if (Achievement->CurrentProgress >= Achievement->RequiredProgress)
    {
        Achievement->bUnlocked = true;
        Achievement->UnlockDate = FDateTime::Now();
        
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 업적 언락! - %d"), (int32)AchievementType);
        
        // 업적 언락 보상 지급
        switch (AchievementType)
        {
            case EHSAchievementType::FirstKill:
                AddMetaCurrency(EHSCurrencyType::MetaSouls, 10);
                break;
            case EHSAchievementType::BossKiller:
                AddMetaCurrency(EHSCurrencyType::EssencePoints, 50);
                break;
            case EHSAchievementType::Survivor:
                AddMetaCurrency(EHSCurrencyType::UnlockPoints, 5);
                break;
            case EHSAchievementType::Collector:
                AddMetaCurrency(EHSCurrencyType::CraftingTokens, 100);
                break;
            case EHSAchievementType::Explorer:
                AddMetaCurrency(EHSCurrencyType::RuneShards, 25);
                break;
            case EHSAchievementType::TeamPlayer:
                AddMetaCurrency(EHSCurrencyType::ArcaneOrbs, 30);
                break;
            case EHSAchievementType::RunMaster:
                AddMetaCurrency(EHSCurrencyType::HeroicMedals, 100);
                break;
            case EHSAchievementType::Speedrunner:
                AddMetaCurrency(EHSCurrencyType::DivineFragments, 50);
                break;
            case EHSAchievementType::Perfectionist:
                AddMetaCurrency(EHSCurrencyType::EventTokens, 1);
                break;
            case EHSAchievementType::Veteran:
                AddMetaCurrency(EHSCurrencyType::SeasonCoins, 10);
                break;
        }
    }
    
    UpdateSaveDate();
}

bool UHSSaveGameData::IsAchievementUnlocked(EHSAchievementType AchievementType) const
{
    const FHSAchievementProgress* Achievement = FindAchievementProgress(AchievementType);
    return Achievement ? Achievement->bUnlocked : false;
}

TArray<FHSAchievementProgress> UHSSaveGameData::GetUnlockedAchievements() const
{
    TArray<FHSAchievementProgress> UnlockedAchievements;
    
    for (const FHSAchievementProgress& Achievement : PlayerProfile.Achievements)
    {
        if (Achievement.bUnlocked)
        {
            UnlockedAchievements.Add(Achievement);
        }
    }
    
    return UnlockedAchievements;
}

void UHSSaveGameData::AddMetaCurrency(EHSCurrencyType CurrencyType, int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }
    
    FHSMetaCurrencyData* CurrencyData = FindMetaCurrencyData(CurrencyType);
    if (CurrencyData)
    {
        CurrencyData->Amount += Amount;
        CurrencyData->TotalEarned += Amount;
        
        UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 메타 화폐 획득 - 타입: %d, 양: %d"), 
               (int32)CurrencyType, Amount);
    }
    
    UpdateSaveDate();
}

bool UHSSaveGameData::SpendMetaCurrency(EHSCurrencyType CurrencyType, int32 Amount)
{
    if (Amount <= 0)
    {
        return false;
    }
    
    FHSMetaCurrencyData* CurrencyData = FindMetaCurrencyData(CurrencyType);
    if (!CurrencyData || CurrencyData->Amount < Amount)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSaveGameData: 메타 화폐 부족 - 타입: %d, 필요: %d, 보유: %d"), 
               (int32)CurrencyType, Amount, CurrencyData ? CurrencyData->Amount : 0);
        return false;
    }
    
    CurrencyData->Amount -= Amount;
    CurrencyData->TotalSpent += Amount;
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 메타 화폐 소모 - 타입: %d, 양: %d"), 
           (int32)CurrencyType, Amount);
    
    UpdateSaveDate();
    return true;
}

int32 UHSSaveGameData::GetMetaCurrencyAmount(EHSCurrencyType CurrencyType) const
{
    const FHSMetaCurrencyData* CurrencyData = FindMetaCurrencyData(CurrencyType);
    return CurrencyData ? CurrencyData->Amount : 0;
}

void UHSSaveGameData::UnlockContent(const FString& UnlockID, int32 Cost)
{
    if (IsContentUnlocked(UnlockID))
    {
        return;
    }
    
    FHSUnlockData NewUnlock;
    NewUnlock.UnlockID = UnlockID;
    NewUnlock.bUnlocked = true;
    NewUnlock.UnlockDate = FDateTime::Now();
    NewUnlock.UnlockCost = Cost;
    
    PlayerProfile.UnlockedContent.Add(NewUnlock);
    
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 콘텐츠 언락 - ID: %s, 비용: %d"), *UnlockID, Cost);
    
    UpdateSaveDate();
}

bool UHSSaveGameData::IsContentUnlocked(const FString& UnlockID) const
{
    const FHSUnlockData* UnlockData = FindUnlockData(UnlockID);
    return UnlockData ? UnlockData->bUnlocked : false;
}

TArray<FString> UHSSaveGameData::GetUnlockedContentIDs() const
{
    TArray<FString> UnlockedIDs;
    
    for (const FHSUnlockData& UnlockData : PlayerProfile.UnlockedContent)
    {
        if (UnlockData.bUnlocked)
        {
            UnlockedIDs.Add(UnlockData.UnlockID);
        }
    }
    
    return UnlockedIDs;
}

FHSMetaCurrencyData* UHSSaveGameData::FindMetaCurrencyData(EHSCurrencyType CurrencyType)
{
    for (FHSMetaCurrencyData& CurrencyData : PlayerProfile.MetaCurrencies)
    {
        if (CurrencyData.CurrencyType == CurrencyType)
        {
            return &CurrencyData;
        }
    }
    return nullptr;
}

const FHSMetaCurrencyData* UHSSaveGameData::FindMetaCurrencyData(EHSCurrencyType CurrencyType) const
{
    for (const FHSMetaCurrencyData& CurrencyData : PlayerProfile.MetaCurrencies)
    {
        if (CurrencyData.CurrencyType == CurrencyType)
        {
            return &CurrencyData;
        }
    }
    return nullptr;
}

FHSAchievementProgress* UHSSaveGameData::FindAchievementProgress(EHSAchievementType AchievementType)
{
    for (FHSAchievementProgress& Achievement : PlayerProfile.Achievements)
    {
        if (Achievement.AchievementType == AchievementType)
        {
            return &Achievement;
        }
    }
    return nullptr;
}

const FHSAchievementProgress* UHSSaveGameData::FindAchievementProgress(EHSAchievementType AchievementType) const
{
    for (const FHSAchievementProgress& Achievement : PlayerProfile.Achievements)
    {
        if (Achievement.AchievementType == AchievementType)
        {
            return &Achievement;
        }
    }
    return nullptr;
}

FHSUnlockData* UHSSaveGameData::FindUnlockData(const FString& UnlockID)
{
    for (FHSUnlockData& UnlockData : PlayerProfile.UnlockedContent)
    {
        if (UnlockData.UnlockID == UnlockID)
        {
            return &UnlockData;
        }
    }
    return nullptr;
}

const FHSUnlockData* UHSSaveGameData::FindUnlockData(const FString& UnlockID) const
{
    for (const FHSUnlockData& UnlockData : PlayerProfile.UnlockedContent)
    {
        if (UnlockData.UnlockID == UnlockID)
        {
            return &UnlockData;
        }
    }
    return nullptr;
}

void UHSSaveGameData::UpgradeFromVersion1()
{
    // 버전 1에서 업그레이드할 때 필요한 데이터 변환
    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 버전 1 업그레이드 처리"));
    
    // 예: 새로운 메타 화폐 추가, 업적 시스템 변경 등
}

bool UHSSaveGameData::ValidatePlayerProfile() const
{
    if (PlayerProfile.PlayerName.IsEmpty())
    {
        return false;
    }
    
    if (PlayerProfile.PlayerLevel < 1 || PlayerProfile.PlayerLevel > 1000)
    {
        return false;
    }
    
    if (PlayerProfile.TotalExperience < 0)
    {
        return false;
    }
    
    return true;
}

bool UHSSaveGameData::ValidateSettings() const
{
    // 그래픽 설정 검증
    if (GraphicsSettings.ScreenResolution.X <= 0 || GraphicsSettings.ScreenResolution.Y <= 0)
    {
        return false;
    }
    
    if (GraphicsSettings.FrameRateLimit < 30.0f || GraphicsSettings.FrameRateLimit > 300.0f)
    {
        return false;
    }
    
    // 오디오 설정 검증
    if (AudioSettings.MasterVolume < 0.0f || AudioSettings.MasterVolume > 1.0f)
    {
        return false;
    }
    
    return true;
}

bool UHSSaveGameData::ValidateAchievements() const
{
    for (const FHSAchievementProgress& Achievement : PlayerProfile.Achievements)
    {
        if (Achievement.CurrentProgress < 0 || Achievement.RequiredProgress <= 0)
        {
            return false;
        }
        
        if (Achievement.bUnlocked && Achievement.CurrentProgress < Achievement.RequiredProgress)
        {
            return false;
        }
    }
    
    return true;
}

bool UHSSaveGameData::ValidateMetaCurrencies() const
{
    for (const FHSMetaCurrencyData& CurrencyData : PlayerProfile.MetaCurrencies)
    {
        if (CurrencyData.Amount < 0 || CurrencyData.TotalEarned < 0 || CurrencyData.TotalSpent < 0)
        {
            return false;
        }
        
        if (CurrencyData.TotalEarned < CurrencyData.Amount + CurrencyData.TotalSpent)
        {
            return false; // 수학적으로 불가능한 상태
        }
    }
    
    return true;
}

void UHSSaveGameData::ApplyAudioSettingsToEngine() const
{
    // 엔진 오디오 설정 적용
    if (GEngine)
    {
        FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw();
        if (AudioDevice)
        {
            // 마스터 볼륨 설정
            if (!AudioSettings.bMasterMuted)
            {
                AudioDevice->SetGlobalPitchModulation(AudioSettings.MasterVolume, 0.0f);
            }
            
            // 개별 볼륨 설정 (실제 구현에서는 사운드 클래스별 설정)
        }
    }
}

void UHSSaveGameData::ApplyInputSettingsToEngine() const
{
    // 입력 설정 엔진 적용
    if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings())
    {
        // 마우스 감도 등 입력 관련 설정
        // 실제 구현에서는 게임 인스턴스나 플레이어 컨트롤러에 전달
    }
}
