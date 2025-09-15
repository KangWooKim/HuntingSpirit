// 사냥의 영혼(HuntingSpirit) 게임의 게임 상태 클래스
// 전체 게임 상태를 관리하고 네트워크 복제를 담당하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HuntingSpirit/Cooperation/HSTeamManager.h"
#include "HuntingSpirit/Cooperation/HSCoopMechanics.h"
#include "HuntingSpirit/Cooperation/SharedAbilities/HSSharedAbilitySystem.h"
#include "HSGameStateBase.generated.h"

// 전방 선언
class AHSCharacterBase;
class AHSBossBase;
class UHSPerformanceOptimizer;

// 게임 페이즈 정의
UENUM(BlueprintType)
enum class EHSGamePhase : uint8
{
    GP_WaitingForPlayers    UMETA(DisplayName = "Waiting For Players"),    // 플레이어 대기
    GP_Preparation         UMETA(DisplayName = "Preparation"),             // 준비 단계
    GP_Exploration         UMETA(DisplayName = "Exploration"),             // 탐험 단계
    GP_BossEncounter       UMETA(DisplayName = "Boss Encounter"),          // 보스 조우
    GP_Victory             UMETA(DisplayName = "Victory"),                 // 승리
    GP_Defeat              UMETA(DisplayName = "Defeat"),                  // 패배
    GP_GameEnd             UMETA(DisplayName = "Game End")                 // 게임 종료
};

// 게임 상태 실시간 통계 구조체 (현재 세션용)
USTRUCT(BlueprintType)
struct FHSGameStateStatistics
{
    GENERATED_BODY()

    // 게임 시작 시간
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    float GameStartTime;

    // 현재 생존 플레이어 수
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 AlivePlayers;

    // 총 플레이어 수
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 TotalPlayers;

    // 처치한 일반 적 수
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 EnemiesKilled;

    // 처치한 보스 수
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 BossesKilled;

    // 팀 총 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    float TotalDamageDealt;

    // 팀 총 힐링
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    float TotalHealingDone;

    // 성공한 협동 액션 수
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 SuccessfulCoopActions;

    // 수집한 자원 총량
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 ResourcesGathered;

    // 부활 횟수
    UPROPERTY(BlueprintReadOnly, Category = "Game Statistics")
    int32 RevivalCount;

    // 기본값 설정
    FHSGameStateStatistics()
    {
        GameStartTime = 0.0f;
        AlivePlayers = 0;
        TotalPlayers = 0;
        EnemiesKilled = 0;
        BossesKilled = 0;
        TotalDamageDealt = 0.0f;
        TotalHealingDone = 0.0f;
        SuccessfulCoopActions = 0;
        ResourcesGathered = 0;
        RevivalCount = 0;
    }
};

// 월드 상태 정보
USTRUCT(BlueprintType)
struct FHSWorldState
{
    GENERATED_BODY()

    // 월드 시드
    UPROPERTY(BlueprintReadOnly, Category = "World State")
    int32 WorldSeed;

    // 현재 보스 액터
    UPROPERTY(BlueprintReadOnly, Category = "World State")
    TWeakObjectPtr<AHSBossBase> CurrentBoss;

    // 보스 체력 비율
    UPROPERTY(BlueprintReadOnly, Category = "World State")
    float BossHealthPercentage;

    // 활성 자원 노드 수
    UPROPERTY(BlueprintReadOnly, Category = "World State")
    int32 ActiveResourceNodes;

    // 스폰된 적 수
    UPROPERTY(BlueprintReadOnly, Category = "World State")
    int32 SpawnedEnemies;

    // 환경 위험도 레벨
    UPROPERTY(BlueprintReadOnly, Category = "World State")
    float HazardLevel;

    // 기본값 설정
    FHSWorldState()
    {
        WorldSeed = 0;
        CurrentBoss = nullptr;
        BossHealthPercentage = 1.0f;
        ActiveResourceNodes = 0;
        SpawnedEnemies = 0;
        HazardLevel = 1.0f;
    }
};

// 게임 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGamePhaseChanged, EHSGamePhase, OldPhase, EHSGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerCountChanged, int32, NewPlayerCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossSpawned, AHSBossBase*, Boss, FVector, SpawnLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossDefeated, AHSBossBase*, Boss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerRevived, AHSCharacterBase*, RevivedPlayer, AHSCharacterBase*, Reviver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerEliminated, AHSCharacterBase*, EliminatedPlayer, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameStatisticsUpdated, const FHSGameStateStatistics&, NewStatistics);

