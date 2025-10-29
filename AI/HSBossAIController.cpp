/**
 * @file HSBossAIController.cpp
 * @brief 보스 캐릭터 전용 AI 컨트롤러 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#include "HuntingSpirit/AI/HSBossAIController.h"
#include "HuntingSpirit/Enemies/Bosses/HSBossBase.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HuntingSpirit/Characters/Stats/HSStatsComponent.h"
#include "HuntingSpirit/Core/PlayerState/HSPlayerState.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

/**
 * @brief 보스 AI 컨트롤러 생성자
 * @details 보스 전용 AI 설정값들을 초기화하고 네트워크 복제를 설정합니다
 */
AHSBossAIController::AHSBossAIController()
{
    // 기본 AI 상태 설정
    CurrentAIState = EBossAIState::Idle;
    TargetStrategy = EBossTargetStrategy::HighestThreat;
    
    // 타이머 설정
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f; // 성능 최적화를 위한 틱 간격
    
    // 네트워크 복제 설정
    SetReplicates(true);
    
    // 팀 ID 설정 (적 팀)
    SetGenericTeamId(FGenericTeamId(1));
}

/**
 * @brief 폰 빙의 시작 처리
 * @details 보스 캐릭터를 캐시하고 AI 시스템을 초기화합니다
 */
void AHSBossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    ControlledBoss = Cast<AHSBossBase>(InPawn);
    if (!ControlledBoss)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSBossAIController: 빙의된 폰이 HSBossBase가 아닙니다."));
        return;
    }
    
    // 보스 AI 초기화
    InitializeBossAI();
    
    // 인식 시스템 설정
    SetupPerceptionSystem();
    
    // 행동 트리 설정
    SetupBehaviorTree();
    
    // 초기 상태 설정
    SetAIState(EBossAIState::PatrolPhase);
    
    // 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(
        TargetUpdateTimer,
        this,
        &AHSBossAIController::UpdateTargetSelection,
        TargetSwitchInterval,
        true
    );
    
    GetWorld()->GetTimerManager().SetTimer(
        ThreatDecayTimer,
        this,
        &AHSBossAIController::UpdateThreatTable,
        0.5f,
        true
    );
    
    GetWorld()->GetTimerManager().SetTimer(
        EnvironmentScanTimer,
        this,
        &AHSBossAIController::ScanEnvironmentForTactics,
        2.0f,
        true
    );
}

/**
 * @brief 폰 빙의 해제 처리
 * @details 모든 타이머를 정리하고 참조를 해제합니다
 */
void AHSBossAIController::OnUnPossess()
{
    // 모든 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(TargetUpdateTimer);
    GetWorld()->GetTimerManager().ClearTimer(ThreatDecayTimer);
    GetWorld()->GetTimerManager().ClearTimer(PatternSelectionTimer);
    GetWorld()->GetTimerManager().ClearTimer(StateTransitionTimer);
    GetWorld()->GetTimerManager().ClearTimer(EnvironmentScanTimer);
    
    // 참조 정리
    ControlledBoss = nullptr;
    PrimaryTarget = nullptr;
    CurrentTargets.Empty();
    ThreatTable.Empty();
    TacticalEnvironmentActors.Empty();
    
    Super::OnUnPossess();
}

// 게임 시작
void AHSBossAIController::BeginPlay()
{
    Super::BeginPlay();
    
    // 페이즈별 설정 초기화
    PhaseAggressionLevels.Add(EHSBossPhase::Phase1, 0.5f);
    PhaseAggressionLevels.Add(EHSBossPhase::Phase2, 0.7f);
    PhaseAggressionLevels.Add(EHSBossPhase::Phase3, 0.9f);
    PhaseAggressionLevels.Add(EHSBossPhase::Enraged, 1.0f);
    PhaseAggressionLevels.Add(EHSBossPhase::Final, 1.2f);
    
    PhaseRetreatThresholds.Add(EHSBossPhase::Phase1, 0.3f);
    PhaseRetreatThresholds.Add(EHSBossPhase::Phase2, 0.2f);
    PhaseRetreatThresholds.Add(EHSBossPhase::Phase3, 0.1f);
    PhaseRetreatThresholds.Add(EHSBossPhase::Enraged, 0.0f);
    PhaseRetreatThresholds.Add(EHSBossPhase::Final, 0.0f);
}

