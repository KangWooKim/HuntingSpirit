#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSPersistentProgress.generated.h"

// 영구 통계 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSPersistentStatistics
{
    GENERATED_BODY()

    // 총 게임 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Total Stats")
    int32 TotalRunsStarted = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Total Stats")
    int32 TotalRunsCompleted = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Total Stats")
    int32 TotalBossesDefeated = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Total Stats")
    int32 TotalEnemiesKilled = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Total Stats")
    int32 TotalDeaths = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Total Stats")
    float TotalPlayTime = 0.0f; // 전체 플레이 시간 (초)

    // 최고 기록들
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Best Records")
    float BestRunTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Best Records")
    float BestBossKillTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Best Records")
    int32 MostEnemiesKilledInRun = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Best Records")
    int32 LongestSurvivalStreak = 0; // 연속 무사망 런 횟수

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Best Records")
    int32 HighestDifficultyCleared = 0; // 클리어한 최고 난이도

    // 협동 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cooperation Stats")
    int32 TotalCooperativeActions = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cooperation Stats")
    int32 TotalPlayersRevived = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cooperation Stats")
    int32 TotalComboAttacks = 0;

    // 수집 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Collection Stats")
    int32 TotalItemsCollected = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Collection Stats")
    int32 TotalResourcesGathered = 0;

    // 난이도별 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Difficulty Stats")
    TMap<int32, int32> RunsCompletedByDifficulty;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Difficulty Stats")
    TMap<int32, float> BestTimesByDifficulty;

    FHSPersistentStatistics()
    {
        Reset();
    }

    void Reset()
    {
        TotalRunsStarted = 0;
        TotalRunsCompleted = 0;
        TotalBossesDefeated = 0;
        TotalEnemiesKilled = 0;
        TotalDeaths = 0;
        TotalPlayTime = 0.0f;
        BestRunTime = 0.0f;
        BestBossKillTime = 0.0f;
        MostEnemiesKilledInRun = 0;
        LongestSurvivalStreak = 0;
        HighestDifficultyCleared = 0;
        TotalCooperativeActions = 0;
        TotalPlayersRevived = 0;
        TotalComboAttacks = 0;
        TotalItemsCollected = 0;
        TotalResourcesGathered = 0;
        RunsCompletedByDifficulty.Empty();
        BestTimesByDifficulty.Empty();
    }
};

// 영구 진행도 메타 프로필 구조체 (게임 간 지속되는 메타 정보)
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSPersistentMetaProfile
{
    GENERATED_BODY()

    // 기본 정보
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
    FString PlayerName = TEXT("Unknown Player");

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
    int32 PlayerLevel = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
    int32 Experience = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
    int32 ExperienceToNextLevel = 100;

    // 선호 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Preferences")
    FString FavoriteCharacterClass = TEXT("Warrior");

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Preferences")
    int32 PreferredDifficulty = 1; // Normal

    // 생성 시간
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
    FDateTime CreationTime;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
    FDateTime LastPlayTime;

    FHSPersistentMetaProfile()
    {
        CreationTime = FDateTime::Now();
        LastPlayTime = FDateTime::Now();
    }
};

// 업적 정보 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSAchievementInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    FString AchievementID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    FString Title;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    FString Description;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    bool bIsUnlocked = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    FDateTime UnlockTime;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    int32 Progress = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    int32 RequiredProgress = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    int32 RewardMetaSouls = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Achievement")
    int32 RewardUnlockPoints = 0;

    FHSAchievementInfo()
    {
        UnlockTime = FDateTime::MinValue();
    }

    bool IsCompleted() const
    {
        return Progress >= RequiredProgress;
    }

    float GetProgressPercentage() const
    {
        if (RequiredProgress <= 0)
        {
            return 0.0f;
        }
        return FMath::Clamp(static_cast<float>(Progress) / RequiredProgress, 0.0f, 1.0f);
    }
};

// 영구 진행도 델리게이트들
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMetaPlayerLevelUp, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAchievementUnlocked, const FString&, AchievementID, const FHSAchievementInfo&, Achievement);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatisticUpdated, const FHSPersistentStatistics&, Statistics);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetaCurrencyChanged, const FString&, CurrencyType, int32, NewAmount);