/**
 * 사냥의 영혼 게임 상태 클래스
 * 
 * 주요 기능:
 * - 전체 게임 상태 및 페이즈 관리
 * - 네트워크 복제를 통한 클라이언트 동기화
 * - 게임 통계 및 성과 추적
 * - 팀 및 협동 시스템 통합 관리
 * - 보스 및 월드 상태 관리
 * - 실시간 성능 모니터링
 * - 메모리 최적화 및 가비지 컬렉션 관리
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API AHSGameStateBase : public AGameStateBase
{
    GENERATED_BODY()

public:
    // 생성자
    AHSGameStateBase();

    // AActor interface
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 네트워크 복제 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // === 게임 페이즈 관리 ===

    /**
     * 게임 페이즈를 변경합니다
     * @param NewPhase 새로운 게임 페이즈
     * @param bForceChange 강제 변경 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Game Phase")
    void SetGamePhase(EHSGamePhase NewPhase, bool bForceChange = false);

    /**
     * 현재 게임 페이즈를 반환합니다
     * @return 현재 게임 페이즈
     */
    UFUNCTION(BlueprintPure, Category = "Game Phase")
    EHSGamePhase GetCurrentGamePhase() const { return CurrentGamePhase; }

    /**
     * 게임이 진행 중인지 확인합니다
     * @return 게임 진행 중 여부
     */
    UFUNCTION(BlueprintPure, Category = "Game Phase")
    bool IsGameInProgress() const;

    /**
     * 게임이 종료되었는지 확인합니다
     * @return 게임 종료 여부
     */
    UFUNCTION(BlueprintPure, Category = "Game Phase")
    bool IsGameEnded() const;

    // === 플레이어 관리 ===

    /**
     * 플레이어가 게임에 참여했을 때 호출됩니다
     * @param JoiningPlayer 참여하는 플레이어
     */
    UFUNCTION(BlueprintCallable, Category = "Player Management")
    void OnPlayerJoined(AHSCharacterBase* JoiningPlayer);

    /**
     * 플레이어가 게임에서 떠났을 때 호출됩니다
     * @param LeavingPlayer 떠나는 플레이어
     */
    UFUNCTION(BlueprintCallable, Category = "Player Management")
    void OnPlayerLeft(AHSCharacterBase* LeavingPlayer);

    /**
     * 플레이어가 사망했을 때 호출됩니다
     * @param DeadPlayer 사망한 플레이어
     * @param Killer 킬러 (null일 수 있음)
     */
    UFUNCTION(BlueprintCallable, Category = "Player Management")
    void OnPlayerDied(AHSCharacterBase* DeadPlayer, AActor* Killer = nullptr);

    /**
     * 플레이어가 부활했을 때 호출됩니다
     * @param RevivedPlayer 부활한 플레이어
     * @param Reviver 부활시킨 플레이어
     */
    UFUNCTION(BlueprintCallable, Category = "Player Management")
    void HandlePlayerRevived(AHSCharacterBase* RevivedPlayer, AHSCharacterBase* Reviver);

    /**
     * 현재 생존 플레이어 수를 반환합니다
     * @return 생존 플레이어 수
     */
    UFUNCTION(BlueprintPure, Category = "Player Management")
    int32 GetAlivePlayerCount() const { return GameStatistics.AlivePlayers; }

    /**
     * 총 플레이어 수를 반환합니다
     * @return 총 플레이어 수
     */
    UFUNCTION(BlueprintPure, Category = "Player Management")
    int32 GetTotalPlayerCount() const { return GameStatistics.TotalPlayers; }

    // === 보스 관리 ===

    /**
     * 보스를 스폰합니다
     * @param BossClass 스폰할 보스 클래스
     * @param SpawnLocation 스폰 위치
     * @return 스폰된 보스 액터
     */
    UFUNCTION(BlueprintCallable, Category = "Boss Management")
    AHSBossBase* SpawnBoss(TSubclassOf<AHSBossBase> BossClass, const FVector& SpawnLocation);

    /**
     * 보스가 패배했을 때 호출됩니다
     * @param DefeatedBoss 패배한 보스
     */
    UFUNCTION(BlueprintCallable, Category = "Boss Management")
    void HandleBossDefeated(AHSBossBase* DefeatedBoss);

    /**
     * 현재 보스를 반환합니다
     * @return 현재 보스 액터 (없으면 nullptr)
     */
    UFUNCTION(BlueprintPure, Category = "Boss Management")
    AHSBossBase* GetCurrentBoss() const;

    /**
     * 보스 체력 비율을 반환합니다
     * @return 보스 체력 비율 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Boss Management")
    float GetBossHealthPercentage() const { return WorldState.BossHealthPercentage; }

    // === 게임 통계 관리 ===

    /**
     * 적 처치 수를 증가시킵니다
     * @param KilledBy 처치한 플레이어
     * @param bIsBoss 보스인지 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Game Statistics")
    void IncrementEnemyKill(AHSCharacterBase* KilledBy, bool bIsBoss = false);

    /**
     * 데미지 통계를 업데이트합니다
     * @param DamageAmount 가한 데미지량
     * @param DamageDealer 데미지를 가한 플레이어
     */
    UFUNCTION(BlueprintCallable, Category = "Game Statistics")
    void UpdateDamageStatistics(float DamageAmount, AHSCharacterBase* DamageDealer);

    /**
     * 힐링 통계를 업데이트합니다
     * @param HealAmount 힐링량
     * @param Healer 힐링한 플레이어
     */
    UFUNCTION(BlueprintCallable, Category = "Game Statistics")
    void UpdateHealingStatistics(float HealAmount, AHSCharacterBase* Healer);

    /**
     * 협동 액션 성공 수를 증가시킵니다
     * @param ActionID 성공한 액션 ID
     * @param Participants 참여자들
     */
    UFUNCTION(BlueprintCallable, Category = "Game Statistics")
    void IncrementCoopActionSuccess(const FName& ActionID, const TArray<AHSCharacterBase*>& Participants);

    /**
     * 자원 수집 통계를 업데이트합니다
     * @param ResourceAmount 수집한 자원량
     * @param Gatherer 수집한 플레이어
     */
    UFUNCTION(BlueprintCallable, Category = "Game Statistics")
    void UpdateResourceStatistics(int32 ResourceAmount, AHSCharacterBase* Gatherer);

    /**
     * 게임 통계를 반환합니다
     * @return 현재 게임 통계
     */
    UFUNCTION(BlueprintPure, Category = "Game Statistics")
    FHSGameStateStatistics GetGameStatistics() const { return GameStatistics; }

    /**
     * 게임 진행 시간을 반환합니다 (초 단위)
     * @return 게임 진행 시간
     */
    UFUNCTION(BlueprintPure, Category = "Game Statistics")
    float GetGameDuration() const;

    // === 월드 상태 관리 ===

    /**
     * 월드 시드를 설정합니다
     * @param NewSeed 새로운 시드값
     */
    UFUNCTION(BlueprintCallable, Category = "World State")
    void SetWorldSeed(int32 NewSeed);

    /**
     * 월드 시드를 반환합니다
     * @return 현재 월드 시드
     */
    UFUNCTION(BlueprintPure, Category = "World State")
    int32 GetWorldSeed() const { return WorldState.WorldSeed; }

    /**
     * 환경 위험도를 업데이트합니다
     * @param NewHazardLevel 새로운 위험도 (1.0 = 기본)
     */
    UFUNCTION(BlueprintCallable, Category = "World State")
    void UpdateHazardLevel(float NewHazardLevel);

    /**
     * 월드 상태를 반환합니다
     * @return 현재 월드 상태
     */
    UFUNCTION(BlueprintPure, Category = "World State")
    FHSWorldState GetWorldState() const { return WorldState; }

    // === 시스템 통합 관리 ===

    /**
     * 팀 매니저를 반환합니다
     * @return 팀 매니저 인스턴스
     */
    UFUNCTION(BlueprintPure, Category = "System Integration")
    UHSTeamManager* GetTeamManager() const { return TeamManager; }

    /**
     * 협동 메커니즘 시스템을 반환합니다
     * @return 협동 메커니즘 인스턴스
     */
    UFUNCTION(BlueprintPure, Category = "System Integration")
    UHSCoopMechanics* GetCoopMechanics() const { return CoopMechanics; }

    /**
     * 공유 능력 시스템을 반환합니다
     * @return 공유 능력 시스템 인스턴스
     */
    UFUNCTION(BlueprintPure, Category = "System Integration")
    UHSSharedAbilitySystem* GetSharedAbilitySystem() const { return SharedAbilitySystem; }

    // === 성능 모니터링 ===

    /**
     * 현재 FPS를 반환합니다
     * @return 평균 FPS
     */
    UFUNCTION(BlueprintPure, Category = "Performance")
    float GetCurrentFPS() const { return CurrentFPS; }

    /**
     * 메모리 사용량을 반환합니다 (MB 단위)
     * @return 현재 메모리 사용량
     */
    UFUNCTION(BlueprintPure, Category = "Performance")
    float GetMemoryUsage() const { return CurrentMemoryUsage; }

    /**
     * 네트워크 핑을 반환합니다
     * @return 평균 핑 (ms)
     */
    UFUNCTION(BlueprintPure, Category = "Performance")
    float GetNetworkPing() const { return AverageNetworkPing; }

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnGamePhaseChanged OnGamePhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnPlayerCountChanged OnPlayerCountChanged;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnBossSpawned OnBossSpawned;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnBossDefeated OnBossDefeated;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnPlayerRevived OnPlayerRevived;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnPlayerEliminated OnPlayerEliminated;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnGameStatisticsUpdated OnGameStatisticsUpdated;

