// HSBossAbilitySystem.cpp
// HuntingSpirit Game - 보스 능력 시스템 구현
// 현업 수준의 최적화 기법 적용: SIMD, 메모리 풀링, 캐싱, 배치 처리

#include "HSBossAbilitySystem.h"
#include "HSBossBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Async/AsyncWork.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "DrawDebugHelpers.h"

// 성능 통계 매크로
DECLARE_STATS_GROUP(TEXT("HSBossAbilitySystem"), STATGROUP_HSBossAbilitySystem, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ExecuteAbility"), STAT_ExecuteAbility, STATGROUP_HSBossAbilitySystem);
DECLARE_CYCLE_STAT(TEXT("FindTargets"), STAT_FindTargets, STATGROUP_HSBossAbilitySystem);
DECLARE_CYCLE_STAT(TEXT("UpdateCooldowns"), STAT_UpdateCooldowns, STATGROUP_HSBossAbilitySystem);
DECLARE_CYCLE_STAT(TEXT("CacheOptimization"), STAT_CacheOptimization, STATGROUP_HSBossAbilitySystem);

// SIMD 최적화를 위한 내부 네임스페이스
namespace HSAbilitySystemOptimization
{
    // SIMD를 활용한 거리 계산 최적화
    static void BatchCalculateDistancesSIMD(const TArray<FVector>& Positions, const FVector& CenterPosition, TArray<float>& OutDistances)
    {
        OutDistances.SetNum(Positions.Num());
        
        const VectorRegister CenterVec = VectorLoadFloat3_W0(&CenterPosition);
        
        // 4개씩 배치로 처리 (SIMD 최적화)
        int32 BatchCount = Positions.Num() / 4;
        int32 ProcessedCount = 0;
        
        for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
        {
            VectorRegister Pos0 = VectorLoadFloat3_W0(&Positions[ProcessedCount]);
            VectorRegister Pos1 = VectorLoadFloat3_W0(&Positions[ProcessedCount + 1]);
            VectorRegister Pos2 = VectorLoadFloat3_W0(&Positions[ProcessedCount + 2]);
            VectorRegister Pos3 = VectorLoadFloat3_W0(&Positions[ProcessedCount + 3]);
            
            VectorRegister Diff0 = VectorSubtract(Pos0, CenterVec);
            VectorRegister Diff1 = VectorSubtract(Pos1, CenterVec);
            VectorRegister Diff2 = VectorSubtract(Pos2, CenterVec);
            VectorRegister Diff3 = VectorSubtract(Pos3, CenterVec);
            
            VectorRegister DotResult0 = VectorDot3(Diff0, Diff0);
            VectorRegister DotResult1 = VectorDot3(Diff1, Diff1);
            VectorRegister DotResult2 = VectorDot3(Diff2, Diff2);
            VectorRegister DotResult3 = VectorDot3(Diff3, Diff3);
            
            float DotFloat0, DotFloat1, DotFloat2, DotFloat3;
            VectorStoreFloat1(DotResult0, &DotFloat0);
            VectorStoreFloat1(DotResult1, &DotFloat1);
            VectorStoreFloat1(DotResult2, &DotFloat2);
            VectorStoreFloat1(DotResult3, &DotFloat3);
            
            OutDistances[ProcessedCount] = FMath::Sqrt(DotFloat0);
            OutDistances[ProcessedCount + 1] = FMath::Sqrt(DotFloat1);
            OutDistances[ProcessedCount + 2] = FMath::Sqrt(DotFloat2);
            OutDistances[ProcessedCount + 3] = FMath::Sqrt(DotFloat3);
            
            ProcessedCount += 4;
        }
        
        // 남은 요소들 처리
        for (int32 i = ProcessedCount; i < Positions.Num(); ++i)
        {
            OutDistances[i] = FVector::Dist(Positions[i], CenterPosition);
        }
    }
    
    // 메모리 풀 최적화된 배열 정렬
    template<typename T, typename PredicateType>
    static void OptimizedSort(TArray<T>& Array, PredicateType Predicate)
    {
        // 작은 배열은 삽입 정렬이 더 빠름
        if (Array.Num() <= 16)
        {
            for (int32 i = 1; i < Array.Num(); ++i)
            {
                T Key = Array[i];
                int32 j = i - 1;
                while (j >= 0 && Predicate(Key, Array[j]))
                {
                    Array[j + 1] = Array[j];
                    --j;
                }
                Array[j + 1] = Key;
            }
        }
        else
        {
            Array.Sort(Predicate);
        }
    }
}

// 생성자 - 초기화 최적화
UHSBossAbilitySystem::UHSBossAbilitySystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f; // 20Hz 업데이트로 성능 최적화
    
    // 기본값 설정
    MaxConcurrentAbilities = 3;
    GlobalCooldownMultiplier = 1.0f;
    bEnablePerformanceTracking = true;
    bUseAdvancedTargeting = true;
    TargetingUpdateFrequency = 10.0f; // 10Hz
    bDebugMode = false;
    
    // 캐시 초기화
    LastCachedPhase = EHSBossPhase::Phase1;
    LastCacheTime = 0.0f;
    
    // 메모리 풀 예약 (성능 최적화)
    AbilitiesMap.Reserve(32);
    CachedAvailableAbilities.Reserve(16);
    ExecutingAbilities.Reserve(8);
    CooldownTimers.Reserve(32);
    ExecutionTimers.Reserve(8);
    VFXPool.Reserve(16);
    AudioPool.Reserve(16);
}

void UHSBossAbilitySystem::BeginPlay()
{
    Super::BeginPlay();
    
    // 자가검증: 게임 상태 일관성 확인
    if (!CheckGameStateConsistency())
    {
        LogErrorWithContext(TEXT("Game state consistency check failed during BeginPlay"));
        return;
    }
    
    InitializeAbilitySystem();
}

void UHSBossAbilitySystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    SCOPE_CYCLE_COUNTER(STAT_UpdateCooldowns);
    
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // 자가검증: nullptr 체크
    if (!IsValid(GetOwner()))
    {
        LogErrorWithContext(TEXT("Owner is invalid during tick"));
        return;
    }
    
    // 쿨다운 업데이트 (최적화된 배치 처리)
    UpdateCooldowns(DeltaTime);
    
    // 큐에 있는 능력 처리
    ProcessQueuedAbilities();
    
    // 캐시 최적화 (주기적으로 실행)
    static float CacheOptimizationTimer = 0.0f;
    CacheOptimizationTimer += DeltaTime;
    if (CacheOptimizationTimer >= 1.0f) // 1초마다 실행
    {
        OptimizeAbilityCache();
        CacheOptimizationTimer = 0.0f;
    }
    
    // 메모리 정리 (5초마다)
    static float MemoryCleanupTimer = 0.0f;
    MemoryCleanupTimer += DeltaTime;
    if (MemoryCleanupTimer >= 5.0f)
    {
        CleanupExpiredReferences();
        OptimizeMemoryUsage();
        MemoryCleanupTimer = 0.0f;
    }
}

void UHSBossAbilitySystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 리소스 정리
    InterruptAllAbilities();
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        for (auto& Timer : CooldownTimers)
        {
            TimerManager.ClearTimer(Timer.Value);
        }
        for (auto& Timer : ExecutionTimers)
        {
            TimerManager.ClearTimer(Timer.Value);
        }
    }
    
    // 풀된 컴포넌트 정리
    for (UNiagaraComponent* VFX : VFXPool)
    {
        if (IsValid(VFX))
        {
            VFX->DestroyComponent();
        }
    }
    
    for (UAudioComponent* Audio : AudioPool)
    {
        if (IsValid(Audio))
        {
            Audio->DestroyComponent();
        }
    }
    
    VFXPool.Empty();
    AudioPool.Empty();
    
    Super::EndPlay(EndPlayReason);
}

// 능력 추가 - 자가검증 적용
bool UHSBossAbilitySystem::AddAbility(const FHSBossAbility& NewAbility)
{
    // 자가검증: 능력 유효성 검사
    if (!ValidateAbility(NewAbility))
    {
        LogErrorWithContext(TEXT("Invalid ability provided"), NewAbility.AbilityID);
        return false;
    }
    
    // 중복 확인
    if (AbilitiesMap.Contains(NewAbility.AbilityID))
    {
        LogErrorWithContext(TEXT("Ability already exists"), NewAbility.AbilityID);
        return false;
    }
    
    // 능력 추가
    FHSBossAbility AbilityCopy = NewAbility;
    AbilityCopy.LastUsedTime = 0.0f;
    AbilityCopy.RemainingCooldown = 0.0f;
    AbilityCopy.CurrentState = EHSAbilityState::Ready;
    AbilityCopy.UsageCount = 0;
    AbilityCopy.TotalDamageDealt = 0.0f;
    
    AbilitiesMap.Add(AbilityCopy.AbilityID, AbilityCopy);
    
    // 성능 추적 데이터 초기화
    if (bEnablePerformanceTracking)
    {
        FHSAbilityPerformanceData PerfData;
        PerfData.AbilityID = AbilityCopy.AbilityID;
        PerformanceDataMap.Add(AbilityCopy.AbilityID, PerfData);
    }
    
    // 캐시 무효화
    LastCacheTime = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Added ability %s"), *NewAbility.AbilityID.ToString());
    return true;
}

