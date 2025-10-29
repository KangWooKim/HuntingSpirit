// 사냥의 영혼(HuntingSpirit) 게임의 게임 상태 클래스 구현
// 전체 게임 상태를 관리하고 네트워크 복제를 담당하는 클래스 구현

#include "HSGameStateBase.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HuntingSpirit/Characters/Stats/HSStatsComponent.h"
#include "HuntingSpirit/Enemies/Bosses/HSBossBase.h"
#include "HuntingSpirit/Optimization/HSPerformanceOptimizer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Stats/Stats.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"

// 생성자
AHSGameStateBase::AHSGameStateBase()
{
    // 기본값 설정
    CurrentGamePhase = EHSGamePhase::GP_WaitingForPlayers;
    GameStatistics = FHSGameStateStatistics();
    WorldState = FHSWorldState();

    // 시스템 컴포넌트 초기화
    TeamManager = nullptr;
    CoopMechanics = nullptr;
    SharedAbilitySystem = nullptr;
    PerformanceOptimizer = nullptr;

    // 성능 모니터링 변수 초기화
    CurrentFPS = 60.0f;
    CurrentMemoryUsage = 0.0f;
    AverageNetworkPing = 0.0f;

    // 설정 가능한 매개변수 기본값
    MinimumPlayersToStart = 1; // 테스트를 위해 1명으로 설정
    MaximumPlayers = 4;
    GameTimeLimit = 0.0f; // 무제한
    PerformanceMonitoringInterval = 1.0f; // 1초마다
    FPSSampleSize = 30; // 30개 샘플
    PingSampleSize = 10; // 10개 샘플
    GarbageCollectionInterval = 300.0f; // 5분마다

    // 플래그 초기화
    bSystemsInitialized = false;
    bPerformanceMonitoringEnabled = true;

    // 네트워크 복제 설정
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f; // 10FPS로 제한하여 성능 최적화

    // 메모리 풀 초기화
    FPSSamples.Reserve(FPSSampleSize);
    PingSamples.Reserve(PingSampleSize);
}

// 게임 시작 시 호출
void AHSGameStateBase::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 시스템 초기화
    if (HasAuthority())
    {
        InitializeSystems();
        InitializePerformanceMonitoring();
        SetupTimers();

        // 게임 시작 시간 기록
        GameStatistics.GameStartTime = GetWorld()->GetTimeSeconds();

        UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 게임 상태 초기화 완료"));
    }
}

// 매 프레임 호출
void AHSGameStateBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 서버에서만 실행
    if (!HasAuthority())
    {
        return;
    }

    // 시스템들 틱 업데이트
    if (bSystemsInitialized)
    {
        if (CoopMechanics)
        {
            CoopMechanics->TickCoopMechanics(DeltaTime);
        }

        if (SharedAbilitySystem)
        {
            SharedAbilitySystem->TickSharedAbilities(DeltaTime);
        }
    }

    // 승패 조건 확인
    if (CurrentGamePhase == EHSGamePhase::GP_Exploration || CurrentGamePhase == EHSGamePhase::GP_BossEncounter)
    {
        if (CheckVictoryCondition())
        {
            SetGamePhase(EHSGamePhase::GP_Victory);
        }
        else if (CheckDefeatCondition())
        {
            SetGamePhase(EHSGamePhase::GP_Defeat);
        }
    }
}

// 게임 종료 시 호출
void AHSGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(PerformanceMonitoringTimer);
        TimerManager.ClearTimer(StatisticsUpdateTimer);
        TimerManager.ClearTimer(GarbageCollectionTimer);
        TimerManager.ClearTimer(BossHealthUpdateTimer);
    }

    // 시스템 정리
    if (CoopMechanics)
    {
        CoopMechanics->Shutdown();
    }

    if (SharedAbilitySystem)
    {
        SharedAbilitySystem->Shutdown();
    }

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 게임 상태 정리 완료"));

    Super::EndPlay(EndPlayReason);
}

// 네트워크 복제 설정
void AHSGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 게임 상태 복제
    DOREPLIFETIME(AHSGameStateBase, CurrentGamePhase);
    DOREPLIFETIME(AHSGameStateBase, GameStatistics);
    DOREPLIFETIME(AHSGameStateBase, WorldState);
}

// === 게임 페이즈 관리 ===

