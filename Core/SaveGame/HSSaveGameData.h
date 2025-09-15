#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "../../RoguelikeSystem/Progression/HSMetaCurrency.h"
#include "HSSaveGameData.generated.h"

UENUM(BlueprintType)
enum class EHSAchievementType : uint8
{
    FirstKill,
    BossKiller,
    Survivor,
    Collector,
    Explorer,
    TeamPlayer,
    RunMaster,
    Speedrunner,
    Perfectionist,
    Veteran
};

UENUM(BlueprintType)
enum class EHSDifficulty : uint8
{
    Easy,
    Normal,
    Hard,
    Nightmare,
    Hell
};

UENUM(BlueprintType)
enum class EHSQualityLevel : uint8
{
    Low,
    Medium,
    High,
    Epic,
    Ultra
};

USTRUCT(BlueprintType)
struct FHSGraphicsSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    EHSQualityLevel OverallQuality = EHSQualityLevel::Medium;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    EHSQualityLevel TextureQuality = EHSQualityLevel::Medium;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    EHSQualityLevel ShadowQuality = EHSQualityLevel::Medium;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    EHSQualityLevel EffectsQuality = EHSQualityLevel::Medium;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    EHSQualityLevel PostProcessQuality = EHSQualityLevel::Medium;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    bool bVSyncEnabled = false;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    bool bFullscreenMode = true;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    FIntPoint ScreenResolution = FIntPoint(1920, 1080);

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    float FrameRateLimit = 60.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    bool bMotionBlurEnabled = true;

    FHSGraphicsSettings()
    {
        OverallQuality = EHSQualityLevel::Medium;
        TextureQuality = EHSQualityLevel::Medium;
        ShadowQuality = EHSQualityLevel::Medium;
        EffectsQuality = EHSQualityLevel::Medium;
        PostProcessQuality = EHSQualityLevel::Medium;
        bVSyncEnabled = false;
        bFullscreenMode = true;
        ScreenResolution = FIntPoint(1920, 1080);
        FrameRateLimit = 60.0f;
        bMotionBlurEnabled = true;
    }
};

USTRUCT(BlueprintType)
struct FHSAudioSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    float MasterVolume = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    float SFXVolume = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    float MusicVolume = 0.7f;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    float VoiceVolume = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    float AmbientVolume = 0.8f;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    bool bMasterMuted = false;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    bool bSFXMuted = false;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    bool bMusicMuted = false;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    bool bVoiceMuted = false;

    UPROPERTY(BlueprintReadWrite, Category = "Audio")
    bool bAmbientMuted = false;

    FHSAudioSettings()
    {
        MasterVolume = 1.0f;
        SFXVolume = 1.0f;
        MusicVolume = 0.7f;
        VoiceVolume = 1.0f;
        AmbientVolume = 0.8f;
        bMasterMuted = false;
        bSFXMuted = false;
        bMusicMuted = false;
        bVoiceMuted = false;
        bAmbientMuted = false;
    }
};

USTRUCT(BlueprintType)
struct FHSInputSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float MouseSensitivity = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float ControllerSensitivity = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bInvertMouseY = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bInvertControllerY = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bControllerVibrationsEnabled = true;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    TMap<FString, FString> KeyBindings;

    FHSInputSettings()
    {
        MouseSensitivity = 1.0f;
        ControllerSensitivity = 1.0f;
        bInvertMouseY = false;
        bInvertControllerY = false;
        bControllerVibrationsEnabled = true;
    }
};

USTRUCT(BlueprintType)
struct FHSGameplaySettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    EHSDifficulty PreferredDifficulty = EHSDifficulty::Normal;

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    bool bAutoSaveEnabled = true;

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    float AutoSaveInterval = 300.0f; // 5분

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    bool bDamageNumbersEnabled = true;

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    bool bHealthBarsEnabled = true;

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    bool bCrosshairEnabled = true;

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    bool bSubtitlesEnabled = false;

    UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
    float UIScale = 1.0f;

    FHSGameplaySettings()
    {
        PreferredDifficulty = EHSDifficulty::Normal;
        bAutoSaveEnabled = true;
        AutoSaveInterval = 300.0f;
        bDamageNumbersEnabled = true;
        bHealthBarsEnabled = true;
        bCrosshairEnabled = true;
        bSubtitlesEnabled = false;
        UIScale = 1.0f;
    }
};

USTRUCT(BlueprintType)
struct FHSNetworkSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Network")
    int32 MaxPingThreshold = 100;

    UPROPERTY(BlueprintReadWrite, Category = "Network")
    bool bShowPing = true;

    UPROPERTY(BlueprintReadWrite, Category = "Network")
    bool bAutoConnectToLastServer = false;

    UPROPERTY(BlueprintReadWrite, Category = "Network")
    FString LastServerAddress;

    UPROPERTY(BlueprintReadWrite, Category = "Network")
    FString PreferredRegion;

    UPROPERTY(BlueprintReadWrite, Category = "Network")
    bool bAllowCrossPlatformPlay = true;

    FHSNetworkSettings()
    {
        MaxPingThreshold = 100;
        bShowPing = true;
        bAutoConnectToLastServer = false;
        bAllowCrossPlatformPlay = true;
    }
};