// 매 프레임 업데이트
void AHSBossAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!ControlledBoss || CurrentAIState == EBossAIState::Defeated)
    {
        return;
    }
    
    // AI 업데이트
    UpdateAITick(DeltaTime);
    
    // 상태 전환 처리
    ProcessStateTransitions();
    
    // 디버그 정보 표시
    if (bDebugDrawPerception)
    {
        DrawDebugSphere(
            GetWorld(),
            GetPawn()->GetActorLocation(),
            BossSightRadius,
            32,
            FColor::Blue,
            false,
            -1.0f,
            0,
            2.0f
        );
    }
    
    if (bDebugDrawThreatLevels && PrimaryTarget)
    {
        float ThreatLevel = ThreatTable.Contains(PrimaryTarget) ? ThreatTable[PrimaryTarget] : 0.0f;
        DrawDebugString(
            GetWorld(),
            PrimaryTarget->GetActorLocation() + FVector(0, 0, 100),
            FString::Printf(TEXT("Threat: %.1f"), ThreatLevel),
            nullptr,
            FColor::Red,
            0.0f,
            true
        );
    }
}

// 보스 AI 초기화
void AHSBossAIController::InitializeBossAI()
{
    if (!ControlledBoss)
    {
        return;
    }
    
    // 보스 이벤트 바인딩
    ControlledBoss->OnBossPhaseChanged.AddDynamic(this, &AHSBossAIController::HandlePhaseTransition);
    
    // 사용 가능한 패턴 캐싱
    AvailablePatterns = ControlledBoss->GetAttackPatterns();
}

// 인식 시스템 설정
void AHSBossAIController::SetupPerceptionSystem()
{
    if (!AIPerceptionComponent)
    {
        return;
    }
    
    // 시각 설정 업데이트
    if (SightConfig)
    {
        SightConfig->SightRadius = BossSightRadius;
        SightConfig->LoseSightRadius = BossSightRadius * 1.2f;
        SightConfig->PeripheralVisionAngleDegrees = BossPeripheralVisionAngle;
        AIPerceptionComponent->ConfigureSense(*SightConfig);
    }
    
    // 청각 설정 업데이트
    if (HearingConfig)
    {
        HearingConfig->HearingRange = BossHearingRange;
        AIPerceptionComponent->ConfigureSense(*HearingConfig);
    }
    
    // 인식 이벤트 바인딩
    AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AHSBossAIController::OnTargetPerceptionUpdated);
    AIPerceptionComponent->OnTargetPerceptionForgotten.AddDynamic(this, &AHSBossAIController::OnTargetPerceptionForgotten);
}

// 행동 트리 설정
void AHSBossAIController::SetupBehaviorTree()
{
    // 기본 행동 트리는 부모 클래스에서 처리
    // 보스 전용 블랙보드 키 추가
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsEnum(TEXT("BossAIState"), (uint8)CurrentAIState);
        BlackboardComponent->SetValueAsFloat(TEXT("AggressionLevel"), 0.5f);
        BlackboardComponent->SetValueAsBool(TEXT("IsExecutingPattern"), false);
        BlackboardComponent->SetValueAsBool(TEXT("IsEnraged"), false);
        BlackboardComponent->SetValueAsInt(TEXT("ActivePlayerCount"), 0);
    }
}

// AI 상태 설정
void AHSBossAIController::SetAIState(EBossAIState NewState)
{
    if (CurrentAIState == NewState)
    {
        return;
    }
    
    EBossAIState OldState = CurrentAIState;
    CurrentAIState = NewState;
    
    // 블랙보드 업데이트
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsEnum(TEXT("BossAIState"), (uint8)CurrentAIState);
    }
    
    // 상태별 처리
    switch (CurrentAIState)
    {
    case EBossAIState::PatrolPhase:
        bIsInCombat = false;
        break;
        
    case EBossAIState::CombatPhase:
        bIsInCombat = true;
        if (!PatternSelectionTimer.IsValid())
        {
            GetWorld()->GetTimerManager().SetTimer(
                PatternSelectionTimer,
                this,
                &AHSBossAIController::ExecuteNextPattern,
                PatternSelectionInterval,
                true
            );
        }
        break;
        
    case EBossAIState::TransitionPhase:
        InterruptCurrentPattern();
        break;
        
    case EBossAIState::Enraged:
        if (ControlledBoss && !ControlledBoss->IsEnraged())
        {
            ControlledBoss->EnterEnrageMode();
        }
        break;
        
    case EBossAIState::Defeated:
        InterruptCurrentPattern();
        GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
        break;
    }
    
    // 상태 변경 브로드캐스트
    BroadcastAIEvents();
}