// 게임 페이즈 설정
void AHSGameStateBase::SetGamePhase(EHSGamePhase NewPhase, bool bForceChange)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSGameStateBase: 클라이언트에서 게임 페이즈 변경 시도"));
        return;
    }

    if (CurrentGamePhase == NewPhase && !bForceChange)
    {
        return;
    }

    EHSGamePhase OldPhase = CurrentGamePhase;
    CurrentGamePhase = NewPhase;

    // 페이즈 전환 처리
    ProcessGamePhaseTransition(OldPhase, NewPhase);

    // 이벤트 브로드캐스트
    OnGamePhaseChanged.Broadcast(OldPhase, NewPhase);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 게임 페이즈 변경 - %d -> %d"), 
           (int32)OldPhase, (int32)NewPhase);
}

// 게임 진행 중 여부 확인
bool AHSGameStateBase::IsGameInProgress() const
{
    return CurrentGamePhase == EHSGamePhase::GP_Exploration || 
           CurrentGamePhase == EHSGamePhase::GP_BossEncounter;
}

// 게임 종료 여부 확인
bool AHSGameStateBase::IsGameEnded() const
{
    return CurrentGamePhase == EHSGamePhase::GP_Victory || 
           CurrentGamePhase == EHSGamePhase::GP_Defeat || 
           CurrentGamePhase == EHSGamePhase::GP_GameEnd;
}

// === 플레이어 관리 ===

// 플레이어 참여 처리
void AHSGameStateBase::OnPlayerJoined(AHSCharacterBase* JoiningPlayer)
{
    if (!HasAuthority() || !JoiningPlayer)
    {
        return;
    }

    // 스레드 안전성을 위한 뮤텍스 락
    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.TotalPlayers++;
    GameStatistics.AlivePlayers++;

    // 이벤트 브로드캐스트
    OnPlayerCountChanged.Broadcast(GameStatistics.TotalPlayers);

    // 최소 인원 도달 확인
    if (CurrentGamePhase == EHSGamePhase::GP_WaitingForPlayers && 
        GameStatistics.TotalPlayers >= MinimumPlayersToStart)
    {
        SetGamePhase(EHSGamePhase::GP_Preparation);
    }

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 플레이어 참여 - %s (총 %d명)"), 
           *JoiningPlayer->GetName(), GameStatistics.TotalPlayers);
}

// 플레이어 떠남 처리
void AHSGameStateBase::OnPlayerLeft(AHSCharacterBase* LeavingPlayer)
{
    if (!HasAuthority() || !LeavingPlayer)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.TotalPlayers = FMath::Max(0, GameStatistics.TotalPlayers - 1);
    GameStatistics.AlivePlayers = FMath::Max(0, GameStatistics.AlivePlayers - 1);

    OnPlayerCountChanged.Broadcast(GameStatistics.TotalPlayers);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 플레이어 떠남 - %s (총 %d명)"), 
           *LeavingPlayer->GetName(), GameStatistics.TotalPlayers);
}

// 플레이어 사망 처리
void AHSGameStateBase::OnPlayerDied(AHSCharacterBase* DeadPlayer, AActor* Killer)
{
    if (!HasAuthority() || !DeadPlayer)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.AlivePlayers = FMath::Max(0, GameStatistics.AlivePlayers - 1);

    // 이벤트 브로드캐스트
    OnPlayerEliminated.Broadcast(DeadPlayer, Killer);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 플레이어 사망 - %s (생존자 %d명)"), 
           *DeadPlayer->GetName(), GameStatistics.AlivePlayers);
}

// 플레이어 부활 처리
void AHSGameStateBase::HandlePlayerRevived(AHSCharacterBase* RevivedPlayer, AHSCharacterBase* Reviver)
{
    if (!HasAuthority() || !RevivedPlayer)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.AlivePlayers++;
    GameStatistics.RevivalCount++;

    // 이벤트 브로드캐스트
    OnPlayerRevived.Broadcast(RevivedPlayer, Reviver);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 플레이어 부활 - %s (부활자: %s)"), 
           *RevivedPlayer->GetName(), Reviver ? *Reviver->GetName() : TEXT("Unknown"));
}

// === 보스 관리 ===