protected:
    // === 복제되는 게임 상태 변수들 ===

    // 현재 게임 페이즈
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
    EHSGamePhase CurrentGamePhase;

    // 게임 통계
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
    FHSGameStateStatistics GameStatistics;

    // 월드 상태
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
    FHSWorldState WorldState;

    // === 시스템 컴포넌트들 ===

    // 팀 매니저 참조
    UPROPERTY(BlueprintReadOnly, Category = "System Components")
    UHSTeamManager* TeamManager;

    // 협동 메커니즘 시스템
    UPROPERTY(BlueprintReadOnly, Category = "System Components")
    UHSCoopMechanics* CoopMechanics;

    // 공유 능력 시스템
    UPROPERTY(BlueprintReadOnly, Category = "System Components")
    UHSSharedAbilitySystem* SharedAbilitySystem;

    // 성능 최적화 시스템
    UPROPERTY(BlueprintReadOnly, Category = "System Components")
    UHSPerformanceOptimizer* PerformanceOptimizer;

    // === 성능 모니터링 변수들 ===

    // 현재 FPS
    UPROPERTY(BlueprintReadOnly, Category = "Performance Monitoring")
    float CurrentFPS;

    // 메모리 사용량 (MB)
    UPROPERTY(BlueprintReadOnly, Category = "Performance Monitoring")
    float CurrentMemoryUsage;

    // 평균 네트워크 핑
    UPROPERTY(BlueprintReadOnly, Category = "Performance Monitoring")
    float AverageNetworkPing;

    // FPS 샘플링을 위한 배열
    UPROPERTY()
    TArray<float> FPSSamples;

    // 핑 샘플링을 위한 배열
    UPROPERTY()
    TArray<float> PingSamples;

    // === 타이머 핸들들 ===

    // 성능 모니터링 타이머
    FTimerHandle PerformanceMonitoringTimer;

    // 통계 업데이트 타이머
    FTimerHandle StatisticsUpdateTimer;

    // 가비지 컬렉션 타이머
    FTimerHandle GarbageCollectionTimer;

    // 보스 체력 업데이트 타이머
    FTimerHandle BossHealthUpdateTimer;

    // === 설정 가능한 매개변수들 ===

    // 최소 플레이어 수 (게임 시작)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Configuration")
    int32 MinimumPlayersToStart;

    // 최대 플레이어 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Configuration")
    int32 MaximumPlayers;

    // 게임 제한 시간 (초, 0이면 무제한)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Configuration")
    float GameTimeLimit;

    // 성능 모니터링 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    float PerformanceMonitoringInterval;

    // FPS 샘플 크기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    int32 FPSSampleSize;

    // 핑 샘플 크기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    int32 PingSampleSize;

    // 자동 가비지 컬렉션 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    float GarbageCollectionInterval;

