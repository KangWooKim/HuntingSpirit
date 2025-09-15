/**
 * @file HSBossAIController.h
 * @brief 보스 캐릭터 전용 AI 컨트롤러 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/AI/HSAIControllerBase.h"
#include "HuntingSpirit/Enemies/Bosses/HSBossBase.h"
#include "HSBossAIController.generated.h"

// 전방 선언
class UBehaviorTree;
class UBlackboardData;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class UHSBossPhaseSystem;
class UHSBossAbilitySystem;

// 보스 AI 상태
UENUM(BlueprintType)
enum class EBossAIState : uint8
{
    Idle                UMETA(DisplayName = "Idle"),
    PatrolPhase         UMETA(DisplayName = "Patrol Phase"),
    CombatPhase         UMETA(DisplayName = "Combat Phase"),
    TransitionPhase     UMETA(DisplayName = "Phase Transition"),
    ExecutingPattern    UMETA(DisplayName = "Executing Pattern"),
    Enraged             UMETA(DisplayName = "Enraged"),
    Defeated            UMETA(DisplayName = "Defeated")
};

// 타겟 선택 전략
UENUM(BlueprintType)
enum class EBossTargetStrategy : uint8
{
    HighestThreat       UMETA(DisplayName = "Highest Threat"),
    LowestHealth        UMETA(DisplayName = "Lowest Health"),
    NearestTarget       UMETA(DisplayName = "Nearest Target"),
    RandomTarget        UMETA(DisplayName = "Random Target"),
    SpecificRole        UMETA(DisplayName = "Specific Role")
};

/**
 * @brief 보스 캐릭터 전용 AI 컨트롤러
 * @details 복잡한 보스 AI 패턴, 페이즈별 행동, 협동 대응 메커니즘을 관리합니다.
 *          HSAIControllerBase를 상속받아 보스 전용 고급 기능을 추가로 제공합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API AHSBossAIController : public AHSAIControllerBase
{
    GENERATED_BODY()

public:
    /**
     * @brief 기본 생성자
     * @details 보스 AI 전용 설정값들을 초기화합니다
     */
    AHSBossAIController();

    /**
     * @brief 폰 빙의 시 호출되는 함수
     * @param InPawn 빙의할 보스 폰
     */
    virtual void OnPossess(APawn* InPawn) override;

    /**
     * @brief 폰 빙의 해제 시 호출되는 함수
     */
    virtual void OnUnPossess() override;

    /**
     * @brief 게임 시작 시 보스 AI 초기화
     */
    virtual void BeginPlay() override;

    /**
     * @brief 매 프레임 보스 AI 업데이트
     * @param DeltaTime 이전 프레임과의 시간 차이
     */
    virtual void Tick(float DeltaTime) override;

    /**
     * @brief 보스 AI 상태를 설정
     * @param NewState 새로운 AI 상태
     */
    UFUNCTION(BlueprintCallable, Category = "Boss AI")
    void SetAIState(EBossAIState NewState);

    /**
     * @brief 현재 보스 AI 상태를 반환
     * @return 현재 AI 상태
     */
    UFUNCTION(BlueprintPure, Category = "Boss AI")
    EBossAIState GetAIState() const { return CurrentAIState; }

    /**
     * @brief 타겟 선택 로직을 업데이트
     * @details 현재 전략에 따라 최적의 타겟을 선택합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Boss AI|Target")
    void UpdateTargetSelection();

    /**
     * @brief 타겟 선택 전략을 설정
     * @param NewStrategy 새로운 타겟 선택 전략
     */
    UFUNCTION(BlueprintCallable, Category = "Boss AI|Target")
    void SetTargetStrategy(EBossTargetStrategy NewStrategy);

    /**
     * @brief 주요 타겟을 반환
     * @return 현재 주요 타겟 액터
     */
    UFUNCTION(BlueprintPure, Category = "Boss AI|Target")
    AActor* GetPrimaryTarget() const { return PrimaryTarget; }

    /**
     * @brief 모든 타겟 목록을 반환
     * @return 현재 인식된 모든 타겟들의 배열
     */
    UFUNCTION(BlueprintPure, Category = "Boss AI|Target")
    TArray<AActor*> GetAllTargets() const { return CurrentTargets; }

    // 패턴 실행
    UFUNCTION(BlueprintCallable, Category = "Boss AI|Pattern")
    void ExecuteNextPattern();

    UFUNCTION(BlueprintCallable, Category = "Boss AI|Pattern")
    void InterruptCurrentPattern();

    UFUNCTION(BlueprintPure, Category = "Boss AI|Pattern")
    bool IsExecutingPattern() const { return bIsExecutingPattern; }

    // 페이즈 전환
    UFUNCTION(BlueprintCallable, Category = "Boss AI|Phase")
    void HandlePhaseTransition(EHSBossPhase OldPhase, EHSBossPhase NewPhase);

    // 협동 메커니즘
    UFUNCTION(BlueprintCallable, Category = "Boss AI|Coop")
    void EvaluateCoopThreat();

    UFUNCTION(BlueprintCallable, Category = "Boss AI|Coop")
    void TriggerCoopCounterMeasure();

    // 환경 활용
    UFUNCTION(BlueprintCallable, Category = "Boss AI|Environment")
    void ScanEnvironmentForTactics();

    UFUNCTION(BlueprintCallable, Category = "Boss AI|Environment")
    void ExecuteEnvironmentalTactic();

    // 플레이어 수 조회
    UFUNCTION(BlueprintPure, Category = "Boss AI|Target")
    int32 GetActivePlayerCount() const;