// 타겟 선택 업데이트
void AHSBossAIController::UpdateTargetSelection()
{
    if (!ControlledBoss || CurrentAIState == EBossAIState::Defeated)
    {
        return;
    }
    
    // 유효한 타겟 확인
    ValidateTargets();
    
    // 새로운 타겟 선택
    AActor* NewTarget = SelectTargetByStrategy();
    
    if (NewTarget != PrimaryTarget)
    {
        PrimaryTarget = NewTarget;
        
        // 블랙보드 업데이트
        if (BlackboardComponent)
        {
            BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), PrimaryTarget);
            
            if (PrimaryTarget)
            {
                LastKnownTargetLocation = PrimaryTarget->GetActorLocation();
                BlackboardComponent->SetValueAsVector(TEXT("LastKnownLocation"), LastKnownTargetLocation);
            }
        }
        
        // 전투 상태 전환
        if (PrimaryTarget && CurrentAIState == EBossAIState::PatrolPhase)
        {
            SetAIState(EBossAIState::CombatPhase);
        }
        else if (!PrimaryTarget && CurrentAIState == EBossAIState::CombatPhase)
        {
            SetAIState(EBossAIState::PatrolPhase);
        }
    }
}

// 타겟 전략 설정
void AHSBossAIController::SetTargetStrategy(EBossTargetStrategy NewStrategy)
{
    TargetStrategy = NewStrategy;
    bNeedsTargetUpdate = true;
}

// 다음 패턴 실행
void AHSBossAIController::ExecuteNextPattern()
{
    if (!ControlledBoss || bIsExecutingPattern || CurrentAIState != EBossAIState::CombatPhase)
    {
        return;
    }
    
    // 현재 페이즈에 맞는 패턴 선택
    EHSBossPhase CurrentPhase = ControlledBoss->GetCurrentPhase();
    FHSBossAttackPattern SelectedPattern = SelectPatternForPhase(CurrentPhase);
    
    // 패턴 조건 평가
    if (EvaluatePatternConditions(SelectedPattern))
    {
        bIsExecutingPattern = true;
        CurrentExecutingPattern = SelectedPattern;
        LastPatternExecutionTime = GetWorld()->GetTimeSeconds();
        
        // 블랙보드 업데이트
        if (BlackboardComponent)
        {
            BlackboardComponent->SetValueAsBool(TEXT("IsExecutingPattern"), true);
            BlackboardComponent->SetValueAsString(TEXT("CurrentPatternName"), SelectedPattern.PatternName.ToString());
        }
        
        // 보스에게 패턴 실행 요청
        ControlledBoss->ExecuteAttackPattern(SelectedPattern);
        
        // 패턴 종료 타이머 설정
        FTimerHandle PatternEndTimer;
        GetWorld()->GetTimerManager().SetTimer(
            PatternEndTimer,
            [this]()
            {
                bIsExecutingPattern = false;
                if (BlackboardComponent)
                {
                    BlackboardComponent->SetValueAsBool(TEXT("IsExecutingPattern"), false);
                }
            },
            SelectedPattern.ActivationTime + SelectedPattern.Cooldown,
            false
        );
        
        if (bDebugLogPatternSelection)
        {
            UE_LOG(LogTemp, Log, TEXT("보스 AI: 패턴 실행 - %s"), *SelectedPattern.PatternName.ToString());
        }
    }
}

// 현재 패턴 중단
void AHSBossAIController::InterruptCurrentPattern()
{
    if (!bIsExecutingPattern)
    {
        return;
    }
    
    bIsExecutingPattern = false;
    
    // 블랙보드 업데이트
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsBool(TEXT("IsExecutingPattern"), false);
    }
    
    // 보스에게 패턴 중단 알림
    if (ControlledBoss)
    {
        // 보스의 패턴 중단 처리
        ControlledBoss->OnBossPatternEnd.Broadcast(CurrentExecutingPattern);
    }
}

// 페이즈 전환 처리
void AHSBossAIController::HandlePhaseTransition(EHSBossPhase OldPhase, EHSBossPhase NewPhase)
{
    // 일시적으로 전환 상태로 변경
    SetAIState(EBossAIState::TransitionPhase);
    
    // 페이즈별 행동 업데이트
    float AggressionLevel = PhaseAggressionLevels.Contains(NewPhase) ? PhaseAggressionLevels[NewPhase] : 0.7f;
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsFloat(TEXT("AggressionLevel"), AggressionLevel);
    }
    
    // 패턴 재계산 필요
    bNeedsPatternRecalculation = true;
    
    // 전환 후 전투 상태로 복귀
    FTimerHandle TransitionTimer;
    GetWorld()->GetTimerManager().SetTimer(
        TransitionTimer,
        [this]()
        {
            if (CurrentAIState == EBossAIState::TransitionPhase)
            {
                SetAIState(EBossAIState::CombatPhase);
            }
        },
        StateTransitionDelay,
        false
    );
}