// 보스 스폰
AHSBossBase* AHSGameStateBase::SpawnBoss(TSubclassOf<AHSBossBase> BossClass, const FVector& SpawnLocation)
{
    if (!HasAuthority() || !BossClass)
    {
        return nullptr;
    }

    // 기존 보스가 있다면 제거
    if (AHSBossBase* ExistingBoss = GetCurrentBoss())
    {
        ExistingBoss->Destroy();
    }

    // 새 보스 스폰
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AHSBossBase* NewBoss = GetWorld()->SpawnActor<AHSBossBase>(BossClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    if (NewBoss)
    {
        WorldState.CurrentBoss = NewBoss;
        WorldState.BossHealthPercentage = 1.0f;

        // 보스 체력 업데이트 타이머 시작
        GetWorld()->GetTimerManager().SetTimer(BossHealthUpdateTimer, this, 
                                             &AHSGameStateBase::UpdateBossHealth, 0.5f, true);

        // 게임 페이즈를 보스 조우로 변경
        SetGamePhase(EHSGamePhase::GP_BossEncounter);

        // 이벤트 브로드캐스트
        OnBossSpawned.Broadcast(NewBoss, SpawnLocation);

        UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 보스 스폰 완료 - %s"), *NewBoss->GetName());
    }

    return NewBoss;
}

// 보스 패배 처리
void AHSGameStateBase::HandleBossDefeated(AHSBossBase* DefeatedBoss)
{
    if (!HasAuthority() || !DefeatedBoss)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.BossesKilled++;
    WorldState.CurrentBoss = nullptr;
    WorldState.BossHealthPercentage = 0.0f;

    // 보스 체력 업데이트 타이머 정지
    GetWorld()->GetTimerManager().ClearTimer(BossHealthUpdateTimer);

    // 이벤트 브로드캐스트
    OnBossDefeated.Broadcast(DefeatedBoss);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 보스 패배 - %s"), *DefeatedBoss->GetName());
}

// 현재 보스 가져오기
AHSBossBase* AHSGameStateBase::GetCurrentBoss() const
{
    return WorldState.CurrentBoss.IsValid() ? WorldState.CurrentBoss.Get() : nullptr;
}

// === 게임 통계 관리 ===

// 적 처치 수 증가
void AHSGameStateBase::IncrementEnemyKill(AHSCharacterBase* KilledBy, bool bIsBoss)
{
    if (!HasAuthority())
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    if (bIsBoss)
    {
        GameStatistics.BossesKilled++;
    }
    else
    {
        GameStatistics.EnemiesKilled++;
    }

    OnGameStatisticsUpdated.Broadcast(GameStatistics);
}

// 데미지 통계 업데이트
void AHSGameStateBase::UpdateDamageStatistics(float DamageAmount, AHSCharacterBase* DamageDealer)
{
    if (!HasAuthority() || DamageAmount <= 0.0f)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.TotalDamageDealt += DamageAmount;
}

// 힐링 통계 업데이트
void AHSGameStateBase::UpdateHealingStatistics(float HealAmount, AHSCharacterBase* Healer)
{
    if (!HasAuthority() || HealAmount <= 0.0f)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.TotalHealingDone += HealAmount;
}

// 협동 액션 성공 수 증가
void AHSGameStateBase::IncrementCoopActionSuccess(const FName& ActionID, const TArray<AHSCharacterBase*>& Participants)
{
    if (!HasAuthority())
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.SuccessfulCoopActions++;

    OnGameStatisticsUpdated.Broadcast(GameStatistics);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 협동 액션 성공 - %s (참여자 %d명)"), 
           *ActionID.ToString(), Participants.Num());
}

// 자원 수집 통계 업데이트
void AHSGameStateBase::UpdateResourceStatistics(int32 ResourceAmount, AHSCharacterBase* Gatherer)
{
    if (!HasAuthority() || ResourceAmount <= 0)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);

    GameStatistics.ResourcesGathered += ResourceAmount;
}

// 게임 진행 시간 반환
float AHSGameStateBase::GetGameDuration() const
{
    if (GameStatistics.GameStartTime <= 0.0f)
    {
        return 0.0f;
    }

    return GetWorld()->GetTimeSeconds() - GameStatistics.GameStartTime;
}

// === 월드 상태 관리 ===

// 월드 시드 설정
void AHSGameStateBase::SetWorldSeed(int32 NewSeed)
{
    if (HasAuthority())
    {
        WorldState.WorldSeed = NewSeed;
        UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 월드 시드 설정 - %d"), NewSeed);
    }
}

// 환경 위험도 업데이트
void AHSGameStateBase::UpdateHazardLevel(float NewHazardLevel)
{
    if (HasAuthority())
    {
        WorldState.HazardLevel = FMath::Clamp(NewHazardLevel, 0.1f, 10.0f);
        UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 환경 위험도 업데이트 - %.2f"), WorldState.HazardLevel);
    }
}

// === 프라이빗 함수들 ===