private:
    // === 내부 헬퍼 함수들 ===

    // 시스템 초기화
    void InitializeSystems();

    // 성능 모니터링 초기화
    void InitializePerformanceMonitoring();

    // 타이머 설정
    void SetupTimers();

    // 게임 페이즈 전환 처리
    void ProcessGamePhaseTransition(EHSGamePhase OldPhase, EHSGamePhase NewPhase);

    // 승리 조건 확인
    bool CheckVictoryCondition() const;

    // 패배 조건 확인
    bool CheckDefeatCondition() const;

    // === 타이머 콜백 함수들 ===

    // 성능 모니터링 업데이트
    UFUNCTION()
    void UpdatePerformanceMonitoring();

    // 통계 업데이트
    UFUNCTION()
    void UpdateStatistics();

    // 가비지 컬렉션 실행
    UFUNCTION()
    void PerformGarbageCollection();

    // 보스 체력 업데이트
    UFUNCTION()
    void UpdateBossHealth();

    // === 네트워크 복제 콜백 함수들 ===

    UFUNCTION()
    void OnRep_CurrentGamePhase();

    UFUNCTION()
    void OnRep_GameStatistics();

    UFUNCTION()
    void OnRep_WorldState();

    // === 디버그 및 로깅 함수들 ===

    // 게임 상태 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogGameState() const;

    // 성능 통계 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogPerformanceStats() const;

    // === 메모리 최적화 관련 ===

    // 사용하지 않는 참조 정리
    void CleanupUnusedReferences();

    // 메모리 풀 관리
    void ManageMemoryPools();

    // 오브젝트 풀 최적화
    void OptimizeObjectPools();

    // 초기화 완료 플래그
    bool bSystemsInitialized;

    // 성능 모니터링 활성화 플래그
    bool bPerformanceMonitoringEnabled;

    // 스레드 안전성을 위한 뮤텍스
    mutable FCriticalSection StatisticsMutex;
    mutable FCriticalSection PerformanceMutex;
};