protected:
    // 초기화 함수들
    virtual void InitializeBossAI();
    virtual void SetupPerceptionSystem();
    virtual void SetupBehaviorTree();

    // 인식 시스템 콜백
    virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    virtual void OnTargetPerceptionForgotten(AActor* Actor);

    // 페이즈별 행동
    virtual void ExecutePhase1Behavior();
    virtual void ExecutePhase2Behavior();
    virtual void ExecutePhase3Behavior();
    virtual void ExecuteEnragedBehavior();
    virtual void ExecuteFinalStandBehavior();

    // 패턴 선택 로직
    virtual FHSBossAttackPattern SelectPatternForPhase(EHSBossPhase Phase);
    virtual bool EvaluatePatternConditions(const FHSBossAttackPattern& Pattern);
    virtual float CalculatePatternScore(const FHSBossAttackPattern& Pattern);

    // 타겟 평가
    virtual float EvaluateThreatLevel(AActor* Target);
    virtual bool IsValidTarget(AActor* Target);
    virtual void UpdateThreatTable();

    // 전술적 의사결정
    virtual bool ShouldRetreat();
    virtual bool ShouldUseEnvironment();
    virtual bool ShouldCallMinions();
    virtual FVector SelectTacticalPosition();

    // 보스 참조
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI")
    AHSBossBase* ControlledBoss;

    // AI 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|State")
    EBossAIState CurrentAIState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|State")
    float StateTransitionDelay = 1.5f;

    // 타겟 관리
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Target")
    AActor* PrimaryTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Target")
    TArray<AActor*> CurrentTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Target")
    EBossTargetStrategy TargetStrategy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Target")
    float TargetSwitchInterval = 5.0f;

    // 위협 수준 시스템
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Threat")
    TMap<AActor*, float> ThreatTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Threat")
    float ThreatDecayRate = 0.1f;

    // 패턴 실행
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Pattern")
    bool bIsExecutingPattern;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Pattern")
    FHSBossAttackPattern CurrentExecutingPattern;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Pattern")
    float PatternSelectionInterval = 3.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Pattern")
    FTimerHandle PatternSelectionTimer;

    // 페이즈별 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Phase")
    TMap<EHSBossPhase, float> PhaseAggressionLevels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Phase")
    TMap<EHSBossPhase, float> PhaseRetreatThresholds;

    // 협동 대응
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Coop")
    float CoopThreatThreshold = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Coop")
    bool bCoopCounterMeasureActive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Coop")
    float CoopResponseDelay = 2.0f;

    // 환경 전술
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Environment")
    float EnvironmentScanRadius = 2000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss AI|Environment")
    TArray<AActor*> TacticalEnvironmentActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Environment")
    float EnvironmentUsageChance = 0.3f;

    // 인식 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Perception")
    float BossSightRadius = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Perception")
    float BossHearingRange = 4000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Perception")
    float BossPeripheralVisionAngle = 90.0f;

    // 성능 최적화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Optimization")
    float AITickInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Optimization")
    int32 MaxSimultaneousTargets = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Optimization")
    bool bUseOctreeForTargetSearch = true;

private:
    // 내부 헬퍼 함수들
    void UpdateAITick(float DeltaTime);
    void ProcessStateTransitions();
    void ValidateTargets();
    void CleanupThreatTable();
    AActor* SelectTargetByStrategy();
    void BroadcastAIEvents();
    void UpdatePatternWeights();

    // 타이머 핸들들
    FTimerHandle TargetUpdateTimer;
    FTimerHandle ThreatDecayTimer;
    FTimerHandle StateTransitionTimer;
    FTimerHandle EnvironmentScanTimer;

    // 캐시된 데이터
    float LastPatternExecutionTime;
    TArray<FHSBossAttackPattern> AvailablePatterns;
    FVector LastKnownTargetLocation;
    
    // 패턴 가중치 시스템
    TMap<FName, float> PatternWeights;
    
    // 최적화를 위한 플래그
    bool bNeedsTargetUpdate;
    bool bNeedsPatternRecalculation;
    bool bIsInCombat;

    // 디버그 설정
    UPROPERTY(EditAnywhere, Category = "Boss AI|Debug")
    bool bDebugDrawPerception;

    UPROPERTY(EditAnywhere, Category = "Boss AI|Debug")
    bool bDebugDrawThreatLevels;

    UPROPERTY(EditAnywhere, Category = "Boss AI|Debug")
    bool bDebugLogPatternSelection;
};