USTRUCT(BlueprintType)
struct FHSAccessibilitySettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Accessibility")
    bool bColorBlindMode = false;

    UPROPERTY(BlueprintReadWrite, Category = "Accessibility")
    float TextSize = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Accessibility")
    bool bHighContrastMode = false;

    UPROPERTY(BlueprintReadWrite, Category = "Accessibility")
    bool bReduceMotion = false;

    UPROPERTY(BlueprintReadWrite, Category = "Accessibility")
    bool bScreenReaderSupport = false;

    FHSAccessibilitySettings()
    {
        bColorBlindMode = false;
        TextSize = 1.0f;
        bHighContrastMode = false;
        bReduceMotion = false;
        bScreenReaderSupport = false;
    }
};

USTRUCT(BlueprintType)
struct FHSPlayerLifetimeStatistics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalPlayTime = 0; // 초 단위

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalRuns = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 SuccessfulRuns = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalEnemiesKilled = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalBossesKilled = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalDeaths = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalRevives = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalItemsCrafted = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 TotalResourcesGathered = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    float BestRunTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    float TotalDamageDealt = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    float TotalDamageTaken = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    float TotalHealingDone = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 HighestLevel = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Statistics")
    int32 LongestSurvivalTime = 0;

    FHSPlayerLifetimeStatistics()
    {
        TotalPlayTime = 0;
        TotalRuns = 0;
        SuccessfulRuns = 0;
        TotalEnemiesKilled = 0;
        TotalBossesKilled = 0;
        TotalDeaths = 0;
        TotalRevives = 0;
        TotalItemsCrafted = 0;
        TotalResourcesGathered = 0;
        BestRunTime = 0.0f;
        TotalDamageDealt = 0.0f;
        TotalDamageTaken = 0.0f;
        TotalHealingDone = 0.0f;
        HighestLevel = 1;
        LongestSurvivalTime = 0;
    }
};

USTRUCT(BlueprintType)
struct FHSAchievementProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Achievement")
    EHSAchievementType AchievementType = EHSAchievementType::FirstKill;

    UPROPERTY(BlueprintReadWrite, Category = "Achievement")
    bool bUnlocked = false;

    UPROPERTY(BlueprintReadWrite, Category = "Achievement")
    int32 CurrentProgress = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Achievement")
    int32 RequiredProgress = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Achievement")
    FDateTime UnlockDate;

    FHSAchievementProgress()
    {
        AchievementType = EHSAchievementType::FirstKill;
        bUnlocked = false;
        CurrentProgress = 0;
        RequiredProgress = 1;
    }
};

USTRUCT(BlueprintType)
struct FHSMetaCurrencyData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "MetaCurrency")
    EHSCurrencyType CurrencyType = EHSCurrencyType::MetaSouls;

    UPROPERTY(BlueprintReadWrite, Category = "MetaCurrency")
    int32 Amount = 0;

    UPROPERTY(BlueprintReadWrite, Category = "MetaCurrency")
    int32 TotalEarned = 0;

    UPROPERTY(BlueprintReadWrite, Category = "MetaCurrency")
    int32 TotalSpent = 0;

    FHSMetaCurrencyData()
    {
        CurrencyType = EHSCurrencyType::MetaSouls;
        Amount = 0;
        TotalEarned = 0;
        TotalSpent = 0;
    }
};

USTRUCT(BlueprintType)
struct FHSUnlockData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Unlock")
    FString UnlockID;

    UPROPERTY(BlueprintReadWrite, Category = "Unlock")
    bool bUnlocked = false;

    UPROPERTY(BlueprintReadWrite, Category = "Unlock")
    FDateTime UnlockDate;

    UPROPERTY(BlueprintReadWrite, Category = "Unlock")
    int32 UnlockCost = 0;

    FHSUnlockData()
    {
        bUnlocked = false;
        UnlockCost = 0;
    }
};

USTRUCT(BlueprintType)
struct FHSSavePlayerProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 PlayerLevel = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 TotalExperience = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FDateTime CreationDate;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FDateTime LastPlayDate;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    TArray<FHSMetaCurrencyData> MetaCurrencies;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    TArray<FHSUnlockData> UnlockedContent;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FHSPlayerLifetimeStatistics Statistics;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    TArray<FHSAchievementProgress> Achievements;

    FHSSavePlayerProfile()
    {
        PlayerLevel = 1;
        TotalExperience = 0;
    }
};

