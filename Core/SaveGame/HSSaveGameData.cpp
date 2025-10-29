#include "HSSaveGameData.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HSSaveGameManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"

namespace
{
    UWorld* ResolvePrimaryWorld()
    {
        if (!GEngine)
        {
            return nullptr;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World)
            {
                continue;
            }

            if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
            {
                return World;
            }
        }

        return GEngine->GetWorldContexts().Num() > 0 ? GEngine->GetWorldContexts()[0].World() : nullptr;
    }

    UGameInstance* ResolvePrimaryGameInstance()
    {
        if (UWorld* World = ResolvePrimaryWorld())
        {
            return World->GetGameInstance();
        }
        return nullptr;
    }

    IConsoleVariable* ResolveRuntimeCVar(const TCHAR* Name, float DefaultValue)
    {
        static TMap<FString, IConsoleVariable*> CachedCVars;
        if (IConsoleVariable** Found = CachedCVars.Find(FString(Name)))
        {
            return *Found;
        }

        IConsoleManager& ConsoleManager = IConsoleManager::Get();
        IConsoleVariable* Variable = ConsoleManager.FindConsoleVariable(Name);
        if (!Variable)
        {
            Variable = ConsoleManager.RegisterConsoleVariable(Name, DefaultValue, TEXT("Managed by HuntingSpirit save data runtime settings"), ECVF_Default);
        }

        CachedCVars.Add(FString(Name), Variable);
        return Variable;
    }

    void SetRuntimeCVar(const TCHAR* Name, float Value)
    {
        if (IConsoleVariable* Variable = ResolveRuntimeCVar(Name, Value))
        {
            Variable->Set(Value, ECVF_SetByGameSetting);
        }
    }

    void SetRuntimeCVarBool(const TCHAR* Name, bool bValue)
    {
        SetRuntimeCVar(Name, bValue ? 1.0f : 0.0f);
    }
}

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
    const float ClampedScale = FMath::Clamp(GameplaySettings.UIScale, 0.5f, 3.0f);
    
    if (UGameInstance* GameInstance = ResolvePrimaryGameInstance())
    {
        if (UHSSaveGameManager* SaveManager = GameInstance->GetSubsystem<UHSSaveGameManager>())
        {
            SaveManager->EnableAutoSave(GameplaySettings.bAutoSaveEnabled, GameplaySettings.AutoSaveInterval);
        }
    }
    
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetApplicationScale(ClampedScale);
    }
    
    SetRuntimeCVar(TEXT("hs.UI.Scale"), ClampedScale);
    SetRuntimeCVarBool(TEXT("hs.Gameplay.DamageNumbers"), GameplaySettings.bDamageNumbersEnabled);
    SetRuntimeCVarBool(TEXT("hs.Gameplay.HealthBars"), GameplaySettings.bHealthBarsEnabled);
    SetRuntimeCVarBool(TEXT("hs.Gameplay.Crosshair"), GameplaySettings.bCrosshairEnabled);
    SetRuntimeCVarBool(TEXT("hs.Gameplay.Subtitles"), GameplaySettings.bSubtitlesEnabled);
    SetRuntimeCVar(TEXT("hs.Gameplay.PreferredDifficulty"), static_cast<float>(GameplaySettings.PreferredDifficulty));

    if (GConfig)
    {
        const TCHAR* Section = TEXT("HuntingSpirit.Gameplay");
        GConfig->SetInt(Section, TEXT("PreferredDifficulty"), static_cast<int32>(GameplaySettings.PreferredDifficulty), GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bAutoSaveEnabled"), GameplaySettings.bAutoSaveEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("AutoSaveInterval"), GameplaySettings.AutoSaveInterval, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bDamageNumbersEnabled"), GameplaySettings.bDamageNumbersEnabled, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bHealthBarsEnabled"), GameplaySettings.bHealthBarsEnabled, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bCrosshairEnabled"), GameplaySettings.bCrosshairEnabled, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bSubtitlesEnabled"), GameplaySettings.bSubtitlesEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("UIScale"), GameplaySettings.UIScale, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings())
    {
        UserSettings->ApplySettings(false);
    }

    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 게임플레이 설정 적용 완료"));
}