// 협동 위협 평가
void AHSBossAIController::EvaluateCoopThreat()
{
    if (!ControlledBoss)
    {
        return;
    }
    
    // 활성 플레이어 수 계산
    int32 ActivePlayerCount = CurrentTargets.Num();
    
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsInt(TEXT("ActivePlayerCount"), ActivePlayerCount);
    }
    
    // 협동 위협 수준 계산
    float CoopThreatLevel = 0.0f;
    
    // 근접한 플레이어들 간의 거리 확인
    for (int32 i = 0; i < CurrentTargets.Num(); i++)
    {
        for (int32 j = i + 1; j < CurrentTargets.Num(); j++)
        {
            float Distance = FVector::Dist(
                CurrentTargets[i]->GetActorLocation(),
                CurrentTargets[j]->GetActorLocation()
            );
            
            // 300 유닛 이내의 플레이어는 협동 중으로 간주
            if (Distance < 300.0f)
            {
                CoopThreatLevel += 1.0f;
            }
        }
    }
    
    // 협동 대응 필요 여부 판단
    if (CoopThreatLevel >= CoopThreatThreshold && !bCoopCounterMeasureActive)
    {
        FTimerHandle CoopResponseTimer;
        GetWorld()->GetTimerManager().SetTimer(
            CoopResponseTimer,
            this,
            &AHSBossAIController::TriggerCoopCounterMeasure,
            CoopResponseDelay,
            false
        );
        
        bCoopCounterMeasureActive = true;
    }
}

// 협동 대응 조치 발동
void AHSBossAIController::TriggerCoopCounterMeasure()
{
    if (!ControlledBoss)
    {
        return;
    }
    
    // 협동 메커니즘 발동
    ControlledBoss->TriggerCoopMechanic();
    
    // 타겟 전략 변경 (가장 가까운 그룹 공격)
    SetTargetStrategy(EBossTargetStrategy::NearestTarget);
    
    // 환경 전술 사용 확률 증가
    EnvironmentUsageChance = FMath::Min(EnvironmentUsageChance * 1.5f, 0.8f);
    
    // 쿨다운 설정
    FTimerHandle CoopCooldownTimer;
    GetWorld()->GetTimerManager().SetTimer(
        CoopCooldownTimer,
        [this]()
        {
            bCoopCounterMeasureActive = false;
            EnvironmentUsageChance = 0.3f;
        },
        10.0f,
        false
    );
}

// 환경 전술 스캔
void AHSBossAIController::ScanEnvironmentForTactics()
{
    if (!ControlledBoss || !bIsInCombat)
    {
        return;
    }
    
    TacticalEnvironmentActors.Empty();
    
    // 주변 액터 검색
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(ControlledBoss);
    
    GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        ControlledBoss->GetActorLocation(),
        FQuat::Identity,
        ECC_WorldStatic,
        FCollisionShape::MakeSphere(EnvironmentScanRadius),
        QueryParams
    );
    
    // 전술적 가치가 있는 오브젝트 필터링
    for (const FOverlapResult& Result : OverlapResults)
    {
        if (AActor* Actor = Result.GetActor())
        {
            // 파괴 가능한 오브젝트, 함정, 환경 위험 요소 등 확인
            if (Actor->ActorHasTag(TEXT("Destructible")) ||
                Actor->ActorHasTag(TEXT("EnvironmentalHazard")) ||
                Actor->ActorHasTag(TEXT("TacticalObject")))
            {
                TacticalEnvironmentActors.Add(Actor);
            }
        }
    }
    
    // 환경 사용 결정
    if (TacticalEnvironmentActors.Num() > 0 && FMath::RandRange(0.0f, 1.0f) < EnvironmentUsageChance)
    {
        ExecuteEnvironmentalTactic();
    }
}

// 환경 전술 실행
void AHSBossAIController::ExecuteEnvironmentalTactic()
{
    if (TacticalEnvironmentActors.Num() == 0 || !ControlledBoss)
    {
        return;
    }
    
    // 가장 가까운 전술 오브젝트 선택
    AActor* ClosestTacticalObject = nullptr;
    float ClosestDistance = FLT_MAX;
    
    for (AActor* TacticalActor : TacticalEnvironmentActors)
    {
        float Distance = FVector::Dist(ControlledBoss->GetActorLocation(), TacticalActor->GetActorLocation());
        if (Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestTacticalObject = TacticalActor;
        }
    }
    
    if (ClosestTacticalObject)
    {
        // 환경 오브젝트 활용
        if (ClosestTacticalObject->ActorHasTag(TEXT("Destructible")))
        {
            ControlledBoss->DestroyEnvironmentalObject(ClosestTacticalObject);
        }
        else if (ClosestTacticalObject->ActorHasTag(TEXT("EnvironmentalHazard")))
        {
            ControlledBoss->TriggerEnvironmentalHazard();
        }
        
        // 사용한 오브젝트 제거
        TacticalEnvironmentActors.Remove(ClosestTacticalObject);
    }
}