// 시스템 초기화
void AHSGameStateBase::InitializeSystems()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("HSGameStateBase: GameInstance를 찾을 수 없습니다"));
        return;
    }

    // 팀 매니저 가져오기
    TeamManager = GameInstance->GetSubsystem<UHSTeamManager>();
    if (!TeamManager)
    {
        UE_LOG(LogTemp, Error, TEXT("HSGameStateBase: TeamManager를 찾을 수 없습니다"));
    }

    // 협동 메커니즘 시스템 생성
    CoopMechanics = NewObject<UHSCoopMechanics>(this);
    if (CoopMechanics && TeamManager)
    {
        // 공유 능력 시스템 생성
        SharedAbilitySystem = NewObject<UHSSharedAbilitySystem>(this);
        if (SharedAbilitySystem)
        {
            SharedAbilitySystem->Initialize(TeamManager);
            CoopMechanics->Initialize(TeamManager, SharedAbilitySystem);
        }
    }

    // 성능 최적화 시스템 생성
    PerformanceOptimizer = NewObject<UHSPerformanceOptimizer>(this);

    bSystemsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 시스템 초기화 완료"));
}

// 성능 모니터링 초기화
void AHSGameStateBase::InitializePerformanceMonitoring()
{
    if (!bPerformanceMonitoringEnabled)
    {
        return;
    }

    // FPS 및 핑 샘플 배열 초기화
    FPSSamples.Empty();
    FPSSamples.Reserve(FPSSampleSize);

    PingSamples.Empty();
    PingSamples.Reserve(PingSampleSize);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 성능 모니터링 초기화 완료"));
}

// 타이머 설정
void AHSGameStateBase::SetupTimers()
{
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();

    // 성능 모니터링 타이머
    if (bPerformanceMonitoringEnabled)
    {
        TimerManager.SetTimer(PerformanceMonitoringTimer, this, 
                            &AHSGameStateBase::UpdatePerformanceMonitoring, 
                            PerformanceMonitoringInterval, true);
    }

    // 통계 업데이트 타이머
    TimerManager.SetTimer(StatisticsUpdateTimer, this, 
                        &AHSGameStateBase::UpdateStatistics, 
                        5.0f, true);

    // 가비지 컬렉션 타이머
    TimerManager.SetTimer(GarbageCollectionTimer, this, 
                        &AHSGameStateBase::PerformGarbageCollection, 
                        GarbageCollectionInterval, true);

    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 타이머 설정 완료"));
}

// 게임 페이즈 전환 처리
void AHSGameStateBase::ProcessGamePhaseTransition(EHSGamePhase OldPhase, EHSGamePhase NewPhase)
{
    switch (NewPhase)
    {
        case EHSGamePhase::GP_Preparation:
            UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 준비 단계 시작"));
            // 월드 생성 등 준비 작업 수행
            break;

        case EHSGamePhase::GP_Exploration:
            UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 탐험 단계 시작"));
            // 적 스폰, 자원 배치 등
            break;

        case EHSGamePhase::GP_BossEncounter:
            UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 보스 조우 시작"));
            // 보스 관련 UI 활성화 등
            break;

        case EHSGamePhase::GP_Victory:
            UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 승리!"));
            // 승리 처리 및 보상 지급
            break;

        case EHSGamePhase::GP_Defeat:
            UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 패배!"));
            // 패배 처리
            break;

        case EHSGamePhase::GP_GameEnd:
            UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 게임 종료"));
            // 최종 정리 작업
            break;

        default:
            break;
    }
}

// 승리 조건 확인
bool AHSGameStateBase::CheckVictoryCondition() const
{
    // 모든 보스가 처치되었고 플레이어가 한 명 이상 생존
    return GameStatistics.BossesKilled > 0 && 
           GameStatistics.AlivePlayers > 0 && 
           !GetCurrentBoss();
}

// 패배 조건 확인
bool AHSGameStateBase::CheckDefeatCondition() const
{
    // 모든 플레이어가 사망했거나 제한 시간 초과
    bool bAllPlayersDead = GameStatistics.AlivePlayers <= 0 && GameStatistics.TotalPlayers > 0;
    bool bTimeOut = GameTimeLimit > 0.0f && GetGameDuration() >= GameTimeLimit;

    return bAllPlayersDead || bTimeOut;
}

// === 타이머 콜백 함수들 ===