/**
 * 저장 게임 데이터 - 모든 플레이어 진행도 및 설정 관리
 * 현업 최적화: 버전 관리, 압축, 백업 시스템 내장
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSSaveGameData : public USaveGame
{
    GENERATED_BODY()

public:
    UHSSaveGameData();

    // === 기본 저장 데이터 ===
    UPROPERTY(BlueprintReadWrite, Category = "SaveData")
    int32 SaveDataVersion = 1;

    UPROPERTY(BlueprintReadWrite, Category = "SaveData")
    FDateTime SaveDate;

    UPROPERTY(BlueprintReadWrite, Category = "SaveData")
    FString PlayerID;

    UPROPERTY(BlueprintReadWrite, Category = "SaveData")
    bool bIsValidSave = true;

    // === 플레이어 프로필 및 진행도 ===
    UPROPERTY(BlueprintReadWrite, Category = "Player")
    FHSSavePlayerProfile PlayerProfile;

    // === 게임 설정 ===
    UPROPERTY(BlueprintReadWrite, Category = "Settings")
    FHSGraphicsSettings GraphicsSettings;

    UPROPERTY(BlueprintReadWrite, Category = "Settings")
    FHSAudioSettings AudioSettings;

    UPROPERTY(BlueprintReadWrite, Category = "Settings")
    FHSInputSettings InputSettings;

    UPROPERTY(BlueprintReadWrite, Category = "Settings")
    FHSGameplaySettings GameplaySettings;

    UPROPERTY(BlueprintReadWrite, Category = "Settings")
    FHSNetworkSettings NetworkSettings;

    UPROPERTY(BlueprintReadWrite, Category = "Settings")
    FHSAccessibilitySettings AccessibilitySettings;

    // === 런 기록 ===
    UPROPERTY(BlueprintReadWrite, Category = "Runs")
    TArray<FString> CompletedRunIDs;

    UPROPERTY(BlueprintReadWrite, Category = "Runs")
    FString CurrentRunID;

    UPROPERTY(BlueprintReadWrite, Category = "Runs")
    bool bHasActiveRun = false;

    // === 유틸리티 함수 ===
    UFUNCTION(BlueprintCallable, Category = "SaveData")
    void UpdateSaveDate();

    UFUNCTION(BlueprintCallable, Category = "SaveData")
    bool ValidateSaveData() const;

    UFUNCTION(BlueprintCallable, Category = "SaveData")
    void UpgradeSaveDataVersion();

    // 설정 관리
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyGraphicsSettings() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyAudioSettings() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyInputSettings() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyGameplaySettings() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyNetworkSettings() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyAccessibilitySettings() const;

    // 통계 업데이트
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void UpdatePlayTime(int32 AdditionalSeconds);

    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordRunCompletion(bool bSuccessful, float RunTime);

    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordEnemyKill(bool bIsBoss = false);

    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordDeath();

    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordDamage(float DamageDealt, float DamageTaken, float HealingDone);

    // 업적 시스템
    UFUNCTION(BlueprintCallable, Category = "Achievements")
    void UpdateAchievementProgress(EHSAchievementType AchievementType, int32 ProgressIncrement = 1);

    UFUNCTION(BlueprintCallable, Category = "Achievements")
    bool IsAchievementUnlocked(EHSAchievementType AchievementType) const;

    UFUNCTION(BlueprintCallable, Category = "Achievements")
    TArray<FHSAchievementProgress> GetUnlockedAchievements() const;

    // 메타 화폐 관리
    UFUNCTION(BlueprintCallable, Category = "MetaCurrency")
    void AddMetaCurrency(EHSCurrencyType CurrencyType, int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "MetaCurrency")
    bool SpendMetaCurrency(EHSCurrencyType CurrencyType, int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "MetaCurrency")
    int32 GetMetaCurrencyAmount(EHSCurrencyType CurrencyType) const;

    // 언락 시스템
    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    void UnlockContent(const FString& UnlockID, int32 Cost = 0);

    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    bool IsContentUnlocked(const FString& UnlockID) const;

    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    TArray<FString> GetUnlockedContentIDs() const;

protected:
    // 내부 유틸리티
    FHSMetaCurrencyData* FindMetaCurrencyData(EHSCurrencyType CurrencyType);
    const FHSMetaCurrencyData* FindMetaCurrencyData(EHSCurrencyType CurrencyType) const;
    
    FHSAchievementProgress* FindAchievementProgress(EHSAchievementType AchievementType);
    const FHSAchievementProgress* FindAchievementProgress(EHSAchievementType AchievementType) const;
    
    FHSUnlockData* FindUnlockData(const FString& UnlockID);
    const FHSUnlockData* FindUnlockData(const FString& UnlockID) const;

private:
    // 버전 업그레이드 처리
    void UpgradeFromVersion1();
    void UpgradeFromVersion2();
    
    // 설정 적용 헬퍼
    void ApplyGraphicsSettingsToEngine() const;
    void ApplyAudioSettingsToEngine() const;
    void ApplyInputSettingsToEngine() const;
    
    // 데이터 무결성 검증
    bool ValidatePlayerProfile() const;
    bool ValidateSettings() const;
    bool ValidateAchievements() const;
    bool ValidateMetaCurrencies() const;
    
    // 현재 지원되는 최신 버전
    static constexpr int32 LatestSaveDataVersion = 1;
};