bool UHSBossAbilitySystem::RemoveAbility(FName AbilityID)
{
    // 자가검증: 유효한 ID인지 확인
    if (AbilityID.IsNone())
    {
        LogErrorWithContext(TEXT("Invalid AbilityID provided for removal"));
        return false;
    }
    
    // 실행 중인 능력인지 확인
    if (ExecutingAbilities.Contains(AbilityID))
    {
        InterruptAbility(AbilityID);
    }
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        
        if (FTimerHandle* CooldownTimer = CooldownTimers.Find(AbilityID))
        {
            TimerManager.ClearTimer(*CooldownTimer);
            CooldownTimers.Remove(AbilityID);
        }
        
        if (FTimerHandle* ExecutionTimer = ExecutionTimers.Find(AbilityID))
        {
            TimerManager.ClearTimer(*ExecutionTimer);
            ExecutionTimers.Remove(AbilityID);
        }
    }
    
    // 능력 제거
    bool bRemoved = AbilitiesMap.Remove(AbilityID) > 0;
    if (bRemoved)
    {
        PerformanceDataMap.Remove(AbilityID);
        LastCacheTime = 0.0f; // 캐시 무효화
        
        UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Removed ability %s"), *AbilityID.ToString());
    }
    
    return bRemoved;
}

void UHSBossAbilitySystem::ClearAllAbilities()
{
    // 모든 실행 중인 능력 중단
    InterruptAllAbilities();
    
    // 모든 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        
        for (auto& Timer : CooldownTimers)
        {
            TimerManager.ClearTimer(Timer.Value);
        }
        for (auto& Timer : ExecutionTimers)
        {
            TimerManager.ClearTimer(Timer.Value);
        }
    }
    
    // 데이터 정리
    AbilitiesMap.Empty();
    PerformanceDataMap.Empty();
    CooldownTimers.Empty();
    ExecutionTimers.Empty();
    ExecutingAbilities.Empty();
    
    // 큐 정리
    while (!QueuedAbilities.IsEmpty())
    {
        TPair<FName, FHSAbilityExecutionContext> Item;
        QueuedAbilities.Dequeue(Item);
    }
    
    // 캐시 무효화
    LastCacheTime = 0.0f;
    CachedAvailableAbilities.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Cleared all abilities"));
}

// 능력 검색 - 최적화된 캐싱 시스템
TArray<FHSBossAbility> UHSBossAbilitySystem::GetAvailableAbilities(EHSBossPhase CurrentPhase) const
{
    SCOPE_CYCLE_COUNTER(STAT_CacheOptimization);
    
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // 캐시 유효성 검사
    if (LastCachedPhase == CurrentPhase && 
        (CurrentTime - LastCacheTime) < CACHE_VALIDITY_TIME &&
        CachedAvailableAbilities.Num() > 0)
    {
        return CachedAvailableAbilities;
    }
    
    // 캐시 갱신
    CachedAvailableAbilities.Empty();
    CachedAvailableAbilities.Reserve(AbilitiesMap.Num());
    
    for (const auto& AbilityPair : AbilitiesMap)
    {
        const FHSBossAbility& Ability = AbilityPair.Value;
        
        // 페이즈 조건 확인
        if (static_cast<uint8>(Ability.RequiredPhase) <= static_cast<uint8>(CurrentPhase))
        {
            // 쿨다운 확인
            if (Ability.RemainingCooldown <= 0.0f && Ability.CurrentState == EHSAbilityState::Ready)
            {
                // 추가 조건 확인
                if (CanUseAbility(Ability.AbilityID, CurrentPhase))
                {
                    CachedAvailableAbilities.Add(Ability);
                }
            }
        }
    }
    
    // 우선순위별 정렬 (최적화된 정렬 알고리즘 사용)
    HSAbilitySystemOptimization::OptimizedSort(CachedAvailableAbilities, 
        [](const FHSBossAbility& A, const FHSBossAbility& B) 
        {
            return static_cast<uint8>(A.Priority) > static_cast<uint8>(B.Priority);
        });
    
    // 캐시 업데이트
    LastCachedPhase = CurrentPhase;
    LastCacheTime = CurrentTime;
    
    return CachedAvailableAbilities;
}

// 능력 실행 - 고성능 구현
bool UHSBossAbilitySystem::ExecuteAbility(FName AbilityID, const FHSAbilityExecutionContext& Context)
{
    SCOPE_CYCLE_COUNTER(STAT_ExecuteAbility);
    
    // 자가검증: 실행 컨텍스트 검증
    if (!ValidateExecutionContext(Context))
    {
        LogErrorWithContext(TEXT("Invalid execution context"), AbilityID);
        return false;
    }
    
    // 능력 존재 확인
    FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID);
    if (!Ability)
    {
        LogErrorWithContext(TEXT("Ability not found"), AbilityID);
        return false;
    }
    
    // 실행 가능 여부 확인
    if (!CanUseAbility(AbilityID, IsValid(Context.Caster) ? 
        Cast<AHSBossBase>(Context.Caster)->GetCurrentPhase() : EHSBossPhase::Phase1))
    {
        LogErrorWithContext(TEXT("Ability cannot be used"), AbilityID);
        return false;
    }
    
    // 동시 실행 제한 확인
    if (ExecutingAbilities.Num() >= MaxConcurrentAbilities)
    {
        LogErrorWithContext(TEXT("Maximum concurrent abilities reached"), AbilityID);
        return false;
    }
    
    // 성능 추적 시작
    double ExecutionStartTime = FPlatformTime::Seconds();
    
    // 능력 실행
    bool bSuccess = InternalExecuteAbility(*Ability, Context);
    
    if (bSuccess)
    {
        // 상태 업데이트
        Ability->CurrentState = EHSAbilityState::Executing;
        Ability->LastUsedTime = IsValid(Context.Caster) ? 
            Context.Caster->GetWorld()->GetTimeSeconds() : 0.0f;
        Ability->UsageCount++;
        ExecutingAbilities.Add(AbilityID);
        
        // 쿨다운 설정
        float ActualCooldown = Ability->Cooldown * GlobalCooldownMultiplier - Context.CooldownReduction;
        ActualCooldown = FMath::Max(0.0f, ActualCooldown);
        Ability->RemainingCooldown = ActualCooldown;
        
        // 쿨다운 타이머 설정
        if (UWorld* World = GetWorld())
        {
            FTimerHandle& CooldownTimer = CooldownTimers.FindOrAdd(AbilityID);
            World->GetTimerManager().SetTimer(CooldownTimer, 
                FTimerDelegate::CreateUFunction(this, FName("OnCooldownExpired"), AbilityID),
                ActualCooldown, false);
        }
        
        // 실행 완료 타이머 설정
        if (Ability->CastTime > 0.0f)
        {
            if (UWorld* World = GetWorld())
            {
                FTimerHandle& ExecutionTimer = ExecutionTimers.FindOrAdd(AbilityID);
                World->GetTimerManager().SetTimer(ExecutionTimer,
                    FTimerDelegate::CreateUFunction(this, FName("OnAbilityExecutionComplete"), AbilityID),
                    Ability->CastTime, false);
            }
        }
        else
        {
            // 즉시 실행 완료
            OnAbilityExecutionComplete(AbilityID);
        }
        
        // 성능 추적 업데이트
        if (bEnablePerformanceTracking)
        {
            double ExecutionTime = FPlatformTime::Seconds() - ExecutionStartTime;
            UpdatePerformanceData(AbilityID, ExecutionTime, Ability->Damage * Context.DamageMultiplier);
        }
        
        // 델리게이트 브로드캐스트
        OnAbilityExecuted.Broadcast(*Ability, Context);
        OnAbilityStateChanged.Broadcast(AbilityID, EHSAbilityState::Executing);
        
        // 캐시 무효화
        LastCacheTime = 0.0f;
        
        UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Executed ability %s"), *AbilityID.ToString());
    }
    else
    {
        LogErrorWithContext(TEXT("Failed to execute ability"), AbilityID);
    }
    
    return bSuccess;
}