// 인식 업데이트 콜백
void AHSBossAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor || !IsValidTarget(Actor))
    {
        return;
    }
    
    // 새로운 타겟 추가
    if (Stimulus.WasSuccessfullySensed() && !CurrentTargets.Contains(Actor))
    {
        CurrentTargets.Add(Actor);
        ThreatTable.Add(Actor, 1.0f);
        
        // 첫 타겟 감지 시 전투 시작
        if (CurrentTargets.Num() == 1 && CurrentAIState == EBossAIState::PatrolPhase)
        {
            SetAIState(EBossAIState::CombatPhase);
        }
    }
    
    // 위협 수준 업데이트
    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
    {
        float* CurrentThreat = ThreatTable.Find(Actor);
        if (CurrentThreat)
        {
            *CurrentThreat += Stimulus.Strength;
        }
    }
}

// 인식 상실 콜백
void AHSBossAIController::OnTargetPerceptionForgotten(AActor* Actor)
{
    CurrentTargets.Remove(Actor);
    
    if (PrimaryTarget == Actor)
    {
        PrimaryTarget = nullptr;
        bNeedsTargetUpdate = true;
    }
}

// 페이즈별 패턴 선택
FHSBossAttackPattern AHSBossAIController::SelectPatternForPhase(EHSBossPhase Phase)
{
    TArray<FHSBossAttackPattern> ValidPatterns;
    
    // 현재 페이즈에서 사용 가능한 패턴 필터링
    for (const FHSBossAttackPattern& Pattern : AvailablePatterns)
    {
        if (Pattern.MinimumPhase <= (int32)Phase && EvaluatePatternConditions(Pattern))
        {
            ValidPatterns.Add(Pattern);
        }
    }
    
    // 패턴 점수 계산 및 선택
    if (ValidPatterns.Num() > 0)
    {
        float TotalScore = 0.0f;
        TArray<float> PatternScores;
        
        for (const FHSBossAttackPattern& Pattern : ValidPatterns)
        {
            float Score = CalculatePatternScore(Pattern);
            PatternScores.Add(Score);
            TotalScore += Score;
        }
        
        // 가중치 기반 랜덤 선택
        float RandomValue = FMath::RandRange(0.0f, TotalScore);
        float AccumulatedScore = 0.0f;
        
        for (int32 i = 0; i < ValidPatterns.Num(); i++)
        {
            AccumulatedScore += PatternScores[i];
            if (RandomValue <= AccumulatedScore)
            {
                return ValidPatterns[i];
            }
        }
    }
    
    // 기본 패턴 반환
    if (AvailablePatterns.Num() > 0)
    {
        return AvailablePatterns[0];
    }
    
    return FHSBossAttackPattern();
}

// 패턴 조건 평가
bool AHSBossAIController::EvaluatePatternConditions(const FHSBossAttackPattern& Pattern)
{
    if (!PrimaryTarget || !ControlledBoss)
    {
        return false;
    }
    
    // 거리 확인
    float DistanceToTarget = FVector::Dist(ControlledBoss->GetActorLocation(), PrimaryTarget->GetActorLocation());
    if (DistanceToTarget > Pattern.Range)
    {
        return false;
    }
    
    // 다중 플레이어 요구사항 확인
    if (Pattern.bRequiresMultiplePlayers && CurrentTargets.Num() < 2)
    {
        return false;
    }
    
    // 쿨다운 확인
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (LastPatternExecutionTime > 0.0f && (CurrentTime - LastPatternExecutionTime) < Pattern.Cooldown)
    {
        return false;
    }
    
    return true;
}

// 패턴 점수 계산
float AHSBossAIController::CalculatePatternScore(const FHSBossAttackPattern& Pattern)
{
    float Score = 100.0f;
    
    // 패턴 타입별 가중치
    switch (Pattern.PatternType)
    {
    case EHSBossPatternType::Melee:
        Score *= (CurrentTargets.Num() == 1) ? 1.5f : 0.8f;
        break;
        
    case EHSBossPatternType::Area:
        Score *= (CurrentTargets.Num() > 2) ? 2.0f : 0.5f;
        break;
        
    case EHSBossPatternType::Special:
        Score *= 1.2f;
        break;
        
    case EHSBossPatternType::Ultimate:
        Score *= (ControlledBoss->GetCurrentPhase() >= EHSBossPhase::Phase3) ? 1.5f : 0.3f;
        break;
    }
    
    // 협동 대응 중이면 AOE 패턴 선호
    if (bCoopCounterMeasureActive && Pattern.PatternType == EHSBossPatternType::Area)
    {
        Score *= 2.0f;
    }
    
    // 패턴 다양성을 위한 최근 사용 페널티
    if (CurrentExecutingPattern.PatternName == Pattern.PatternName)
    {
        Score *= 0.5f;
    }
    
    return Score;
}