void UHSSaveGameData::ApplyNetworkSettings() const
{
    SetRuntimeCVar(TEXT("hs.Network.MaxPingThreshold"), static_cast<float>(NetworkSettings.MaxPingThreshold));
    SetRuntimeCVarBool(TEXT("hs.Network.ShowPing"), NetworkSettings.bShowPing);
    SetRuntimeCVarBool(TEXT("hs.Network.AutoReconnect"), NetworkSettings.bAutoConnectToLastServer);
    SetRuntimeCVarBool(TEXT("hs.Network.AllowCrossPlay"), NetworkSettings.bAllowCrossPlatformPlay);

    if (IConsoleVariable* MaxPredictionPing = IConsoleManager::Get().FindConsoleVariable(TEXT("net.MaxPredictionPing")))
    {
        MaxPredictionPing->Set(NetworkSettings.MaxPingThreshold, ECVF_SetByGameSetting);
    }

    if (IConsoleVariable* ShowPing = IConsoleManager::Get().FindConsoleVariable(TEXT("net.DisplayPing")))
    {
        ShowPing->Set(NetworkSettings.bShowPing ? 1 : 0, ECVF_SetByGameSetting);
    }

    if (GConfig)
    {
        const TCHAR* Section = TEXT("HuntingSpirit.Network");
        GConfig->SetInt(Section, TEXT("MaxPingThreshold"), NetworkSettings.MaxPingThreshold, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bShowPing"), NetworkSettings.bShowPing, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bAutoConnectToLastServer"), NetworkSettings.bAutoConnectToLastServer, GGameUserSettingsIni);
        GConfig->SetString(Section, TEXT("LastServerAddress"), *NetworkSettings.LastServerAddress, GGameUserSettingsIni);
        GConfig->SetString(Section, TEXT("PreferredRegion"), *NetworkSettings.PreferredRegion, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bAllowCrossPlatformPlay"), NetworkSettings.bAllowCrossPlatformPlay, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    UE_LOG(LogTemp, Log, TEXT("HSSaveGameData: 네트워크 설정 적용 완료"));
}

void UHSSaveGameData::ApplyAccessibilitySettings() const
{
    SetRuntimeCVarBool(TEXT("hs.Accessibility.ColorBlindMode"), AccessibilitySettings.bColorBlindMode);
    SetRuntimeCVar(TEXT("hs.Accessibility.TextScale"), AccessibilitySettings.TextSize);
    SetRuntimeCVarBool(TEXT("hs.Accessibility.HighContrastMode"), AccessibilitySettings.bHighContrastMode);
    SetRuntimeCVarBool(TEXT("hs.Accessibility.ReduceMotion"), AccessibilitySettings.bReduceMotion);
    SetRuntimeCVarBool(TEXT("hs.Accessibility.ScreenReader"), AccessibilitySettings.bScreenReaderSupport);

    if (FSlateApplication::IsInitialized())
    {
        const float ClampedScale = FMath::Clamp(AccessibilitySettings.TextSize, 0.5f, 2.5f);
        FSlateApplication::Get().SetApplicationScale(ClampedScale);
    }

    if (GConfig)
    {
        const TCHAR* Section = TEXT("HuntingSpirit.Accessibility");
        GConfig->SetBool(Section, TEXT("bColorBlindMode"), AccessibilitySettings.bColorBlindMode, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("TextSize"), AccessibilitySettings.TextSize, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bHighContrastMode"), AccessibilitySettings.bHighContrastMode, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bReduceMotion"), AccessibilitySettings.bReduceMotion, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bScreenReaderSupport"), AccessibilitySettings.bScreenReaderSupport, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

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
    const float MasterVolume = AudioSettings.bMasterMuted ? 0.0f : AudioSettings.MasterVolume;
    const float SfxVolume = AudioSettings.bSFXMuted ? 0.0f : AudioSettings.SFXVolume;
    const float MusicVolume = AudioSettings.bMusicMuted ? 0.0f : AudioSettings.MusicVolume;
    const float VoiceVolume = AudioSettings.bVoiceMuted ? 0.0f : AudioSettings.VoiceVolume;
    const float AmbientVolume = AudioSettings.bAmbientMuted ? 0.0f : AudioSettings.AmbientVolume;

    if (GEngine)
    {
        if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw())
        {
            AudioDevice->SetTransientPrimaryVolume(MasterVolume);
        }
    }

    SetRuntimeCVar(TEXT("hs.Audio.MasterVolume"), MasterVolume);
    SetRuntimeCVar(TEXT("hs.Audio.SFXVolume"), SfxVolume);
    SetRuntimeCVar(TEXT("hs.Audio.MusicVolume"), MusicVolume);
    SetRuntimeCVar(TEXT("hs.Audio.VoiceVolume"), VoiceVolume);
    SetRuntimeCVar(TEXT("hs.Audio.AmbientVolume"), AmbientVolume);

    if (GConfig)
    {
        const TCHAR* Section = TEXT("HuntingSpirit.Audio");
        GConfig->SetFloat(Section, TEXT("MasterVolume"), AudioSettings.MasterVolume, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("SFXVolume"), AudioSettings.SFXVolume, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("MusicVolume"), AudioSettings.MusicVolume, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("VoiceVolume"), AudioSettings.VoiceVolume, GGameUserSettingsIni);
        GConfig->SetFloat(Section, TEXT("AmbientVolume"), AudioSettings.AmbientVolume, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bMasterMuted"), AudioSettings.bMasterMuted, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bSFXMuted"), AudioSettings.bSFXMuted, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bMusicMuted"), AudioSettings.bMusicMuted, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bVoiceMuted"), AudioSettings.bVoiceMuted, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("bAmbientMuted"), AudioSettings.bAmbientMuted, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

void UHSSaveGameData::ApplyInputSettingsToEngine() const
{
    SetRuntimeCVar(TEXT("hs.Input.MouseSensitivity"), InputSettings.MouseSensitivity);
    SetRuntimeCVar(TEXT("hs.Input.ControllerSensitivity"), InputSettings.ControllerSensitivity);
    SetRuntimeCVarBool(TEXT("hs.Input.InvertMouseY"), InputSettings.bInvertMouseY);
    SetRuntimeCVarBool(TEXT("hs.Input.InvertControllerY"), InputSettings.bInvertControllerY);
    SetRuntimeCVarBool(TEXT("hs.Input.ControllerVibration"), InputSettings.bControllerVibrationsEnabled);

    if (GConfig)
    {
        const TCHAR* Section = TEXT("HuntingSpirit.Input");
        GConfig->SetFloat(Section, TEXT("MouseSensitivity"), InputSettings.MouseSensitivity, GInputIni);
        GConfig->SetFloat(Section, TEXT("ControllerSensitivity"), InputSettings.ControllerSensitivity, GInputIni);
        GConfig->SetBool(Section, TEXT("bInvertMouseY"), InputSettings.bInvertMouseY, GInputIni);
        GConfig->SetBool(Section, TEXT("bInvertControllerY"), InputSettings.bInvertControllerY, GInputIni);
        GConfig->SetBool(Section, TEXT("bControllerVibrationsEnabled"), InputSettings.bControllerVibrationsEnabled, GInputIni);
        for (const TPair<FString, FString>& Binding : InputSettings.KeyBindings)
        {
            const FString ConfigKey = FString::Printf(TEXT("KeyBinding_%s"), *Binding.Key);
            GConfig->SetString(Section, *ConfigKey, *Binding.Value, GInputIni);
        }
        GConfig->Flush(false, GInputIni);
    }

    if (UInputSettings* EngineInputSettings = UInputSettings::GetInputSettings())
    {
        const TArray<FInputActionKeyMapping> ExistingMappings = EngineInputSettings->GetActionMappings();
        for (const TPair<FString, FString>& Binding : InputSettings.KeyBindings)
        {
            const FName ActionName(*Binding.Key);
            const FKey DesiredKey(FName(*Binding.Value));
            if (!DesiredKey.IsValid())
            {
                continue;
            }

            for (const FInputActionKeyMapping& Mapping : ExistingMappings)
            {
                if (Mapping.ActionName == ActionName)
                {
                    EngineInputSettings->RemoveActionMapping(Mapping, false);
                }
            }

            EngineInputSettings->AddActionMapping(FInputActionKeyMapping(ActionName, DesiredKey), false);
        }

        EngineInputSettings->SaveKeyMappings();
        EngineInputSettings->ForceRebuildKeymaps();
    }
}