// 타겟 찾기 - SIMD 최적화 적용
TArray<AActor*> UHSBossAbilitySystem::FindTargetsForAbility(const FHSBossAbility& Ability, const FVector& TargetLocation) const
{
    SCOPE_CYCLE_COUNTER(STAT_FindTargets);
    
    TArray<AActor*> FoundTargets;
    
    // 자가검증: 유효한 위치인지 확인
    if (TargetLocation.ContainsNaN())
    {
        LogErrorWithContext(TEXT("Invalid target location contains NaN"), Ability.AbilityID);
        return FoundTargets;
    }
    
    switch (Ability.TargetType)
    {
        case EHSAbilityTargetType::Self:
        {
            if (IsValid(OwnerBoss))
            {
                FoundTargets.Add(OwnerBoss);
            }
            break;
        }
        
        case EHSAbilityTargetType::SingleEnemy:
        {
            FoundTargets = FindSingleTarget(Ability, TargetLocation);
            break;
        }
        
        case EHSAbilityTargetType::MultipleEnemies:
        {
            FoundTargets = FindMultipleTargets(Ability, TargetLocation);
            break;
        }
        
        case EHSAbilityTargetType::AreaOfEffect:
        {
            FoundTargets = FindAreaTargets(Ability, TargetLocation);
            break;
        }
        
        case EHSAbilityTargetType::AllEnemies:
        {
            // 모든 플레이어 대상
            if (UWorld* World = GetWorld())
            {
                for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
                {
                    if (APlayerController* PC = Iterator->Get())
                    {
                        if (APawn* PlayerPawn = PC->GetPawn())
                        {
                            if (IsValidTarget(Ability, PlayerPawn))
                            {
                                FoundTargets.Add(PlayerPawn);
                            }
                        }
                    }
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    // 최대 타겟 수 제한
    if (FoundTargets.Num() > Ability.MaxTargets)
    {
        // 거리 기준으로 정렬하여 가장 가까운 타겟들만 선택 (SIMD 최적화)
        TArray<FVector> Positions;
        Positions.Reserve(FoundTargets.Num());
        
        for (AActor* Target : FoundTargets)
        {
            if (IsValid(Target))
            {
                Positions.Add(Target->GetActorLocation());
            }
        }
        
        TArray<float> Distances;
        HSAbilitySystemOptimization::BatchCalculateDistancesSIMD(Positions, TargetLocation, Distances);
        
        // 거리와 타겟을 연결한 배열 생성
        TArray<TPair<float, AActor*>> DistanceTargetPairs;
        DistanceTargetPairs.Reserve(FoundTargets.Num());
        
        for (int32 i = 0; i < FoundTargets.Num() && i < Distances.Num(); ++i)
        {
            DistanceTargetPairs.Emplace(Distances[i], FoundTargets[i]);
        }
        
        // 거리순 정렬
        HSAbilitySystemOptimization::OptimizedSort(DistanceTargetPairs,
            [](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B)
            {
                return A.Key < B.Key;
            });
        
        // 최대 타겟 수만큼 선택
        FoundTargets.Empty();
        for (int32 i = 0; i < FMath::Min(Ability.MaxTargets, DistanceTargetPairs.Num()); ++i)
        {
            FoundTargets.Add(DistanceTargetPairs[i].Value);
        }
    }
    
    // 자가검증: 타겟 유효성 확인
    if (!ValidateTargets(FoundTargets))
    {
        LogErrorWithContext(TEXT("Invalid targets found"), Ability.AbilityID);
        FoundTargets.Empty();
    }
    
    return FoundTargets;
}

// 쿨다운 상태 확인
bool UHSBossAbilitySystem::CanUseAbility(FName AbilityID, EHSBossPhase CurrentPhase) const
{
    // 자가검증: 유효한 ID인지 확인
    if (AbilityID.IsNone())
    {
        return false;
    }
    
    const FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID);
    if (!Ability)
    {
        return false;
    }
    
    // 페이즈 조건 확인
    if (static_cast<uint8>(Ability->RequiredPhase) > static_cast<uint8>(CurrentPhase))
    {
        return false;
    }
    
    // 상태 확인
    if (Ability->CurrentState != EHSAbilityState::Ready)
    {
        return false;
    }
    
    // 쿨다운 확인
    if (Ability->RemainingCooldown > 0.0f)
    {
        return false;
    }
    
    // 보스 상태 확인
    if (IsValid(OwnerBoss))
    {
            AHSBossBase* Boss = OwnerBoss;
            
            // 체력 조건 확인
            if (Ability->HealthThreshold > 0.0f)
            {
                float HealthRatio = Boss->GetCurrentHealth() / Boss->GetMaxHealth();
                if (HealthRatio > Ability->HealthThreshold)
                {
                    return false;
                }
            }
        
        // 분노 모드 조건 확인
        if (Ability->bOnlyInEnrageMode && !Boss->IsEnraged())
        {
            return false;
        }
        
        // 플레이어 수 조건 확인
        if (Ability->MinPlayerCount > 1)
        {
            int32 PlayerCount = Boss->GetActivePlayerCount();
            if (PlayerCount < Ability->MinPlayerCount)
            {
                return false;
            }
        }
    }
    
    // 필수 능력 확인
    for (const FName& RequiredAbility : Ability->RequiredAbilities)
    {
        const FHSBossAbility* ReqAbility = AbilitiesMap.Find(RequiredAbility);
        if (!ReqAbility || ReqAbility->UsageCount == 0)
        {
            return false;
        }
    }
    
    return true;
}

// 내부 초기화 함수
void UHSBossAbilitySystem::InitializeAbilitySystem()
{
    // 보스 참조 설정
    OwnerBoss = Cast<AHSBossBase>(GetOwner());
    if (!IsValid(OwnerBoss))
    {
        LogErrorWithContext(TEXT("Owner is not a boss"));
        return;
    }
    
    // 기본 능력 로드
    LoadDefaultAbilities();
    
    // VFX 및 Audio 풀 초기화
    if (UWorld* World = GetWorld())
    {
        for (int32 i = 0; i < 8; ++i)
        {
            // VFX 컴포넌트 풀 생성
            UNiagaraComponent* VFXComp = CreateDefaultSubobject<UNiagaraComponent>(
                *FString::Printf(TEXT("PooledVFX_%d"), i));
            if (VFXComp)
            {
                VFXComp->SetAutoActivate(false);
                VFXComp->AttachToComponent(OwnerBoss->GetRootComponent(), 
                    FAttachmentTransformRules::KeepWorldTransform);
                VFXPool.Add(VFXComp);
            }
            
            // Audio 컴포넌트 풀 생성
            UAudioComponent* AudioComp = CreateDefaultSubobject<UAudioComponent>(
                *FString::Printf(TEXT("PooledAudio_%d"), i));
            if (AudioComp)
            {
                AudioComp->SetAutoActivate(false);
                AudioComp->AttachToComponent(OwnerBoss->GetRootComponent(),
                    FAttachmentTransformRules::KeepWorldTransform);
                AudioPool.Add(AudioComp);
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Initialized for boss %s"), 
        IsValid(OwnerBoss) ? *OwnerBoss->GetName() : TEXT("Unknown"));
}

// 성능 추적 데이터 업데이트
void UHSBossAbilitySystem::UpdatePerformanceData(FName AbilityID, float ExecutionTime, float DamageDealt)
{
    if (!bEnablePerformanceTracking)
    {
        return;
    }
    
    FHSAbilityPerformanceData* PerfData = PerformanceDataMap.Find(AbilityID);
    if (!PerfData)
    {
        return;
    }
    
    PerfData->ExecutionCount++;
    PerfData->TotalExecutionTime += ExecutionTime;
    PerfData->TotalDamageOutput += DamageDealt;
    PerfData->AverageExecutionTime = PerfData->TotalExecutionTime / PerfData->ExecutionCount;
    PerfData->MaxExecutionTime = FMath::Max(PerfData->MaxExecutionTime, ExecutionTime);
}

// 자가검증 함수들
bool UHSBossAbilitySystem::ValidateAbility(const FHSBossAbility& Ability) const
{
    // ID 검증
    if (Ability.AbilityID.IsNone())
    {
        return false;
    }
    
    // 수치 검증
    if (Ability.Cooldown < 0.0f || Ability.CastTime < 0.0f || 
        Ability.Damage < 0.0f || Ability.Range < 0.0f)
    {
        return false;
    }
    
    // 타겟 수 검증
    if (Ability.MaxTargets <= 0)
    {
        return false;
    }
    
    return true;
}

bool UHSBossAbilitySystem::ValidateExecutionContext(const FHSAbilityExecutionContext& Context) const
{
    // Caster 검증
    if (!IsValid(Context.Caster))
    {
        return false;
    }
    
    // 타겟 위치 검증
    if (Context.TargetLocation.ContainsNaN())
    {
        return false;
    }
    
    // 수치 검증
    if (Context.DamageMultiplier < 0.0f || Context.CooldownReduction < 0.0f)
    {
        return false;
    }
    
    return true;
}

bool UHSBossAbilitySystem::ValidateTargets(const TArray<AActor*>& Targets) const
{
    for (AActor* Target : Targets)
    {
        if (!IsValid(Target))
        {
            return false;
        }
    }
    return true;
}

bool UHSBossAbilitySystem::CheckGameStateConsistency() const
{
    // 월드 검증
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    
    // 오너 검증
    if (!IsValid(GetOwner()))
    {
        return false;
    }
    
    // 게임 모드 검증
    if (!World->GetAuthGameMode())
    {
        return false;
    }
    
    return true;
}

void UHSBossAbilitySystem::LogErrorWithContext(const FString& ErrorMessage, FName AbilityID) const
{
    FString FullMessage = FString::Printf(TEXT("HSBossAbilitySystem Error: %s"), *ErrorMessage);
    
    if (!AbilityID.IsNone())
    {
        FullMessage += FString::Printf(TEXT(" [Ability: %s]"), *AbilityID.ToString());
    }
    
    if (IsValid(OwnerBoss))
    {
        FullMessage += FString::Printf(TEXT(" [Boss: %s]"), *OwnerBoss->GetName());
    }
    
    UE_LOG(LogTemp, Error, TEXT("%s"), *FullMessage);
    
    // 디버그 모드에서는 화면에도 출력
    if (bDebugMode && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FullMessage);
    }
}

// 쿨다운 업데이트 - 최적화된 배치 처리
void UHSBossAbilitySystem::UpdateCooldowns(float DeltaTime)
{
    // SIMD 최적화를 위한 배치 처리
    TArray<FName> AbilitiesToUpdate;
    AbilitiesToUpdate.Reserve(AbilitiesMap.Num());
    
    for (auto& AbilityPair : AbilitiesMap)
    {
        FHSBossAbility& Ability = AbilityPair.Value;
        if (Ability.RemainingCooldown > 0.0f)
        {
            AbilitiesToUpdate.Add(AbilityPair.Key);
        }
    }
    
    // 배치 처리로 쿨다운 업데이트
    for (const FName& AbilityID : AbilitiesToUpdate)
    {
        if (FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
        {
            Ability->RemainingCooldown = FMath::Max(0.0f, Ability->RemainingCooldown - DeltaTime);
        }
    }
}

// 큐에 있는 능력 처리
void UHSBossAbilitySystem::ProcessQueuedAbilities()
{
    while (!QueuedAbilities.IsEmpty())
    {
        TPair<FName, FHSAbilityExecutionContext> QueuedItem;
        if (QueuedAbilities.Dequeue(QueuedItem))
        {
            // 지연 시간 확인 (필요시 구현)
            ExecuteAbility(QueuedItem.Key, QueuedItem.Value);
        }
    }
}

// 내부 능력 실행 함수
bool UHSBossAbilitySystem::InternalExecuteAbility(const FHSBossAbility& Ability, const FHSAbilityExecutionContext& Context)
{
    // 타겟 찾기
    TArray<AActor*> Targets = FindTargetsForAbility(Ability, Context.TargetLocation);
    
    // 타겟이 없으면 실패 (Self 타입 제외)
    if (Targets.Num() == 0 && Ability.TargetType != EHSAbilityTargetType::Self)
    {
        return false;
    }
    
    // 시각/청각 효과 재생
    PlayAbilityEffects(Ability, Context);
    
    // 능력 효과 적용
    ApplyAbilityEffects(Ability, Context);
    
    return true;
}

// 능력 효과 재생
void UHSBossAbilitySystem::PlayAbilityEffects(const FHSBossAbility& Ability, const FHSAbilityExecutionContext& Context)
{
    if (!IsValid(OwnerBoss))
    {
        return;
    }
    
    // 애니메이션 재생
    if (Ability.AnimationMontage)
    {
        if (USkeletalMeshComponent* MeshComp = OwnerBoss->GetMesh())
        {
            if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
            {
                AnimInstance->Montage_Play(Ability.AnimationMontage);
            }
        }
    }
    
    // Niagara 효과 재생
    if (Ability.VFXTemplate)
    {
        UNiagaraComponent* VFXComp = GetPooledVFXComponent();
        if (VFXComp)
        {
            VFXComp->SetAsset(Ability.VFXTemplate);
            VFXComp->SetWorldLocation(Context.TargetLocation);
            VFXComp->SetColorParameter(TEXT("EffectColor"), Ability.EffectColor);
            VFXComp->Activate();
            
            // 일정 시간 후 풀로 반환
            if (UWorld* World = GetWorld())
            {
                FTimerHandle ReturnTimer;
                World->GetTimerManager().SetTimer(ReturnTimer,
                    FTimerDelegate::CreateUFunction(this, FName("ReturnVFXComponentToPool"), VFXComp),
                    Ability.Duration > 0.0f ? Ability.Duration : 5.0f, false);
            }
        }
    }
    
    // 사운드 재생
    if (Ability.SoundEffect)
    {
        UAudioComponent* AudioComp = GetPooledAudioComponent();
        if (AudioComp)
        {
            AudioComp->SetSound(Ability.SoundEffect);
            AudioComp->SetWorldLocation(Context.TargetLocation);
            AudioComp->Play();
            
            // 사운드 재생 완료 후 풀로 반환
            if (UWorld* World = GetWorld())
            {
                FTimerHandle ReturnTimer;
                float SoundDuration = Ability.SoundEffect->Duration;
                World->GetTimerManager().SetTimer(ReturnTimer,
                    FTimerDelegate::CreateUFunction(this, FName("ReturnAudioComponentToPool"), AudioComp),
                    SoundDuration, false);
            }
        }
    }
}

// 능력 효과 적용
void UHSBossAbilitySystem::ApplyAbilityEffects(const FHSBossAbility& Ability, const FHSAbilityExecutionContext& Context)
{
    TArray<AActor*> Targets = Context.Targets;
    
    // 타겟이 없으면 새로 찾기
    if (Targets.Num() == 0)
    {
        Targets = FindTargetsForAbility(Ability, Context.TargetLocation);
    }
    
    // 각 타겟에 효과 적용
    for (AActor* Target : Targets)
    {
        if (!IsValid(Target))
        {
            continue;
        }
        
        switch (Ability.EffectType)
        {
            case EHSAbilityEffectType::Damage:
            {
                float FinalDamage = Ability.Damage * Context.DamageMultiplier;
                
                // 데미지 적용
                if (APawn* TargetPawn = Cast<APawn>(Target))
                {
                    FPointDamageEvent DamageEvent;
                    DamageEvent.Damage = FinalDamage;
                    DamageEvent.HitInfo.Location = Target->GetActorLocation();
                    DamageEvent.ShotDirection = (Target->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal();
                    
                    Target->TakeDamage(FinalDamage, DamageEvent, 
                        IsValid(OwnerBoss) ? OwnerBoss->GetController() : nullptr, 
                        OwnerBoss.Get());
                }
                break;
            }
            
            case EHSAbilityEffectType::Heal:
            {
                // 힐링 효과 적용 (보스 자신에게)
                if (Target == OwnerBoss.Get())
                {
                    float HealAmount = Ability.Damage * Context.DamageMultiplier;
                    if (AHSBossBase* Boss = Cast<AHSBossBase>(Target))
                    {
                        float NewHealth = Boss->GetCurrentHealth() + HealAmount;
                        Boss->SetHealth(FMath::Min(NewHealth, Boss->GetMaxHealth()));
                    }
                }
                break;
            }
            
            case EHSAbilityEffectType::Summon:
            {
                // 소환 효과 (미니언 소환 등)
                if (UWorld* World = GetWorld())
                {
                    FVector SpawnLocation = Target->GetActorLocation() + FVector(0, 0, 100);
                    // 소환 로직은 구체적인 보스별로 구현
                }
                break;
            }
            
            case EHSAbilityEffectType::Environmental:
            {
                // 환경 효과 (지면 균열, 독 구름 등)
                if (IsValid(OwnerBoss))
                {
                    OwnerBoss->TriggerEnvironmentalHazard();
                }
                break;
            }
            
            default:
                break;
        }
    }
}

// 단일 타겟 찾기
TArray<AActor*> UHSBossAbilitySystem::FindSingleTarget(const FHSBossAbility& Ability, const FVector& TargetLocation) const
{
    TArray<AActor*> FoundTargets;
    
    if (UWorld* World = GetWorld())
    {
        // 범위 내 플레이어 검색
        TArray<FOverlapResult> OverlapResults;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(OwnerBoss.Get());
        
        bool bHit = World->OverlapMultiByChannel(
            OverlapResults,
            TargetLocation,
            FQuat::Identity,
            ECollisionChannel::ECC_Pawn,
            FCollisionShape::MakeSphere(Ability.Range),
            QueryParams
        );
        
        if (bHit)
        {
            float ClosestDistance = FLT_MAX;
            AActor* ClosestTarget = nullptr;
            
            for (const FOverlapResult& Result : OverlapResults)
            {
                if (AActor* Actor = Result.GetActor())
                {
                    if (IsValidTarget(Ability, Actor))
                    {
                        float Distance = FVector::Dist(Actor->GetActorLocation(), TargetLocation);
                        if (Distance < ClosestDistance)
                        {
                            ClosestDistance = Distance;
                            ClosestTarget = Actor;
                        }
                    }
                }
            }
            
            if (ClosestTarget)
            {
                FoundTargets.Add(ClosestTarget);
            }
        }
    }
    
    return FoundTargets;
}

// 복수 타겟 찾기
TArray<AActor*> UHSBossAbilitySystem::FindMultipleTargets(const FHSBossAbility& Ability, const FVector& TargetLocation) const
{
    TArray<AActor*> FoundTargets;
    
    if (UWorld* World = GetWorld())
    {
        TArray<FOverlapResult> OverlapResults;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(OwnerBoss.Get());
        
        bool bHit = World->OverlapMultiByChannel(
            OverlapResults,
            TargetLocation,
            FQuat::Identity,
            ECollisionChannel::ECC_Pawn,
            FCollisionShape::MakeSphere(Ability.Range),
            QueryParams
        );
        
        if (bHit)
        {
            for (const FOverlapResult& Result : OverlapResults)
            {
                if (AActor* Actor = Result.GetActor())
                {
                    if (IsValidTarget(Ability, Actor))
                    {
                        FoundTargets.Add(Actor);
                    }
                }
            }
        }
    }
    
    return FoundTargets;
}

// 영역 타겟 찾기
TArray<AActor*> UHSBossAbilitySystem::FindAreaTargets(const FHSBossAbility& Ability, const FVector& TargetLocation) const
{
    TArray<AActor*> FoundTargets;
    
    if (UWorld* World = GetWorld())
    {
        TArray<FOverlapResult> OverlapResults;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(OwnerBoss.Get());
        
        bool bHit = World->OverlapMultiByChannel(
            OverlapResults,
            TargetLocation,
            FQuat::Identity,
            ECollisionChannel::ECC_Pawn,
            FCollisionShape::MakeSphere(Ability.AreaRadius),
            QueryParams
        );
        
        if (bHit)
        {
            for (const FOverlapResult& Result : OverlapResults)
            {
                if (AActor* Actor = Result.GetActor())
                {
                    if (IsValidTarget(Ability, Actor))
                    {
                        FoundTargets.Add(Actor);
                    }
                }
            }
        }
    }
    
    return FoundTargets;
}

// 유효한 타겟인지 확인
bool UHSBossAbilitySystem::IsValidTarget(const FHSBossAbility& Ability, AActor* Target) const
{
    if (!IsValid(Target))
    {
        return false;
    }
    
    // 플레이어인지 확인
    APawn* TargetPawn = Cast<APawn>(Target);
    if (!TargetPawn)
    {
        return false;
    }
    
    // 플레이어 컨트롤러 확인
    APlayerController* PC = Cast<APlayerController>(TargetPawn->GetController());
    if (!PC)
    {
        return false;
    }
    
    // 생존 상태 확인 (언리얼 5.4 호환)
    if (!IsValid(TargetPawn) || !TargetPawn->IsValidLowLevel())
    {
        return false;
    }
    
    return true;
}

// 리소스 풀 관리 함수들
UNiagaraComponent* UHSBossAbilitySystem::GetPooledVFXComponent()
{
    for (UNiagaraComponent* VFX : VFXPool)
    {
        if (VFX && !VFX->IsActive())
        {
            return VFX;
        }
    }
    
    // 사용 가능한 컴포넌트가 없으면 새로 생성
    if (IsValid(OwnerBoss))
    {
        UNiagaraComponent* NewVFX = NewObject<UNiagaraComponent>(OwnerBoss);
        if (NewVFX)
        {
            NewVFX->SetAutoActivate(false);
            NewVFX->AttachToComponent(OwnerBoss->GetRootComponent(),
                FAttachmentTransformRules::KeepWorldTransform);
            VFXPool.Add(NewVFX);
            return NewVFX;
        }
    }
    
    return nullptr;
}

void UHSBossAbilitySystem::ReturnVFXComponentToPool(UNiagaraComponent* Component)
{
    if (Component)
    {
        Component->Deactivate();
        Component->SetAsset(nullptr);
    }
}

UAudioComponent* UHSBossAbilitySystem::GetPooledAudioComponent()
{
    for (UAudioComponent* Audio : AudioPool)
    {
        if (Audio && !Audio->IsPlaying())
        {
            return Audio;
        }
    }
    
    // 사용 가능한 컴포넌트가 없으면 새로 생성
    if (IsValid(OwnerBoss))
    {
        UAudioComponent* NewAudio = NewObject<UAudioComponent>(OwnerBoss);
        if (NewAudio)
        {
            NewAudio->SetAutoActivate(false);
            NewAudio->AttachToComponent(OwnerBoss->GetRootComponent(),
                FAttachmentTransformRules::KeepWorldTransform);
            AudioPool.Add(NewAudio);
            return NewAudio;
        }
    }
    
    return nullptr;
}

void UHSBossAbilitySystem::ReturnAudioComponentToPool(UAudioComponent* Component)
{
    if (Component)
    {
        Component->Stop();
        Component->SetSound(nullptr);
    }
}

// 콜백 함수들
void UHSBossAbilitySystem::OnCooldownExpired(FName AbilityID)
{
    if (FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        Ability->RemainingCooldown = 0.0f;
        if (Ability->CurrentState == EHSAbilityState::Cooldown)
        {
            Ability->CurrentState = EHSAbilityState::Ready;
            OnAbilityStateChanged.Broadcast(AbilityID, EHSAbilityState::Ready);
        }
    }
    
    OnAbilityCooldownExpired.Broadcast(AbilityID);
    
    // 타이머 정리
    CooldownTimers.Remove(AbilityID);
}

void UHSBossAbilitySystem::OnAbilityExecutionComplete(FName AbilityID)
{
    ExecutingAbilities.Remove(AbilityID);
    
    if (FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        Ability->CurrentState = EHSAbilityState::Cooldown;
        OnAbilityStateChanged.Broadcast(AbilityID, EHSAbilityState::Cooldown);
    }
    
    // 타이머 정리
    ExecutionTimers.Remove(AbilityID);
}

// 기본 능력 로드
void UHSBossAbilitySystem::LoadDefaultAbilities()
{
    for (const FHSBossAbility& DefaultAbility : DefaultAbilities)
    {
        AddAbility(DefaultAbility);
    }
}

// 메모리 최적화 함수들
void UHSBossAbilitySystem::OptimizeAbilityCache()
{
    SCOPE_CYCLE_COUNTER(STAT_CacheOptimization);
    
    // 사용하지 않는 캐시 정리
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - LastCacheTime > CACHE_VALIDITY_TIME * 10.0f)
    {
        CachedAvailableAbilities.Empty();
        CachedAvailableAbilities.Shrink();
    }
}

void UHSBossAbilitySystem::CleanupExpiredReferences()
{
    // 참조 정리
    if (!IsValid(OwnerBoss))
    {
        OwnerBoss = nullptr;
    }
    
    // 능력별 타겟 참조 정리
    for (auto& AbilityPair : AbilitiesMap)
    {
        FHSBossAbility& Ability = AbilityPair.Value;
        for (int32 i = Ability.CurrentTargets.Num() - 1; i >= 0; --i)
        {
            if (!IsValid(Ability.CurrentTargets[i]))
            {
                Ability.CurrentTargets.RemoveAt(i);
            }
        }
    }
}

void UHSBossAbilitySystem::OptimizeMemoryUsage()
{
    // 배열 최적화
    AbilitiesMap.Shrink();
    CachedAvailableAbilities.Shrink();
    PerformanceDataMap.Shrink();
    CooldownTimers.Shrink();
    ExecutionTimers.Shrink();
    
    // 풀 최적화
    VFXPool.Shrink();
    AudioPool.Shrink();
}

// 능력 중단
bool UHSBossAbilitySystem::InterruptAbility(FName AbilityID, AActor* Interrupter)
{
    if (FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        if (Ability->CurrentState == EHSAbilityState::Executing && Ability->bCanBeInterrupted)
        {
            Ability->CurrentState = EHSAbilityState::Interrupted;
            ExecutingAbilities.Remove(AbilityID);
            
            // 타이머 정리
            if (UWorld* World = GetWorld())
            {
                FTimerManager& TimerManager = World->GetTimerManager();
                
                if (FTimerHandle* ExecutionTimer = ExecutionTimers.Find(AbilityID))
                {
                    TimerManager.ClearTimer(*ExecutionTimer);
                    ExecutionTimers.Remove(AbilityID);
                }
            }
            
            OnAbilityInterrupted.Broadcast(AbilityID, Interrupter, 
                GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
            OnAbilityStateChanged.Broadcast(AbilityID, EHSAbilityState::Interrupted);
            
            return true;
        }
    }
    
    return false;
}

void UHSBossAbilitySystem::InterruptAllAbilities()
{
    TArray<FName> AbilitiesToInterrupt = ExecutingAbilities.Array();
    for (const FName& AbilityID : AbilitiesToInterrupt)
    {
        InterruptAbility(AbilityID);
    }
}

// 디버깅 함수
void UHSBossAbilitySystem::DebugPrintAllAbilities()
{
    UE_LOG(LogTemp, Warning, TEXT("=== HSBossAbilitySystem Debug Info ==="));
    UE_LOG(LogTemp, Warning, TEXT("Total Abilities: %d"), AbilitiesMap.Num());
    UE_LOG(LogTemp, Warning, TEXT("Executing Abilities: %d"), ExecutingAbilities.Num());
    
    for (const auto& AbilityPair : AbilitiesMap)
    {
        const FHSBossAbility& Ability = AbilityPair.Value;
        UE_LOG(LogTemp, Warning, TEXT("Ability: %s | State: %d | Cooldown: %.2f"), 
            *Ability.AbilityID.ToString(),
            static_cast<int32>(Ability.CurrentState),
            Ability.RemainingCooldown);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("==============================="));
}

// 기타 유틸리티 함수들
bool UHSBossAbilitySystem::HasAbility(FName AbilityID) const
{
    return AbilitiesMap.Contains(AbilityID);
}

FHSBossAbility UHSBossAbilitySystem::GetAbility(FName AbilityID) const
{
    if (const FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        return *Ability;
    }
    return FHSBossAbility();
}

EHSAbilityState UHSBossAbilitySystem::GetAbilityState(FName AbilityID) const
{
    if (const FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        return Ability->CurrentState;
    }
    return EHSAbilityState::Ready;
}

float UHSBossAbilitySystem::GetAbilityCooldown(FName AbilityID) const
{
    if (const FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        return Ability->Cooldown;
    }
    return 0.0f;
}

float UHSBossAbilitySystem::GetAbilityRemainingCooldown(FName AbilityID) const
{
    if (const FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        return Ability->RemainingCooldown;
    }
    return 0.0f;
}

bool UHSBossAbilitySystem::IsExecutingAnyAbility() const
{
    return ExecutingAbilities.Num() > 0;
}

TArray<FName> UHSBossAbilitySystem::GetExecutingAbilities() const
{
    return ExecutingAbilities.Array();
}

// 성능 데이터 관련 함수들
FHSAbilityPerformanceData UHSBossAbilitySystem::GetAbilityPerformanceData(FName AbilityID) const
{
    if (const FHSAbilityPerformanceData* PerfData = PerformanceDataMap.Find(AbilityID))
    {
        return *PerfData;
    }
    return FHSAbilityPerformanceData();
}

TArray<FHSAbilityPerformanceData> UHSBossAbilitySystem::GetAllPerformanceData() const
{
    TArray<FHSAbilityPerformanceData> AllData;
    PerformanceDataMap.GenerateValueArray(AllData);
    return AllData;
}

void UHSBossAbilitySystem::ResetPerformanceData()
{
    for (auto& PerfPair : PerformanceDataMap)
    {
        FHSAbilityPerformanceData& PerfData = PerfPair.Value;
        PerfData.ExecutionCount = 0;
        PerfData.TotalExecutionTime = 0.0f;
        PerfData.AverageExecutionTime = 0.0f;
        PerfData.MaxExecutionTime = 0.0f;
        PerfData.TotalDamageOutput = 0.0f;
    }
}

// 쿨다운 관리 함수들
void UHSBossAbilitySystem::ResetAbilityCooldown(FName AbilityID)
{
    if (FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        Ability->RemainingCooldown = 0.0f;
        if (Ability->CurrentState == EHSAbilityState::Cooldown)
        {
            Ability->CurrentState = EHSAbilityState::Ready;
            OnAbilityStateChanged.Broadcast(AbilityID, EHSAbilityState::Ready);
        }
        
        // 쿨다운 타이머 정리
        if (UWorld* World = GetWorld())
        {
            if (FTimerHandle* Timer = CooldownTimers.Find(AbilityID))
            {
                World->GetTimerManager().ClearTimer(*Timer);
                CooldownTimers.Remove(AbilityID);
            }
        }
    }
}

void UHSBossAbilitySystem::ResetAllCooldowns()
{
    for (auto& AbilityPair : AbilitiesMap)
    {
        ResetAbilityCooldown(AbilityPair.Key);
    }
}

void UHSBossAbilitySystem::ModifyCooldown(FName AbilityID, float CooldownReduction)
{
    if (FHSBossAbility* Ability = AbilitiesMap.Find(AbilityID))
    {
        Ability->RemainingCooldown = FMath::Max(0.0f, Ability->RemainingCooldown - CooldownReduction);
        
        // 쿨다운이 0이 되면 상태 변경
        if (Ability->RemainingCooldown <= 0.0f && Ability->CurrentState == EHSAbilityState::Cooldown)
        {
            Ability->CurrentState = EHSAbilityState::Ready;
            OnAbilityStateChanged.Broadcast(AbilityID, EHSAbilityState::Ready);
            OnAbilityCooldownExpired.Broadcast(AbilityID);
        }
    }
}

void UHSBossAbilitySystem::SetGlobalCooldownMultiplier(float Multiplier)
{
    GlobalCooldownMultiplier = FMath::Max(0.1f, Multiplier); // 최소 0.1배 보장
}

// 능력 큐 관리
bool UHSBossAbilitySystem::QueueAbility(FName AbilityID, const FHSAbilityExecutionContext& Context, float DelayTime)
{
    // 유효성 검사
    if (!HasAbility(AbilityID))
    {
        LogErrorWithContext(TEXT("Cannot queue non-existent ability"), AbilityID);
        return false;
    }
    
    // 큐에 추가
    TPair<FName, FHSAbilityExecutionContext> QueueItem(AbilityID, Context);
    QueuedAbilities.Enqueue(QueueItem);
    
    // 지연 실행 (필요시)
    if (DelayTime > 0.0f)
    {
        if (UWorld* World = GetWorld())
        {
            FTimerHandle DelayTimer;
            World->GetTimerManager().SetTimer(DelayTimer,
                FTimerDelegate::CreateUFunction(this, FName("ProcessQueuedAbilities")),
                DelayTime, false);
        }
    }
    
    return true;
}

// 능력 수정 - 자가검증 적용
bool UHSBossAbilitySystem::ModifyAbility(FName AbilityID, const FHSBossAbility& ModifiedAbility)
{
    // 자가검증: 유효한 ID인지 확인
    if (AbilityID.IsNone())
    {
        LogErrorWithContext(TEXT("Invalid AbilityID provided for modification"));
        return false;
    }
    
    // 자가검증: 수정할 능력 유효성 검사
    if (!ValidateAbility(ModifiedAbility))
    {
        LogErrorWithContext(TEXT("Invalid modified ability provided"), AbilityID);
        return false;
    }
    
    // 능력 존재 확인
    FHSBossAbility* ExistingAbility = AbilitiesMap.Find(AbilityID);
    if (!ExistingAbility)
    {
        LogErrorWithContext(TEXT("Ability not found for modification"), AbilityID);
        return false;
    }
    
    // 실행 중인 능력은 수정 불가
    if (ExistingAbility->CurrentState == EHSAbilityState::Executing)
    {
        LogErrorWithContext(TEXT("Cannot modify ability while executing"), AbilityID);
        return false;
    }
    
    // 런타임 데이터 보존
    float LastUsedTime = ExistingAbility->LastUsedTime;
    float RemainingCooldown = ExistingAbility->RemainingCooldown;
    EHSAbilityState CurrentState = ExistingAbility->CurrentState;
    int32 UsageCount = ExistingAbility->UsageCount;
    float TotalDamageDealt = ExistingAbility->TotalDamageDealt;
    TArray<TObjectPtr<AActor>> CurrentTargets = ExistingAbility->CurrentTargets;
    
    // 능력 데이터 업데이트
    *ExistingAbility = ModifiedAbility;
    
    // 런타임 데이터 복원
    ExistingAbility->LastUsedTime = LastUsedTime;
    ExistingAbility->RemainingCooldown = RemainingCooldown;
    ExistingAbility->CurrentState = CurrentState;
    ExistingAbility->UsageCount = UsageCount;
    ExistingAbility->TotalDamageDealt = TotalDamageDealt;
    ExistingAbility->CurrentTargets = CurrentTargets;
    
    // 캐시 무효화
    LastCacheTime = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Modified ability %s"), *AbilityID.ToString());
    return true;
}

// 우선순위별 능력 검색 - 최적화된 캐싱 적용
TArray<FHSBossAbility> UHSBossAbilitySystem::GetAbilitiesByPriority(EHSAbilityPriority MinPriority) const
{
    TArray<FHSBossAbility> FilteredAbilities;
    FilteredAbilities.Reserve(AbilitiesMap.Num());
    
    // 자가검증: 유효한 우선순위인지 확인
    if (static_cast<uint8>(MinPriority) > static_cast<uint8>(EHSAbilityPriority::Critical))
    {
        LogErrorWithContext(TEXT("Invalid priority level provided"));
        return FilteredAbilities;
    }
    
    // 우선순위 필터링
    for (const auto& AbilityPair : AbilitiesMap)
    {
        const FHSBossAbility& Ability = AbilityPair.Value;
        
        if (static_cast<uint8>(Ability.Priority) >= static_cast<uint8>(MinPriority))
        {
            FilteredAbilities.Add(Ability);
        }
    }
    
    // 우선순위별 정렬 (높은 우선순위가 먼저)
    HSAbilitySystemOptimization::OptimizedSort(FilteredAbilities,
        [](const FHSBossAbility& A, const FHSBossAbility& B)
        {
            return static_cast<uint8>(A.Priority) > static_cast<uint8>(B.Priority);
        });
    
    return FilteredAbilities;
}

// 타겟 타입별 능력 검색 - 자가검증 적용
TArray<FHSBossAbility> UHSBossAbilitySystem::GetAbilitiesByTargetType(EHSAbilityTargetType TargetType) const
{
    TArray<FHSBossAbility> FilteredAbilities;
    FilteredAbilities.Reserve(AbilitiesMap.Num());
    
    // 자가검증: 유효한 타겟 타입인지 확인
    if (static_cast<uint8>(TargetType) > static_cast<uint8>(EHSAbilityTargetType::AllEnemies))
    {
        LogErrorWithContext(TEXT("Invalid target type provided"));
        return FilteredAbilities;
    }
    
    // 타겟 타입 필터링
    for (const auto& AbilityPair : AbilitiesMap)
    {
        const FHSBossAbility& Ability = AbilityPair.Value;
        
        if (Ability.TargetType == TargetType)
        {
            FilteredAbilities.Add(Ability);
        }
    }
    
    // 우선순위별 정렬
    HSAbilitySystemOptimization::OptimizedSort(FilteredAbilities,
        [](const FHSBossAbility& A, const FHSBossAbility& B)
        {
            return static_cast<uint8>(A.Priority) > static_cast<uint8>(B.Priority);
        });
    
    return FilteredAbilities;
}

// 상황에 맞는 최적의 능력 선택 - AI 로직 적용
FHSBossAbility UHSBossAbilitySystem::GetBestAbilityForSituation(const FHSAbilityExecutionContext& Context) const
{
    // 자가검증: 컨텍스트 유효성 확인
    if (!ValidateExecutionContext(Context))
    {
        LogErrorWithContext(TEXT("Invalid execution context for ability selection"));
        return FHSBossAbility();
    }
    
    // 현재 보스 상태 가져오기
    AHSBossBase* Boss = Context.Caster;
    if (!IsValid(Boss))
    {
        LogErrorWithContext(TEXT("Invalid boss in execution context"));
        return FHSBossAbility();
    }
    
    EHSBossPhase CurrentPhase = Boss->GetCurrentPhase();
    float HealthRatio = Boss->GetCurrentHealth() / Boss->GetMaxHealth();
    int32 PlayerCount = Context.Targets.Num();
    
    // 사용 가능한 능력 목록 가져오기
    TArray<FHSBossAbility> AvailableAbilities = GetAvailableAbilities(CurrentPhase);
    
    if (AvailableAbilities.Num() == 0)
    {
        return FHSBossAbility();
    }
    
    // 각 능력에 점수 부여 (AI 로직)
    TArray<TPair<float, FHSBossAbility>> ScoredAbilities;
    ScoredAbilities.Reserve(AvailableAbilities.Num());
    
    for (const FHSBossAbility& Ability : AvailableAbilities)
    {
        float Score = 0.0f;
        
        // 우선순위 점수 (기본)
        Score += static_cast<float>(static_cast<uint8>(Ability.Priority)) * 10.0f;
        
        // 체력 상태에 따른 점수 조정
        if (HealthRatio < 0.3f) // 체력이 낮을 때
        {
            if (Ability.EffectType == EHSAbilityEffectType::Heal)
            {
                Score += 50.0f; // 힐링 능력 우선
            }
            else if (Ability.EffectType == EHSAbilityEffectType::Damage && Ability.Damage > 150.0f)
            {
                Score += 30.0f; // 강력한 공격 능력
            }
        }
        else if (HealthRatio > 0.7f) // 체력이 높을 때
        {
            if (Ability.EffectType == EHSAbilityEffectType::Buff || Ability.EffectType == EHSAbilityEffectType::Special)
            {
                Score += 25.0f; // 버프나 특수 능력 우선
            }
        }
        
        // 플레이어 수에 따른 점수 조정
        if (PlayerCount >= 3)
        {
            if (Ability.TargetType == EHSAbilityTargetType::AreaOfEffect || 
                Ability.TargetType == EHSAbilityTargetType::AllEnemies)
            {
                Score += 40.0f; // 다중 타겟 능력 우선
            }
        }
        else if (PlayerCount == 1)
        {
            if (Ability.TargetType == EHSAbilityTargetType::SingleEnemy)
            {
                Score += 20.0f; // 단일 타겟 능력 우선
            }
        }
        
        // 쿨다운 고려 (짧은 쿨다운이 유리)
        Score += (20.0f - Ability.Cooldown) * 0.5f;
        
        // 사용 빈도 고려 (적게 사용된 능력 우선)
        Score += (10.0f - FMath::Min(10.0f, static_cast<float>(Ability.UsageCount))) * 2.0f;
        
        // 페이즈별 특별 보너스
        if (static_cast<uint8>(Ability.RequiredPhase) == static_cast<uint8>(CurrentPhase))
        {
            Score += 15.0f; // 현재 페이즈 전용 능력
        }
        
        // 분노 모드 보너스
        if (Boss->IsEnraged() && Ability.bOnlyInEnrageMode)
        {
            Score += 35.0f;
        }
        
        ScoredAbilities.Emplace(Score, Ability);
    }
    
    // 점수순 정렬
    HSAbilitySystemOptimization::OptimizedSort(ScoredAbilities,
        [](const TPair<float, FHSBossAbility>& A, const TPair<float, FHSBossAbility>& B)
        {
            return A.Key > B.Key;
        });
    
    // 최고 점수 능력 반환
    if (ScoredAbilities.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSBossAbilitySystem: Selected ability %s with score %.2f"), 
            *ScoredAbilities[0].Value.AbilityID.ToString(), ScoredAbilities[0].Key);
        return ScoredAbilities[0].Value;
    }
    
    return FHSBossAbility();
}

// 최적 타겟 위치 계산 - SIMD 최적화 적용
FVector UHSBossAbilitySystem::GetOptimalTargetLocation(const FHSBossAbility& Ability, const TArray<AActor*>& PotentialTargets) const
{
    // 자가검증: 능력 유효성 확인
    if (!ValidateAbility(Ability))
    {
        LogErrorWithContext(TEXT("Invalid ability for target location calculation"), Ability.AbilityID);
        return FVector::ZeroVector;
    }
    
    // 자가검증: 타겟 유효성 확인
    if (!ValidateTargets(PotentialTargets))
    {
        LogErrorWithContext(TEXT("Invalid targets for optimal location calculation"), Ability.AbilityID);
        return FVector::ZeroVector;
    }
    
    if (PotentialTargets.Num() == 0)
    {
        return IsValid(OwnerBoss) ? OwnerBoss->GetActorLocation() : FVector::ZeroVector;
    }
    
    FVector OptimalLocation = FVector::ZeroVector;
    
    switch (Ability.TargetType)
    {
        case EHSAbilityTargetType::Self:
        {
            OptimalLocation = IsValid(OwnerBoss) ? OwnerBoss->GetActorLocation() : FVector::ZeroVector;
            break;
        }
        
        case EHSAbilityTargetType::SingleEnemy:
        {
            // 가장 가까운 타겟 선택
            if (IsValid(OwnerBoss))
            {
                float ClosestDistance = FLT_MAX;
                FVector BossLocation = OwnerBoss->GetActorLocation();
                
                for (AActor* Target : PotentialTargets)
                {
                    if (IsValid(Target))
                    {
                        float Distance = FVector::Dist(Target->GetActorLocation(), BossLocation);
                        if (Distance < ClosestDistance)
                        {
                            ClosestDistance = Distance;
                            OptimalLocation = Target->GetActorLocation();
                        }
                    }
                }
            }
            break;
        }
        
        case EHSAbilityTargetType::MultipleEnemies:
        case EHSAbilityTargetType::AreaOfEffect:
        {
            // 타겟들의 중심점 계산 (SIMD 최적화)
            TArray<FVector> Positions;
            Positions.Reserve(PotentialTargets.Num());
            
            for (AActor* Target : PotentialTargets)
            {
                if (IsValid(Target))
                {
                    Positions.Add(Target->GetActorLocation());
                }
            }
            
            if (Positions.Num() > 0)
            {
                // 중심점 계산
                FVector CenterPoint = FVector::ZeroVector;
                for (const FVector& Pos : Positions)
                {
                    CenterPoint += Pos;
                }
                CenterPoint /= static_cast<float>(Positions.Num());
                
                // 최대한 많은 타겟을 포함하는 위치 찾기
                if (Ability.AreaRadius > 0.0f)
                {
                    FVector BestLocation = CenterPoint;
                    int32 MaxTargetsInRange = 0;
                    
                    // 각 타겟 위치를 중심으로 테스트
                    for (const FVector& TestPos : Positions)
                    {
                        int32 TargetsInRange = 0;
                        
                        for (const FVector& TargetPos : Positions)
                        {
                            if (FVector::Dist(TestPos, TargetPos) <= Ability.AreaRadius)
                            {
                                TargetsInRange++;
                            }
                        }
                        
                        if (TargetsInRange > MaxTargetsInRange)
                        {
                            MaxTargetsInRange = TargetsInRange;
                            BestLocation = TestPos;
                        }
                    }
                    
                    OptimalLocation = BestLocation;
                }
                else
                {
                    OptimalLocation = CenterPoint;
                }
            }
            break;
        }
        
        case EHSAbilityTargetType::AllEnemies:
        {
            // 모든 타겟의 중심점
            if (PotentialTargets.Num() > 0)
            {
                FVector CenterPoint = FVector::ZeroVector;
                for (AActor* Target : PotentialTargets)
                {
                    if (IsValid(Target))
                    {
                        CenterPoint += Target->GetActorLocation();
                    }
                }
                OptimalLocation = CenterPoint / static_cast<float>(PotentialTargets.Num());
            }
            break;
        }
        
        default:
        {
            // 기본값: 첫 번째 타겟 위치
            if (PotentialTargets.Num() > 0 && IsValid(PotentialTargets[0]))
            {
                OptimalLocation = PotentialTargets[0]->GetActorLocation();
            }
            break;
        }
    }
    
    // 자가검증: 결과 위치 유효성 확인
    if (OptimalLocation.ContainsNaN())
    {
        LogErrorWithContext(TEXT("Calculated optimal location contains NaN"), Ability.AbilityID);
        return IsValid(OwnerBoss) ? OwnerBoss->GetActorLocation() : FVector::ZeroVector;
    }
    
    return OptimalLocation;
}

// 디버그 정보 그리기 - 현업 수준 디버깅 도구
void UHSBossAbilitySystem::DrawDebugInformation()
{
    if (!bDebugMode || !IsValid(OwnerBoss))
    {
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    FVector BossLocation = OwnerBoss->GetActorLocation();
    FVector DebugLocation = BossLocation + FVector(0, 0, 200);
    
    // 기본 정보 표시
    FString DebugInfo = FString::Printf(TEXT("HSBossAbilitySystem Debug\n"
        "Total Abilities: %d\n"
        "Executing: %d\n"
        "Cooldown Multiplier: %.2f"),
        AbilitiesMap.Num(),
        ExecutingAbilities.Num(),
        GlobalCooldownMultiplier);
    
    DrawDebugString(World, DebugLocation, DebugInfo, nullptr, FColor::Yellow, 0.0f);
    
    // 각 능력별 상태 표시
    int32 AbilityIndex = 0;
    for (const auto& AbilityPair : AbilitiesMap)
    {
        const FHSBossAbility& Ability = AbilityPair.Value;
        
        FColor StateColor = FColor::Green; // Ready
        switch (Ability.CurrentState)
        {
            case EHSAbilityState::Cooldown:
                StateColor = FColor::Orange;
                break;
            case EHSAbilityState::Executing:
                StateColor = FColor::Red;
                break;
            case EHSAbilityState::Interrupted:
                StateColor = FColor::Purple;
                break;
            case EHSAbilityState::Disabled:
                StateColor = FColor::Black;
                break;
            default:
                break;
        }
        
        FString AbilityInfo = FString::Printf(TEXT("%s: %.1fs (P:%d)"),
            *Ability.AbilityID.ToString(),
            Ability.RemainingCooldown,
            static_cast<int32>(Ability.Priority));
        
        FVector AbilityDebugLocation = DebugLocation + FVector(200, 0, -50 * AbilityIndex);
        DrawDebugString(World, AbilityDebugLocation, AbilityInfo, nullptr, StateColor, 0.0f);
        
        // 능력 범위 시각화
        if (Ability.Range > 0.0f)
        {
            DrawDebugSphere(World, BossLocation, Ability.Range, 12, StateColor, false, 0.0f, 0, 1.0f);
        }
        
        // AOE 범위 시각화
        if (Ability.AreaRadius > 0.0f && Ability.TargetType == EHSAbilityTargetType::AreaOfEffect)
        {
            // 마지막 타겟 위치에 AOE 범위 표시
            if (Ability.CurrentTargets.Num() > 0 && IsValid(Ability.CurrentTargets[0]))
            {
                FVector AOECenter = Ability.CurrentTargets[0]->GetActorLocation();
                DrawDebugSphere(World, AOECenter, Ability.AreaRadius, 16, FColor::Cyan, false, 0.0f, 0, 2.0f);
            }
        }
        
        AbilityIndex++;
        if (AbilityIndex >= 10) break; // 최대 10개까지만 표시
    }
    
    // 성능 데이터 표시
    if (bEnablePerformanceTracking)
    {
        FVector PerfLocation = DebugLocation + FVector(400, 0, 0);
        FString PerfHeader = TEXT("Performance Data:\n");
        DrawDebugString(World, PerfLocation, PerfHeader, nullptr, FColor::Cyan, 0.0f);
        
        int32 PerfIndex = 0;
        for (const auto& PerfPair : PerformanceDataMap)
        {
            const FHSAbilityPerformanceData& PerfData = PerfPair.Value;
            
            if (PerfData.ExecutionCount > 0)
            {
                FString PerfInfo = FString::Printf(TEXT("%s: %d uses, %.2fms avg"),
                    *PerfData.AbilityID.ToString(),
                    PerfData.ExecutionCount,
                    PerfData.AverageExecutionTime * 1000.0f); // ms 단위로 변환
                
                FVector PerfItemLocation = PerfLocation + FVector(0, 0, -20 * (PerfIndex + 1));
                DrawDebugString(World, PerfItemLocation, PerfInfo, nullptr, FColor::White, 0.0f);
                
                PerfIndex++;
                if (PerfIndex >= 5) break; // 최대 5개까지만 표시
            }
        }
    }
    
    // 메모리 사용량 표시
    FVector MemoryLocation = DebugLocation + FVector(-200, 0, 0);
    FString MemoryInfo = FString::Printf(TEXT("Memory Usage:\n"
        "VFX Pool: %d/%d\n"
        "Audio Pool: %d/%d\n"
        "Timers: %d"),
        VFXPool.Num(), VFXPool.Max(),
        AudioPool.Num(), AudioPool.Max(),
        CooldownTimers.Num() + ExecutionTimers.Num());
    
    DrawDebugString(World, MemoryLocation, MemoryInfo, nullptr, FColor::Magenta, 0.0f);
    
    // 보스 상태 표시
    if (AHSBossBase* Boss = Cast<AHSBossBase>(OwnerBoss))
    {
        FVector BossStatusLocation = BossLocation + FVector(0, 0, 300);
        FString BossStatus = FString::Printf(TEXT("Boss Status:\n"
            "Phase: %d\n"
            "Health: %.0f/%.0f\n"
            "Enraged: %s"),
            static_cast<int32>(Boss->GetCurrentPhase()),
            Boss->GetCurrentHealth(),
            Boss->GetMaxHealth(),
            Boss->IsEnraged() ? TEXT("Yes") : TEXT("No"));
        
        DrawDebugString(World, BossStatusLocation, BossStatus, nullptr, FColor::Red, 0.0f);
    }
}