// 위협 수준 평가
float AHSBossAIController::EvaluateThreatLevel(AActor* Target)
{
    if (!Target || !ControlledBoss)
    {
        return 0.0f;
    }
    
    float ThreatLevel = 1.0f;
    
    // 거리 기반 위협
    float Distance = FVector::Dist(ControlledBoss->GetActorLocation(), Target->GetActorLocation());
    ThreatLevel += (2000.0f - Distance) / 1000.0f;
    
    // 캐릭터 스탯 기반 위협
    if (AHSPlayerCharacter* PlayerChar = Cast<AHSPlayerCharacter>(Target))
    {
        if (UHSStatsComponent* StatsComp = PlayerChar->FindComponentByClass<UHSStatsComponent>())
        {
            // 공격력 기반 위협
            float AttackPower = StatsComp->GetAttackPower();
            ThreatLevel += AttackPower / 50.0f;
            
            // 체력이 낮은 타겟은 위협도 감소
            float HealthPercent = StatsComp->GetHealthPercent();
            if (HealthPercent < 0.3f)
            {
                ThreatLevel *= 0.5f;
            }
        }
    }
    
    // 기존 위협 수준 추가
    if (ThreatTable.Contains(Target))
    {
        ThreatLevel += ThreatTable[Target];
    }
    
    return ThreatLevel;
}

// 유효한 타겟인지 확인
bool AHSBossAIController::IsValidTarget(AActor* Target)
{
    if (!Target || !Target->IsValidLowLevel())
    {
        return false;
    }
    
    // 플레이어 캐릭터인지 확인
    AHSPlayerCharacter* PlayerChar = Cast<AHSPlayerCharacter>(Target);
    if (!PlayerChar)
    {
        return false;
    }
    
    // 생존 상태 확인
    if (PlayerChar->IsDead())
    {
        return false;
    }
    
    return true;
}

// 위협 테이블 업데이트
void AHSBossAIController::UpdateThreatTable()
{
    // 위협 수준 감소
    for (auto& ThreatPair : ThreatTable)
    {
        ThreatPair.Value = FMath::Max(0.0f, ThreatPair.Value - ThreatDecayRate);
    }
    
    // 유효하지 않은 타겟 제거
    CleanupThreatTable();
}

// 후퇴 여부 판단
bool AHSBossAIController::ShouldRetreat()
{
    if (!ControlledBoss)
    {
        return false;
    }
    
    // 현재 페이즈의 후퇴 임계값 확인
    EHSBossPhase CurrentPhase = ControlledBoss->GetCurrentPhase();
    float RetreatThreshold = PhaseRetreatThresholds.Contains(CurrentPhase) ? 
        PhaseRetreatThresholds[CurrentPhase] : 0.2f;
    
    // 체력 기반 후퇴 판단
    float HealthPercent = ControlledBoss->GetHealthPercent();
    if (HealthPercent < RetreatThreshold)
    {
        return true;
    }
    
    // 다수의 적에게 둘러싸인 경우
    int32 NearbyEnemyCount = 0;
    for (AActor* Target : CurrentTargets)
    {
        float Distance = FVector::Dist(ControlledBoss->GetActorLocation(), Target->GetActorLocation());
        if (Distance < 300.0f)
        {
            NearbyEnemyCount++;
        }
    }
    
    if (NearbyEnemyCount >= 4)
    {
        return true;
    }
    
    return false;
}

// 환경 사용 여부 판단
bool AHSBossAIController::ShouldUseEnvironment()
{
    return TacticalEnvironmentActors.Num() > 0 && 
           FMath::RandRange(0.0f, 1.0f) < EnvironmentUsageChance;
}

// 부하 소환 여부 판단
bool AHSBossAIController::ShouldCallMinions()
{
    if (!ControlledBoss)
    {
        return false;
    }
    
    // 페이즈 3 이상에서만 소환
    if (ControlledBoss->GetCurrentPhase() < EHSBossPhase::Phase3)
    {
        return false;
    }
    
    // 플레이어가 많을 때 소환
    return CurrentTargets.Num() >= 3;
}