// 성능 모니터링 업데이트
void AHSGameStateBase::UpdatePerformanceMonitoring()
{
    FScopeLock PerformanceLock(&PerformanceMutex);

    // FPS 계산
    float DeltaTime = GetWorld()->GetDeltaSeconds();
    if (DeltaTime > 0.0f)
    {
        float CurrentFrameRate = 1.0f / DeltaTime;
        FPSSamples.Add(CurrentFrameRate);

        // 샘플 크기 제한
        if (FPSSamples.Num() > FPSSampleSize)
        {
            FPSSamples.RemoveAt(0);
        }

        // 평균 FPS 계산
        float TotalFPS = 0.0f;
        for (float Sample : FPSSamples)
        {
            TotalFPS += Sample;
        }
        CurrentFPS = FPSSamples.Num() > 0 ? TotalFPS / FPSSamples.Num() : 60.0f;
    }

    // 메모리 사용량 계산
    FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
    CurrentMemoryUsage = MemoryStats.UsedPhysical / (1024.0f * 1024.0f); // MB 단위

    // 네트워크 핑 계산 (서버의 경우 클라이언트들의 평균 핑)
    if (UNetDriver* NetDriver = GetWorld()->GetNetDriver())
    {
        float TotalPing = 0.0f;
        int32 ClientCount = 0;

        for (UNetConnection* Connection : NetDriver->ClientConnections)
        {
            if (Connection && Connection->PlayerController)
            {
                TotalPing += Connection->AvgLag * 1000.0f; // 밀리초 단위
                ClientCount++;
            }
        }

        if (ClientCount > 0)
        {
            float CurrentPing = TotalPing / ClientCount;
            PingSamples.Add(CurrentPing);

            if (PingSamples.Num() > PingSampleSize)
            {
                PingSamples.RemoveAt(0);
            }

            float TotalPingSample = 0.0f;
            for (float Sample : PingSamples)
            {
                TotalPingSample += Sample;
            }
            AverageNetworkPing = PingSamples.Num() > 0 ? TotalPingSample / PingSamples.Num() : 0.0f;
        }
    }
}

// 통계 업데이트
void AHSGameStateBase::UpdateStatistics()
{
    // 플레이어 수 재계산 (정확성을 위해)
    TArray<AHSCharacterBase*> AllPlayers;
    for (TActorIterator<AHSCharacterBase> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        if (AHSCharacterBase* Player = *ActorItr)
        {
            AllPlayers.Add(Player);
        }
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    GameStatistics.TotalPlayers = AllPlayers.Num();
    
    // 생존 플레이어 수 계산
    int32 AliveCount = 0;
    for (AHSCharacterBase* Player : AllPlayers)
    {
        if (Player && IsValid(Player))
        {
            // 체력이 0보다 크면 생존으로 간주
            if (UHSStatsComponent* StatsComp = Player->FindComponentByClass<UHSStatsComponent>())
            {
                if (StatsComp->GetCurrentHealth() > 0.0f)
                {
                    AliveCount++;
                }
            }
            else
            {
                // StatsComponent가 없으면 기본적으로 생존으로 간주
                AliveCount++;
            }
        }
    }
    GameStatistics.AlivePlayers = AliveCount;

    // 통계 업데이트 이벤트 브로드캐스트
    OnGameStatisticsUpdated.Broadcast(GameStatistics);
}

// 가비지 컬렉션 실행
void AHSGameStateBase::PerformGarbageCollection()
{
    // 사용하지 않는 참조 정리
    CleanupUnusedReferences();

    // 메모리 풀 관리
    ManageMemoryPools();

    // 오브젝트 풀 최적화
    OptimizeObjectPools();

    // 강제 가비지 컬렉션 (주의: 성능에 영향을 줄 수 있음)
    GEngine->ForceGarbageCollection(true);

    UE_LOG(LogTemp, Verbose, TEXT("HSGameStateBase: 가비지 컬렉션 수행 완료"));
}

// 보스 체력 업데이트
void AHSGameStateBase::UpdateBossHealth()
{
    AHSBossBase* CurrentBoss = GetCurrentBoss();
    if (!CurrentBoss)
    {
        return;
    }

    float CurrentHealth = CurrentBoss->GetCurrentHealth();
    float MaxHealth = CurrentBoss->GetMaxHealth();

    if (MaxHealth > 0.0f)
    {
        WorldState.BossHealthPercentage = CurrentHealth / MaxHealth;
    }
    else
    {
        WorldState.BossHealthPercentage = 0.0f;
    }

    // 보스가 사망했다면 타이머 정지
    if (CurrentHealth <= 0.0f)
    {
        GetWorld()->GetTimerManager().ClearTimer(BossHealthUpdateTimer);
        HandleBossDefeated(CurrentBoss);
    }
}

// === 네트워크 복제 콜백 함수들 ===

void AHSGameStateBase::OnRep_CurrentGamePhase()
{
    UE_LOG(LogTemp, Log, TEXT("HSGameStateBase: 게임 페이즈 복제됨 - %d"), (int32)CurrentGamePhase);
}

void AHSGameStateBase::OnRep_GameStatistics()
{
    OnGameStatisticsUpdated.Broadcast(GameStatistics);
}

void AHSGameStateBase::OnRep_WorldState()
{
    // 월드 상태 변경에 대한 클라이언트 처리
}

// === 디버그 및 로깅 함수들 ===

// 게임 상태 로그 출력
void AHSGameStateBase::LogGameState() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 게임 상태 정보 ==="));
    UE_LOG(LogTemp, Warning, TEXT("현재 페이즈: %d"), (int32)CurrentGamePhase);
    UE_LOG(LogTemp, Warning, TEXT("총 플레이어: %d, 생존자: %d"), GameStatistics.TotalPlayers, GameStatistics.AlivePlayers);
    UE_LOG(LogTemp, Warning, TEXT("처치한 적: %d, 보스: %d"), GameStatistics.EnemiesKilled, GameStatistics.BossesKilled);
    UE_LOG(LogTemp, Warning, TEXT("총 데미지: %.1f, 총 힐링: %.1f"), GameStatistics.TotalDamageDealt, GameStatistics.TotalHealingDone);
    UE_LOG(LogTemp, Warning, TEXT("협동 액션 성공: %d"), GameStatistics.SuccessfulCoopActions);
    UE_LOG(LogTemp, Warning, TEXT("게임 진행 시간: %.1f초"), GetGameDuration());
}

