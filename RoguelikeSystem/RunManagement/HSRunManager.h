#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "HSRunManager.generated.h"

// 런 상태 열거형
UENUM(BlueprintType)
enum class EHSRunState : uint8
{
    None            UMETA(DisplayName = "None"),
    Preparing       UMETA(DisplayName = "Preparing"),
    Active          UMETA(DisplayName = "Active"),
    Paused          UMETA(DisplayName = "Paused"),
    Completed       UMETA(DisplayName = "Completed"),
    Failed          UMETA(DisplayName = "Failed"),
    Abandoned       UMETA(DisplayName = "Abandoned")
};

// 런 결과 열거형
UENUM(BlueprintType)
enum class EHSRunResult : uint8
{
    None            UMETA(DisplayName = "None"),
    Victory         UMETA(DisplayName = "Victory"),
    Defeat          UMETA(DisplayName = "Defeat"),
    Timeout         UMETA(DisplayName = "Timeout"),
    Disconnection   UMETA(DisplayName = "Disconnection"),
    Abandoned       UMETA(DisplayName = "Abandoned")
};

// 런 난이도 열거형
UENUM(BlueprintType)
enum class EHSRunDifficulty : uint8
{
    Easy        UMETA(DisplayName = "Easy"),
    Normal      UMETA(DisplayName = "Normal"),
    Hard        UMETA(DisplayName = "Hard"),
    Nightmare   UMETA(DisplayName = "Nightmare"),
    Hell        UMETA(DisplayName = "Hell")
};

// 런 통계 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSRunStatistics
{
    GENERATED_BODY()

    // 기본 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    int32 EnemiesKilled = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    int32 BossesDefeated = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    int32 DeathCount = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    int32 ReviveCount = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    float TotalDamageDealt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    float TotalDamageTaken = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    int32 ItemsCollected = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    int32 ResourcesGathered = 0;

    // 시간 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Time")
    float RunDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Time")
    float BestBossKillTime = 0.0f;

    // 협동 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cooperation")
    int32 CooperativeActions = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cooperation")
    int32 PlayersRevived = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cooperation")
    int32 ComboAttacks = 0;

    FHSRunStatistics()
    {
        Reset();
    }

    void Reset()
    {
        EnemiesKilled = 0;
        BossesDefeated = 0;
        DeathCount = 0;
        ReviveCount = 0;
        TotalDamageDealt = 0.0f;
        TotalDamageTaken = 0.0f;
        ItemsCollected = 0;
        ResourcesGathered = 0;
        RunDuration = 0.0f;
        BestBossKillTime = 0.0f;
        CooperativeActions = 0;
        PlayersRevived = 0;
        ComboAttacks = 0;
    }
};

// 런 보상 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSRunRewards
{
    GENERATED_BODY()

    // 메타 화폐 보상
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Currency")
    int32 MetaSouls = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Currency")
    int32 EssencePoints = 0;

    // 경험치 보상
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Experience")
    int32 BaseExperience = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Experience")
    int32 BonusExperience = 0;

    // 언락 포인트
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unlocks")
    int32 UnlockPoints = 0;

    FHSRunRewards()
    {
        Reset();
    }

    void Reset()
    {
        MetaSouls = 0;
        EssencePoints = 0;
        BaseExperience = 0;
        BonusExperience = 0;
        UnlockPoints = 0;
    }
};

// 런 설정 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSRunConfiguration
{
    GENERATED_BODY()

    // 기본 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Configuration")
    EHSRunDifficulty Difficulty = EHSRunDifficulty::Normal;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Configuration")
    int32 MaxPlayers = 4;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Configuration")
    float TimeLimit = 3600.0f; // 1시간

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Configuration")
    bool bAllowRespawn = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Configuration")
    int32 MaxRespawns = 3;

    // 월드 생성 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "World")
    int32 WorldSeed = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "World")
    FString BiomeType = TEXT("Random");

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "World")
    int32 WorldSize = 1000;

    // 보스 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Boss")
    FString BossType = TEXT("Random");

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Boss")
    float BossHealthMultiplier = 1.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Boss")
    float BossDamageMultiplier = 1.0f;

    FHSRunConfiguration()
    {
        // 기본값들은 이미 위에서 설정됨
    }
};

// 런 데이터 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSRunData
{
    GENERATED_BODY()

    // 기본 정보
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString RunID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    EHSRunState State = EHSRunState::None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    EHSRunResult Result = EHSRunResult::None;

    // 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Configuration")
    FHSRunConfiguration Configuration;

    // 통계
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Statistics")
    FHSRunStatistics Statistics;

    // 보상
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rewards")
    FHSRunRewards Rewards;

    // 시간 정보
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Time")
    FDateTime StartTime;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Time")
    FDateTime EndTime;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Time")
    float ElapsedTime = 0.0f;

    // 참가자 정보
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Players")
    TArray<FString> ParticipantIDs;

    FHSRunData()
    {
        RunID = FGuid::NewGuid().ToString();
        State = EHSRunState::None;
        Result = EHSRunResult::None;
        StartTime = FDateTime::Now();
        EndTime = FDateTime::MinValue();
        ElapsedTime = 0.0f;
    }
};