// 전술적 위치 선택
FVector AHSBossAIController::SelectTacticalPosition()
{
    if (!ControlledBoss || !PrimaryTarget)
    {
        return ControlledBoss->GetActorLocation();
    }
    
    FVector CurrentLocation = ControlledBoss->GetActorLocation();
    FVector TargetLocation = PrimaryTarget->GetActorLocation();
    
    // 후퇴해야 하는 경우
    if (ShouldRetreat())
    {
        FVector RetreatDirection = (CurrentLocation - TargetLocation).GetSafeNormal();
        return CurrentLocation + RetreatDirection * 500.0f;
    }
    
    // 측면 이동
    FVector RightVector = FVector::CrossProduct(
        (TargetLocation - CurrentLocation).GetSafeNormal(),
        FVector::UpVector
    );
    
    float SideMovement = FMath::RandBool() ? 300.0f : -300.0f;
    return CurrentLocation + RightVector * SideMovement;
}

// 페이즈 1 행동
void AHSBossAIController::ExecutePhase1Behavior()
{
    // 기본 공격 패턴 위주
    // 단일 타겟 집중
    SetTargetStrategy(EBossTargetStrategy::NearestTarget);
}

// 페이즈 2 행동
void AHSBossAIController::ExecutePhase2Behavior()
{
    // 다양한 패턴 사용
    // 위협 기반 타겟 선택
    SetTargetStrategy(EBossTargetStrategy::HighestThreat);
    
    // 협동 위협 평가 시작
    EvaluateCoopThreat();
}

// 페이즈 3 행동
void AHSBossAIController::ExecutePhase3Behavior()
{
    // 공격적인 패턴
    // 낮은 체력 타겟 우선
    SetTargetStrategy(EBossTargetStrategy::LowestHealth);
    
    // 환경 전술 적극 사용
    EnvironmentUsageChance = 0.5f;
}

// 분노 행동
void AHSBossAIController::ExecuteEnragedBehavior()
{
    // 매우 공격적
    // 무작위 타겟 선택으로 예측 불가능
    SetTargetStrategy(EBossTargetStrategy::RandomTarget);
    
    // 패턴 실행 간격 감소
    PatternSelectionInterval = 2.0f;
}

// 최후의 저항 행동
void AHSBossAIController::ExecuteFinalStandBehavior()
{
    // 모든 능력 사용
    // 가장 가까운 타겟에 집중
    SetTargetStrategy(EBossTargetStrategy::NearestTarget);
    
    // 연속 패턴 실행
    PatternSelectionInterval = 1.5f;
    EnvironmentUsageChance = 0.8f;
}

// 활성 플레이어 수 반환
int32 AHSBossAIController::GetActivePlayerCount() const
{
    return CurrentTargets.Num();
}

// AI 틱 업데이트
void AHSBossAIController::UpdateAITick(float DeltaTime)
{
    // 타겟 업데이트 필요 시
    if (bNeedsTargetUpdate)
    {
        UpdateTargetSelection();
        bNeedsTargetUpdate = false;
    }
    
    // 패턴 재계산 필요 시
    if (bNeedsPatternRecalculation)
    {
        UpdatePatternWeights();
        bNeedsPatternRecalculation = false;
    }
}

// 상태 전환 처리
void AHSBossAIController::ProcessStateTransitions()
{
    // 보스 사망 시
    if (ControlledBoss && ControlledBoss->IsDead() && CurrentAIState != EBossAIState::Defeated)
    {
        SetAIState(EBossAIState::Defeated);
        return;
    }
    
    // 분노 상태 확인
    if (ControlledBoss && ControlledBoss->IsEnraged() && CurrentAIState != EBossAIState::Enraged)
    {
        SetAIState(EBossAIState::Enraged);
    }
}

// 타겟 유효성 검증
void AHSBossAIController::ValidateTargets()
{
    CurrentTargets.RemoveAll([this](AActor* Target)
    {
        return !IsValidTarget(Target);
    });

    if (MaxSimultaneousTargets > 0 && CurrentTargets.Num() > MaxSimultaneousTargets)
    {
        CurrentTargets.Sort([this](const AActor& A, const AActor& B)
        {
            const float ThreatA = EvaluateThreatLevel(const_cast<AActor*>(&A));
            const float ThreatB = EvaluateThreatLevel(const_cast<AActor*>(&B));
            return ThreatA > ThreatB;
        });

        CurrentTargets.SetNum(MaxSimultaneousTargets);
    }
}