/**
 * 영구 진행도 관리 시스템
 * - 게임 간 지속되는 플레이어 데이터 관리
 * - 통계, 업적, 프로필 정보 추적
 * - 메타 화폐 및 경험치 시스템
 * - 자동 저장/로드 기능
 * - 성능 최적화 및 데이터 무결성 보장
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSPersistentProgress : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSPersistentProgress();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 저장/로드 메서드
    
    /**
     * 진행도 데이터를 로드합니다
     * @return 로드 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool LoadProgressData();

    /**
     * 진행도 데이터를 저장합니다
     * @return 저장 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool SaveProgressData();

    /**
     * 자동 저장을 설정합니다
     * @param bEnabled 자동 저장 활성화 여부
     * @param IntervalSeconds 저장 간격 (초)
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    void SetAutoSave(bool bEnabled, float IntervalSeconds = 60.0f);

    // 통계 관리
    
    /**
     * 런 시작을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordRunStarted();

    /**
     * 런 완료를 기록합니다
     * @param RunTime 런 지속시간
     * @param Difficulty 난이도
     * @param bVictory 승리 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordRunCompleted(float RunTime, int32 Difficulty, bool bVictory);

    /**
     * 보스 처치를 기록합니다
     * @param KillTime 처치 시간
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordBossKill(float KillTime);

    /**
     * 적 처치를 기록합니다
     * @param Count 처치 수
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordEnemyKills(int32 Count);

    /**
     * 플레이어 사망을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordPlayerDeath();

    /**
     * 플레이 시간을 추가합니다
     * @param PlayTime 플레이 시간 (초)
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddPlayTime(float PlayTime);

    /**
     * 협동 행동을 기록합니다
     * @param CooperativeActions 협동 행동 수
     * @param PlayersRevived 부활시킨 플레이어 수
     * @param ComboAttacks 콤보 공격 수
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordCooperativeStats(int32 CooperativeActions, int32 PlayersRevived, int32 ComboAttacks);

    /**
     * 수집 통계를 기록합니다
     * @param ItemsCollected 수집한 아이템 수
     * @param ResourcesGathered 채집한 자원 수
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void RecordCollectionStats(int32 ItemsCollected, int32 ResourcesGathered);

    // 경험치 및 레벨 시스템
    
    /**
     * 경험치를 추가합니다
     * @param Experience 추가할 경험치
     */
    UFUNCTION(BlueprintCallable, Category = "Experience")
    void AddExperience(int32 Experience);

    /**
     * 플레이어 레벨을 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    int32 GetPlayerLevel() const;

    /**
     * 현재 경험치를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    int32 GetCurrentExperience() const;

    /**
     * 다음 레벨까지 필요한 경험치를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    int32 GetExperienceToNextLevel() const;

    /**
     * 현재 레벨의 진행률을 반환합니다 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    float GetLevelProgress() const;

    // 메타 화폐 관리
    
    /**
     * 메타 화폐를 추가합니다
     * @param CurrencyType 화폐 종류
     * @param Amount 추가할 양
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    void AddMetaCurrency(const FString& CurrencyType, int32 Amount);

    /**
     * 메타 화폐를 사용합니다
     * @param CurrencyType 화폐 종류
     * @param Amount 사용할 양
     * @return 사용 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    bool SpendMetaCurrency(const FString& CurrencyType, int32 Amount);

    /**
     * 메타 화폐 잔액을 반환합니다
     * @param CurrencyType 화폐 종류
     * @return 잔액
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    int32 GetMetaCurrency(const FString& CurrencyType) const;

    /**
     * 메타 화폐 잔액이 충분한지 확인합니다
     * @param CurrencyType 화폐 종류
     * @param Amount 필요한 양
     * @return 충분한지 여부
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    bool HasEnoughMetaCurrency(const FString& CurrencyType, int32 Amount) const;

    // 업적 시스템
    
    /**
     * 업적 진행도를 업데이트합니다
     * @param AchievementID 업적 ID
     * @param Progress 진행도
     */
    UFUNCTION(BlueprintCallable, Category = "Achievements")
    void UpdateAchievementProgress(const FString& AchievementID, int32 Progress);

    /**
     * 업적이 언락되었는지 확인합니다
     * @param AchievementID 업적 ID
     * @return 언락 여부
     */
    UFUNCTION(BlueprintPure, Category = "Achievements")
    bool IsAchievementUnlocked(const FString& AchievementID) const;

    /**
     * 업적 정보를 반환합니다
     * @param AchievementID 업적 ID
     * @return 업적 정보
     */
    UFUNCTION(BlueprintPure, Category = "Achievements")
    FHSAchievementInfo GetAchievementInfo(const FString& AchievementID) const;

    /**
     * 모든 업적 목록을 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Achievements")
    TArray<FHSAchievementInfo> GetAllAchievements() const;

    /**
     * 언락된 업적 수를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Achievements")
    int32 GetUnlockedAchievementCount() const;

    // 프로필 관리
    
    /**
     * 플레이어 프로필을 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Profile")
    const FHSPersistentMetaProfile& GetPlayerProfile() const;

    /**
     * 플레이어 이름을 설정합니다
     * @param PlayerName 새 플레이어 이름
     */
    UFUNCTION(BlueprintCallable, Category = "Profile")
    void SetPlayerName(const FString& PlayerName);

    /**
     * 선호 캐릭터 클래스를 설정합니다
     * @param CharacterClass 선호 클래스
     */
    UFUNCTION(BlueprintCallable, Category = "Profile")
    void SetFavoriteCharacterClass(const FString& CharacterClass);

    /**
     * 선호 난이도를 설정합니다
     * @param Difficulty 선호 난이도
     */
    UFUNCTION(BlueprintCallable, Category = "Profile")
    void SetPreferredDifficulty(int32 Difficulty);

    // 통계 조회
    
    /**
     * 영구 통계를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    const FHSPersistentStatistics& GetPersistentStatistics() const;

    /**
     * 승률을 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    float GetWinRate() const;

    /**
     * 평균 런 시간을 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    float GetAverageRunTime() const;

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMetaPlayerLevelUp OnMetaPlayerLevelUp;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAchievementUnlocked OnAchievementUnlocked;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStatisticUpdated OnStatisticUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMetaCurrencyChanged OnMetaCurrencyChanged;

protected:
    // 핵심 데이터
    UPROPERTY(BlueprintReadOnly, Category = "Data")
    FHSPersistentStatistics Statistics;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    FHSPersistentMetaProfile PlayerProfile;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, int32> MetaCurrencies;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, FHSAchievementInfo> Achievements;

    // 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    FString SaveFileName = TEXT("HuntingSpiritProgress");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    int32 SaveVersion = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    bool bAutoSaveEnabled = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    float AutoSaveInterval = 60.0f;

    // 레벨 시스템 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level System")
    int32 BaseExperienceRequirement = 100;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level System")
    float ExperienceScalingFactor = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level System")
    int32 MaxPlayerLevel = 100;

private:
    // 타이머
    FTimerHandle AutoSaveTimerHandle;

    // 캐싱 (성능 최적화)
    mutable int32 CachedExperienceToNextLevel = -1;
    mutable int32 CachedPlayerLevel = -1;
    mutable float CachedWinRate = -1.0f;
    mutable int32 CachedStatisticsHash = -1;

    // 내부 메서드들
    
    /**
     * 기본 데이터를 초기화합니다
     */
    void InitializeDefaultData();

    /**
     * 업적을 초기화합니다
     */
    void InitializeAchievements();

    /**
     * 레벨업을 처리합니다
     */
    void ProcessLevelUp();

    /**
     * 다음 레벨 필요 경험치를 계산합니다
     * @param Level 레벨
     * @return 필요 경험치
     */
    int32 CalculateExperienceForLevel(int32 Level) const;

    /**
     * 업적 체크를 수행합니다
     */
    void CheckAchievements();

    /**
     * 업적을 언락합니다
     * @param AchievementID 업적 ID
     */
    void UnlockAchievement(const FString& AchievementID);

    /**
     * 자동 저장을 수행합니다
     */
    void PerformAutoSave();

    /**
     * 데이터 무결성을 검증합니다
     */
    bool ValidateData() const;

    /**
     * 통계 해시를 계산합니다 (캐시 무효화용)
     */
    int32 CalculateStatisticsHash() const;

    /**
     * 캐시를 무효화합니다
     */
    void InvalidateCache();
};