// 성능 통계 로그 출력
void AHSGameStateBase::LogPerformanceStats() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 성능 통계 ==="));
    UE_LOG(LogTemp, Warning, TEXT("평균 FPS: %.1f"), CurrentFPS);
    UE_LOG(LogTemp, Warning, TEXT("메모리 사용량: %.1f MB"), CurrentMemoryUsage);
    UE_LOG(LogTemp, Warning, TEXT("평균 핑: %.1f ms"), AverageNetworkPing);
}

// === 메모리 최적화 관련 ===

// 사용하지 않는 참조 정리
void AHSGameStateBase::CleanupUnusedReferences()
{
    // 무효한 보스 참조 정리
    if (WorldState.CurrentBoss.IsValid() && !IsValid(WorldState.CurrentBoss.Get()))
    {
        WorldState.CurrentBoss = nullptr;
    }

    // 시스템 컴포넌트 유효성 확인
    if (TeamManager && !IsValid(TeamManager))
    {
        TeamManager = nullptr;
    }

    if (CoopMechanics && !IsValid(CoopMechanics))
    {
        CoopMechanics = nullptr;
    }

    if (SharedAbilitySystem && !IsValid(SharedAbilitySystem))
    {
        SharedAbilitySystem = nullptr;
    }
}

// 메모리 풀 관리
void AHSGameStateBase::ManageMemoryPools()
{
    // FPS 샘플 메모리 관리
    if (FPSSamples.Num() > FPSSampleSize * 2)
    {
        FPSSamples.RemoveAt(0, FPSSamples.Num() - FPSSampleSize);
        FPSSamples.Shrink();
    }

    // 핑 샘플 메모리 관리
    if (PingSamples.Num() > PingSampleSize * 2)
    {
        PingSamples.RemoveAt(0, PingSamples.Num() - PingSampleSize);
        PingSamples.Shrink();
    }
}

// 오브젝트 풀 최적화
void AHSGameStateBase::OptimizeObjectPools()
{
    if (CoopMechanics)
    {
        CoopMechanics->RequestCacheInvalidation();
    }

    const int32 ActiveBossCount = WorldState.CurrentBoss.IsValid() ? 1 : 0;
    const int32 ExpectedActiveObjects = FMath::Max(GameStatistics.TotalPlayers + WorldState.SpawnedEnemies + ActiveBossCount, 1);
    UHSPerformanceOptimizer::PreallocateMemoryPools(ExpectedActiveObjects);

    if (GameStatistics.TotalPlayers == 0)
    {
        UHSAdvancedMemoryManager::CleanupAllPools();
    }

    FPSSamples.Shrink();
    PingSamples.Shrink();
}