// 런 매니저 델리게이트들
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunStateChanged, EHSRunState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunCompleted, EHSRunResult, Result, const FHSRunRewards&, Rewards);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunStatisticUpdated, const FHSRunStatistics&, Statistics);

/**
 * 로그라이크 런 관리자 시스템
 * - 게임 런의 시작, 진행, 종료를 관리
 * - 런 통계 및 보상 계산
 * - 멀티플레이어 환경에서의 런 동기화
 * - 메모리 풀링 및 성능 최적화 적용
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSRunManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSRunManager();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 런 관리 메서드
    
    /**
     * 새로운 런을 시작합니다
     * @param Configuration 런 설정
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Run Management")
    bool StartNewRun(const FHSRunConfiguration& Configuration);

    /**
     * 현재 런을 종료합니다
     * @param Result 런 결과
     */
    UFUNCTION(BlueprintCallable, Category = "Run Management")
    void EndCurrentRun(EHSRunResult Result);

    /**
     * 현재 런을 일시정지합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Run Management")
    void PauseCurrentRun();

    /**
     * 일시정지된 런을 재개합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Run Management")
    void ResumeCurrentRun();

    /**
     * 현재 런을 포기합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Run Management")
    void AbandonCurrentRun();

    // 런 상태 조회
    
    /**
     * 현재 런 상태를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Run Management")
    EHSRunState GetCurrentRunState() const;

    /**
     * 현재 런 데이터를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Run Management")
    const FHSRunData& GetCurrentRunData() const;

    /**
     * 런이 활성화 상태인지 확인합니다
     */
    UFUNCTION(BlueprintPure, Category = "Run Management")
    bool IsRunActive() const;

    /**
     * 런 진행률을 반환합니다 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Run Management")
    float GetRunProgress() const;

    // 통계 업데이트 메서드들
    
    /**
     * 적 처치 수를 추가합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddEnemyKill(int32 Count = 1);

    /**
     * 보스 처치 수를 추가합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddBossKill(float KillTime);

    /**
     * 플레이어 사망을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddPlayerDeath();

    /**
     * 플레이어 부활을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddPlayerRevive();

    /**
     * 데미지를 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddDamage(float DamageDealt, float DamageTaken);

    /**
     * 아이템 수집을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddItemCollection(int32 Count = 1);

    /**
     * 자원 채집을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddResourceGathering(int32 Count = 1);

    /**
     * 협동 행동을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddCooperativeAction();

    /**
     * 콤보 공격을 기록합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void AddComboAttack();

    // 보상 계산
    
    /**
     * 런 보상을 계산합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Rewards")
    FHSRunRewards CalculateRunRewards() const;

    /**
     * 난이도 보너스를 계산합니다
     */
    UFUNCTION(BlueprintPure, Category = "Rewards")
    float GetDifficultyMultiplier(EHSRunDifficulty Difficulty) const;

    /**
     * 협동 보너스를 계산합니다
     */
    UFUNCTION(BlueprintPure, Category = "Rewards")
    float GetCooperationBonus() const;

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunStateChanged OnRunStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunCompleted OnRunCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunStatisticUpdated OnRunStatisticUpdated;

protected:
    // 현재 런 데이터
    UPROPERTY(BlueprintReadOnly, Category = "Current Run")
    FHSRunData CurrentRun;

    // 런 설정 및 상태
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    float StatisticsUpdateInterval = 1.0f;

    // 타이머 핸들들
    FTimerHandle RunUpdateTimerHandle;
    FTimerHandle StatisticsTimerHandle;

private:
    // 내부 메서드들
    
    /**
     * 런 상태를 변경합니다
     */
    void ChangeRunState(EHSRunState NewState);

    /**
     * 런 업데이트를 처리합니다
     */
    void UpdateRunProgress();

    /**
     * 통계 업데이트를 처리합니다
     */
    void UpdateStatistics();

    /**
     * 런 데이터를 초기화합니다
     */
    void InitializeRunData(const FHSRunConfiguration& Configuration);

    /**
     * 보상 배율을 계산합니다
     */
    float CalculateRewardMultiplier() const;

    /**
     * 성능 최적화를 위한 데이터 캐싱
     */
    mutable float CachedDifficultyMultiplier = 1.0f;
    mutable EHSRunDifficulty CachedDifficulty = EHSRunDifficulty::Normal;
    mutable float CachedCooperationBonus = 1.0f;
    mutable int32 CachedCooperativeActions = -1;
};