// 위협 테이블 정리
void AHSBossAIController::CleanupThreatTable()
{
    TArray<AActor*> KeysToRemove;
    
    for (const auto& ThreatPair : ThreatTable)
    {
        if (!IsValidTarget(ThreatPair.Key) || ThreatPair.Value <= 0.0f)
        {
            KeysToRemove.Add(ThreatPair.Key);
        }
    }
    
    for (AActor* Key : KeysToRemove)
    {
        ThreatTable.Remove(Key);
    }
}

// 전략별 타겟 선택
AActor* AHSBossAIController::SelectTargetByStrategy()
{
    if (CurrentTargets.Num() == 0)
    {
        return nullptr;
    }
    
    switch (TargetStrategy)
    {
    case EBossTargetStrategy::HighestThreat:
        {
            AActor* HighestThreatTarget = nullptr;
            float HighestThreat = 0.0f;
            
            for (AActor* Target : CurrentTargets)
            {
                float Threat = EvaluateThreatLevel(Target);
                if (Threat > HighestThreat)
                {
                    HighestThreat = Threat;
                    HighestThreatTarget = Target;
                }
            }
            return HighestThreatTarget;
        }
        
    case EBossTargetStrategy::LowestHealth:
        {
            AActor* LowestHealthTarget = nullptr;
            float LowestHealth = FLT_MAX;
            
            for (AActor* Target : CurrentTargets)
            {
                if (AHSPlayerCharacter* PlayerChar = Cast<AHSPlayerCharacter>(Target))
                {
                    float Health = PlayerChar->GetHealth();
                    if (Health < LowestHealth)
                    {
                        LowestHealth = Health;
                        LowestHealthTarget = Target;
                    }
                }
            }
            return LowestHealthTarget;
        }
        
    case EBossTargetStrategy::NearestTarget:
        {
            AActor* NearestTarget = nullptr;
            float NearestDistance = FLT_MAX;
            
            FVector BossLocation = ControlledBoss->GetActorLocation();
            for (AActor* Target : CurrentTargets)
            {
                float Distance = FVector::Dist(BossLocation, Target->GetActorLocation());
                if (Distance < NearestDistance)
                {
                    NearestDistance = Distance;
                    NearestTarget = Target;
                }
            }
            return NearestTarget;
        }
        
    case EBossTargetStrategy::RandomTarget:
        {
            int32 RandomIndex = FMath::RandRange(0, CurrentTargets.Num() - 1);
            return CurrentTargets[RandomIndex];
        }
        
    case EBossTargetStrategy::SpecificRole:
        {
            if (PreferredTargetRoles.Num() > 0)
            {
                for (EHSPlayerRole PreferredRole : PreferredTargetRoles)
                {
                    AActor* MatchingTarget = nullptr;
                    float BestThreat = -FLT_MAX;

                    for (AActor* Target : CurrentTargets)
                    {
                        if (!Target)
                        {
                            continue;
                        }

                        if (const AHSPlayerCharacter* PlayerChar = Cast<AHSPlayerCharacter>(Target))
                        {
                            if (const AHSPlayerState* PlayerStateComp = PlayerChar->GetPlayerState<AHSPlayerState>())
                            {
                                if (PlayerStateComp->GetPlayerRole() == PreferredRole)
                                {
                                    float Threat = EvaluateThreatLevel(Target);
                                    if (Threat > BestThreat)
                                    {
                                        BestThreat = Threat;
                                        MatchingTarget = Target;
                                    }
                                }
                            }
                        }
                    }

                    if (MatchingTarget)
                    {
                        return MatchingTarget;
                    }
                }
            }

            return CurrentTargets[0];
        }
        
    default:
        return CurrentTargets[0];
    }
}

// AI 이벤트 브로드캐스트
void AHSBossAIController::BroadcastAIEvents()
{
    // AI 상태 변경 이벤트 등을 다른 시스템에 알림
    // 추후 이벤트 시스템 구현 시 활용
}

// 패턴 가중치 업데이트
void AHSBossAIController::UpdatePatternWeights()
{
    // 페이즈별 패턴 가중치 재계산
    PatternWeights.Empty();
    
    EHSBossPhase CurrentPhase = ControlledBoss ? ControlledBoss->GetCurrentPhase() : EHSBossPhase::Phase1;
    
    for (const FHSBossAttackPattern& Pattern : AvailablePatterns)
    {
        float Weight = 1.0f;
        
        // 페이즈에 따른 가중치
        if (Pattern.MinimumPhase > (int32)CurrentPhase)
        {
            Weight = 0.0f;
        }
        else
        {
            Weight = CalculatePatternScore(Pattern);
        }
        
        PatternWeights.Add(Pattern.PatternName, Weight);
    }
}
