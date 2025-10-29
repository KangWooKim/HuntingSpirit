/**
 * @file HSAIControllerBase.cpp
 * @brief 모든 AI 컨트롤러의 기본 클래스 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#include "HSAIControllerBase.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/World/Navigation/HSRuntimeNavigation.h"
#include "HuntingSpirit/World/Navigation/HSNavigationIntegration.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "BrainComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

/**
 * @brief 기본 생성자 구현
 * @details AI 컴포넌트들을 생성하고 기본값을 설정합니다
 */
AHSAIControllerBase::AHSAIControllerBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // 비헤이비어 트리 컴포넌트 생성
    BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));

    // 블랙보드 컴포넌트 생성
    BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));

    // AI 감지 컴포넌트 - 부모 클래스에서 생성됨
    AIPerceptionComponent = GetAIPerceptionComponent();
    if (!AIPerceptionComponent)
    {
        // 부모 클래스에 없으면 새로 생성
        AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
    }

    // 기본값 설정
    SightRadius = 800.0f;
    SightAngleDegrees = 90.0f;
    HearingRadius = 600.0f;
    MaxAge = 5.0f;
    bShowDebugInfo = false;
    bShowNavigationDebug = false;

    // 오너 적 캐릭터 캐시 초기화
    OwnerEnemy = nullptr;
    
    // === 네비게이션 시스템 통합 초기화 ===
    bAutoRegisterWithNavigationSystem = true;
    bEnableStuckDetection = true;
    StuckDistanceThreshold = 50.0f;
    StuckTimeThreshold = 3.0f;
    
    // 네비게이션 관련 변수 초기화
    CurrentNavigationRequestID = FGuid();
    LastSuccessfulMoveTime = 0.0f;
    LastKnownPosition = FVector::ZeroVector;
    LastPositionCheckTime = 0.0f;
}

/**
 * @brief 게임 시작 시 AI 시스템 초기화
 * @details 감지 시스템 설정, 네비게이션 시스템 등록, AI 시작을 수행합니다
 */
void AHSAIControllerBase::BeginPlay()
{
    Super::BeginPlay();

    // AI 감지 시스템 설정
    SetupAIPerception();

    // 감지 이벤트 바인딩
    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AHSAIControllerBase::OnPerceptionUpdated);
        AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AHSAIControllerBase::OnTargetPerceptionUpdated);
        AIPerceptionComponent->OnTargetPerceptionForgotten.AddDynamic(this, &AHSAIControllerBase::OnTargetPerceptionForgotten);
        AIPerceptionComponent->RequestStimuliListenerUpdate();
    }
    
    // === 네비게이션 시스템 통합 초기화 ===
    InitializeNavigationSystem();
    
    // 막힘 감지를 위한 초기 위치 설정
    if (GetPawn())
    {
        LastKnownPosition = GetPawn()->GetActorLocation();
        LastPositionCheckTime = GetWorld()->GetTimeSeconds();
        LastSuccessfulMoveTime = GetWorld()->GetTimeSeconds();
    }

    // AI 시작
    StartAI();
}

void AHSAIControllerBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateStuckDetection();

    if (bShowNavigationDebug && !bShowDebugInfo)
    {
        DrawNavigationDebug();
    }
}

/**
 * @brief AI 시작 구현
 * @details 폰을 캐시하고 블루프린트 이벤트를 호출합니다
 */
void AHSAIControllerBase::StartAI()
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        return;
    }

    // 오너 적 캐릭터 캐시
    OwnerEnemy = Cast<AHSEnemyBase>(ControlledPawn);

    UBehaviorTree* BehaviorTreeToRun = nullptr;
    if (OwnerEnemy)
    {
        BehaviorTreeToRun = OwnerEnemy->GetBehaviorTree();
    }

    if (BehaviorTreeToRun)
    {
        bool bBlackboardReady = true;
        if (BehaviorTreeToRun->BlackboardAsset)
        {
            if (!UseBlackboard(BehaviorTreeToRun->BlackboardAsset, BlackboardComponent))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSAIControllerBase: Failed to initialize blackboard for %s"), *GetName());
                bBlackboardReady = false;
            }
        }

        if (bBlackboardReady)
        {
            RunBehaviorTree(BehaviorTreeToRun);
        }
    }

    // 블루프린트 이벤트 호출
    OnPawnPossessed(ControlledPawn);
}

/**
 * @brief AI 중지 구현
 * @details 네비게이션 시스템 등록을 해제하고 참조를 정리합니다
 */
void AHSAIControllerBase::StopAI()
{
    // 네비게이션 시스템에서 등록 해제
    UnregisterFromNavigationSystem();

    if (BehaviorTreeComponent)
    {
        BehaviorTreeComponent->StopTree(EBTStopMode::Safe);
    }

    if (BrainComponent)
    {
        BrainComponent->StopLogic(TEXT("StopAI"));
    }

    StopMovement();

    if (BlackboardComponent && BlackboardComponent->HasValidAsset())
    {
        const int32 NumKeys = BlackboardComponent->GetNumKeys();
        for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
        {
            BlackboardComponent->ClearValue(static_cast<FBlackboard::FKey>(KeyIndex));
        }
    }

    SetCurrentTarget(nullptr);

    // 오너 캐릭터 참조 해제
    OwnerEnemy = nullptr;
}

// 시야 범위 설정
void AHSAIControllerBase::SetSightRange(float Range)
{
    SightRadius = FMath::Max(0.0f, Range);
    
    if (SightConfig)
    {
        SightConfig->SightRadius = SightRadius;
        AIPerceptionComponent->ConfigureSense(*SightConfig);
    }
}

// 시야 각도 설정
void AHSAIControllerBase::SetSightAngle(float Angle)
{
    SightAngleDegrees = FMath::Clamp(Angle, 0.0f, 180.0f);
    
    if (SightConfig)
    {
        SightConfig->PeripheralVisionAngleDegrees = SightAngleDegrees;
        AIPerceptionComponent->ConfigureSense(*SightConfig);
    }
}

// 청각 범위 설정
void AHSAIControllerBase::SetHearingRange(float Range)
{
    HearingRadius = FMath::Max(0.0f, Range);
    
    if (HearingConfig)
    {
        HearingConfig->HearingRange = HearingRadius;
        AIPerceptionComponent->ConfigureSense(*HearingConfig);
    }
}

// 현재 타겟 가져오기
AActor* AHSAIControllerBase::GetCurrentTarget() const
{
    if (BlackboardComponent)
    {
        return Cast<AActor>(BlackboardComponent->GetValueAsObject(TEXT("TargetActor")));
    }
    return nullptr;
}

// 현재 타겟 설정
void AHSAIControllerBase::SetCurrentTarget(AActor* NewTarget)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), NewTarget);
    }

    // 오너 적 캐릭터에게도 알림
    if (OwnerEnemy)
    {
        OwnerEnemy->SetCurrentTarget(NewTarget);
    }
}

// 현재 타겟 제거
void AHSAIControllerBase::ClearCurrentTarget()
{
    SetCurrentTarget(nullptr);
}

// 블랙보드 벡터 값 설정
void AHSAIControllerBase::SetBlackboardValueAsVector(const FName& KeyName, const FVector& Value)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsVector(KeyName, Value);
    }
}

// 블랙보드 오브젝트 값 설정
void AHSAIControllerBase::SetBlackboardValueAsObject(const FName& KeyName, UObject* Value)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsObject(KeyName, Value);
    }
}

// 블랙보드 불린 값 설정
void AHSAIControllerBase::SetBlackboardValueAsBool(const FName& KeyName, bool Value)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsBool(KeyName, Value);
    }
}

// 블랙보드 플로트 값 설정
void AHSAIControllerBase::SetBlackboardValueAsFloat(const FName& KeyName, float Value)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsFloat(KeyName, Value);
    }
}

// 블랙보드 정수 값 설정
void AHSAIControllerBase::SetBlackboardValueAsInt(const FName& KeyName, int32 Value)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsInt(KeyName, Value);
    }
}

// 블랙보드 벡터 값 가져오기
FVector AHSAIControllerBase::GetBlackboardValueAsVector(const FName& KeyName) const
{
    if (BlackboardComponent)
    {
        return BlackboardComponent->GetValueAsVector(KeyName);
    }
    return FVector::ZeroVector;
}

// 블랙보드 오브젝트 값 가져오기
UObject* AHSAIControllerBase::GetBlackboardValueAsObject(const FName& KeyName) const
{
    if (BlackboardComponent)
    {
        return BlackboardComponent->GetValueAsObject(KeyName);
    }
    return nullptr;
}

// 블랙보드 불린 값 가져오기
bool AHSAIControllerBase::GetBlackboardValueAsBool(const FName& KeyName) const
{
    if (BlackboardComponent)
    {
        return BlackboardComponent->GetValueAsBool(KeyName);
    }
    return false;
}

// 블랙보드 플로트 값 가져오기
float AHSAIControllerBase::GetBlackboardValueAsFloat(const FName& KeyName) const
{
    if (BlackboardComponent)
    {
        return BlackboardComponent->GetValueAsFloat(KeyName);
    }
    return 0.0f;
}

// 블랙보드 정수 값 가져오기
int32 AHSAIControllerBase::GetBlackboardValueAsInt(const FName& KeyName) const
{
    if (BlackboardComponent)
    {
        return BlackboardComponent->GetValueAsInt(KeyName);
    }
    return 0;
}

// AI 디버그 활성화/비활성화
void AHSAIControllerBase::EnableAIDebug(bool bEnable)
{
    bShowDebugInfo = bEnable;
}

// 디버그 정보 그리기
void AHSAIControllerBase::DrawDebugInfo(float Duration)
{
    if (!bShowDebugInfo || !GetPawn())
    {
        return;
    }

    FVector PawnLocation = GetPawn()->GetActorLocation();
    
    // 시야 범위 그리기
    DrawDebugSphere(GetWorld(), PawnLocation, SightRadius, 16, FColor::Green, false, Duration, 0, 2.0f);
    
    // 청각 범위 그리기
    DrawDebugSphere(GetWorld(), PawnLocation, HearingRadius, 16, FColor::Blue, false, Duration, 0, 1.0f);
    
    // 시야각 그리기
    FVector ForwardVector = GetPawn()->GetActorForwardVector();
    FVector LeftDirection = ForwardVector.RotateAngleAxis(-SightAngleDegrees * 0.5f, FVector::UpVector);
    FVector RightDirection = ForwardVector.RotateAngleAxis(SightAngleDegrees * 0.5f, FVector::UpVector);
    
    DrawDebugLine(GetWorld(), PawnLocation, PawnLocation + (LeftDirection * SightRadius), FColor::Yellow, false, Duration, 0, 2.0f);
    DrawDebugLine(GetWorld(), PawnLocation, PawnLocation + (RightDirection * SightRadius), FColor::Yellow, false, Duration, 0, 2.0f);
    
    // 현재 타겟 표시
    AActor* CurrentTarget = GetCurrentTarget();
    if (CurrentTarget)
    {
        DrawDebugLine(GetWorld(), PawnLocation, CurrentTarget->GetActorLocation(), FColor::Red, false, Duration, 0, 3.0f);
        DrawDebugSphere(GetWorld(), CurrentTarget->GetActorLocation(), 50.0f, 8, FColor::Red, false, Duration, 0, 2.0f);
    }
    
    // 네비게이션 디버그 정보 추가
    if (bShowNavigationDebug)
    {
        DrawNavigationDebug();
    }
}

// AI 감지 시스템 설정
void AHSAIControllerBase::SetupAIPerception()
{
    if (!AIPerceptionComponent)
    {
        return;
    }

    // 시각 감지 설정
    SetupSightSense();
    
    // 청각 감지 설정
    SetupHearingSense();
    
    // 데미지 감지 설정
    SetupDamageSense();

    // 주요 감지 센스를 시각으로 설정
    if (SightConfig)
    {
        AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
    }
}

// 시각 감지 설정
void AHSAIControllerBase::SetupSightSense()
{
    if (!SightConfig)
    {
        SightConfig = NewObject<UAISenseConfig_Sight>(this, TEXT("SightConfig"));
    }
    if (SightConfig)
    {
        SightConfig->SightRadius = SightRadius;
        SightConfig->LoseSightRadius = SightRadius * 1.5f; // 잃는 범위는 더 넓게
        SightConfig->PeripheralVisionAngleDegrees = SightAngleDegrees;
        SightConfig->SetMaxAge(MaxAge);
        SightConfig->AutoSuccessRangeFromLastSeenLocation = 200.0f;
        
        // 감지할 대상 설정 (플레이어만)
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;

        AIPerceptionComponent->ConfigureSense(*SightConfig);
    }
}

// 청각 감지 설정
void AHSAIControllerBase::SetupHearingSense()
{
    if (!HearingConfig)
    {
        HearingConfig = NewObject<UAISenseConfig_Hearing>(this, TEXT("HearingConfig"));
    }
    if (HearingConfig)
    {
        HearingConfig->HearingRange = HearingRadius;
        HearingConfig->SetMaxAge(MaxAge);

        // 감지할 대상 설정
        HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
        HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
        HearingConfig->DetectionByAffiliation.bDetectEnemies = true;

        AIPerceptionComponent->ConfigureSense(*HearingConfig);
    }
}

// 데미지 감지 설정
void AHSAIControllerBase::SetupDamageSense()
{
    if (!DamageConfig)
    {
        DamageConfig = NewObject<UAISenseConfig_Damage>(this, TEXT("DamageConfig"));
    }
    if (DamageConfig)
    {
        DamageConfig->SetMaxAge(MaxAge);
        AIPerceptionComponent->ConfigureSense(*DamageConfig);
    }
}

// 감지 업데이트 이벤트
void AHSAIControllerBase::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
    // 디버그 정보 표시
    if (bShowDebugInfo)
    {
        DrawDebugInfo(1.0f);
    }
}

// 타겟 감지 업데이트 이벤트
void AHSAIControllerBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor || !IsValidTarget(Actor))
    {
        return;
    }

    // 감지 타입에 따라 처리 (FName을 문자열로 변환하여 비교)
    FString StimulusTypeName = Stimulus.Type.Name.ToString();
    
    if (StimulusTypeName.Contains(TEXT("AISense_Sight")))
    {
        HandleSightStimulus(Actor, Stimulus);
    }
    else if (StimulusTypeName.Contains(TEXT("AISense_Hearing")))
    {
        HandleHearingStimulus(Actor, Stimulus);
    }
    else if (StimulusTypeName.Contains(TEXT("AISense_Damage")))
    {
        HandleDamageStimulus(Actor, Stimulus);
    }
}

// 타겟 감지 상실 이벤트
void AHSAIControllerBase::OnTargetPerceptionForgotten(AActor* Actor)
{
    if (Actor == GetCurrentTarget())
    {
        // 현재 타겟을 잃어버린 경우 조사 상태로 전환
        if (OwnerEnemy)
        {
            OwnerEnemy->SetAIState(EHSEnemyAIState::Investigating);
        }
    }
}

// ===== 네비게이션 시스템 통합 함수들 구현 =====

// 지정된 위치로 이동 (통합 네비게이션 시스템 사용)
EHSNavigationRequestResult AHSAIControllerBase::MoveToLocationAdvanced(const FVector& TargetLocation, 
                                                                      float AcceptanceRadius,
                                                                      bool bStopOnOverlap,
                                                                      bool bUsePathfinding,
                                                                      bool bCanStrafe)
{
    if (!GetPawn() || !IsNavigationSystemReady())
    {
        return EHSNavigationRequestResult::Failed;
    }
    
    // 기존 요청 취소
    if (CurrentNavigationRequestID.IsValid() && RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->CancelPathfindingRequest(CurrentNavigationRequestID);
    }
    
    // 막힘 상태 확인 및 복구
    if (IsStuck())
    {
        if (!RecoverFromStuck())
        {
            return EHSNavigationRequestResult::Failed;
        }
    }
    
    if (bUsePathfinding && RuntimeNavigation.IsValid())
    {
        // 통합 네비게이션 시스템 사용
        FVector StartLocation = GetPawn()->GetActorLocation();
        int32 Priority = 50; // 일반 우선순위
        
        CurrentNavigationRequestID = RuntimeNavigation->RequestPathfinding(this, StartLocation, TargetLocation, Priority);
        
        if (CurrentNavigationRequestID.IsValid())
        {
            // 블랙보드에 목표 위치 설정
            SetBlackboardValueAsVector(TEXT("TargetLocation"), TargetLocation);
            
            if (bShowNavigationDebug)
            {
                UE_LOG(LogTemp, Log, TEXT("HSAIControllerBase: 통합 네비게이션 요청 성공. AI: %s, 목표: %s"), 
                       *GetName(), *TargetLocation.ToString());
            }
            
            return EHSNavigationRequestResult::Pending;
        }
        else
        {
            // 통합 시스템 실패 시 기본 네비게이션 사용
            FAIMoveRequest MoveRequest;
            MoveRequest.SetGoalLocation(TargetLocation);
            MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
            MoveRequest.SetCanStrafe(bCanStrafe);
            MoveRequest.SetReachTestIncludesAgentRadius(bStopOnOverlap);
            MoveRequest.SetReachTestIncludesGoalRadius(bStopOnOverlap);
            MoveRequest.SetUsePathfinding(bUsePathfinding);
            
            FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
            
            if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
            {
                LastSuccessfulMoveTime = GetWorld()->GetTimeSeconds();
                return EHSNavigationRequestResult::Success;
            }
            else if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
            {
                return EHSNavigationRequestResult::Success;
            }
            else
            {
                return EHSNavigationRequestResult::Failed;
            }
        }
    }
    else
    {
        // 기본 UE 네비게이션 사용
        FAIMoveRequest MoveRequest;
        MoveRequest.SetGoalLocation(TargetLocation);
        MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
        MoveRequest.SetCanStrafe(bCanStrafe);
        MoveRequest.SetReachTestIncludesAgentRadius(bStopOnOverlap);
        MoveRequest.SetReachTestIncludesGoalRadius(bStopOnOverlap);
        MoveRequest.SetUsePathfinding(bUsePathfinding);
        
        FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
        
        if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
        {
            LastSuccessfulMoveTime = GetWorld()->GetTimeSeconds();
            return EHSNavigationRequestResult::Success;
        }
        else if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
        {
            return EHSNavigationRequestResult::Success;
        }
        else
        {
            return EHSNavigationRequestResult::Failed;
        }
    }
}

// 지정된 액터로 이동 (통합 네비게이션 시스템 사용)
EHSNavigationRequestResult AHSAIControllerBase::MoveToActorAdvanced(AActor* TargetActor,
                                                                   float AcceptanceRadius,
                                                                   bool bStopOnOverlap,
                                                                   bool bUsePathfinding,
                                                                   bool bCanStrafe)
{
    if (!TargetActor)
    {
        return EHSNavigationRequestResult::Failed;
    }
    
    FVector TargetLocation = TargetActor->GetActorLocation();
    return MoveToLocationAdvanced(TargetLocation, AcceptanceRadius, bStopOnOverlap, bUsePathfinding, bCanStrafe);
}

// 현재 이동 중지
void AHSAIControllerBase::StopMovementAdvanced()
{
    // 통합 네비게이션 요청 취소
    if (CurrentNavigationRequestID.IsValid() && RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->CancelPathfindingRequest(CurrentNavigationRequestID);
        CurrentNavigationRequestID = FGuid();
    }
    
    // 기본 이동 중지
    StopMovement();
    
    if (bShowNavigationDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("HSAIControllerBase: 이동을 중지했습니다. AI: %s"), *GetName());
    }
}

// AI가 막혀있는지 확인
bool AHSAIControllerBase::IsStuck() const
{
    if (!GetPawn() || !bEnableStuckDetection)
    {
        return false;
    }
    
    float CurrentTime = GetWorld()->GetTimeSeconds();
    FVector CurrentLocation = GetPawn()->GetActorLocation();
    
    // 시간 기반 막힘 감지
    if ((CurrentTime - LastSuccessfulMoveTime) > StuckTimeThreshold)
    {
        return true;
    }
    
    // 거리 기반 막힘 감지 (일정 시간 동안 같은 위치에 머무르고 있는지)
    if ((CurrentTime - LastPositionCheckTime) > 1.0f) // 1초마다 체크
    {
        float DistanceMoved = FVector::Dist(CurrentLocation, LastKnownPosition);
        
        if (DistanceMoved < StuckDistanceThreshold)
        {
            // 이동 중인데 거리가 적으면 막힌 것으로 판단
            UPathFollowingComponent* PathFollowingComp = GetPathFollowingComponent();
            if (PathFollowingComp && PathFollowingComp->GetStatus() == EPathFollowingStatus::Moving)
            {
                return true;
            }
        }
        
        // 위치 정보 업데이트 (비상수 방식으로 막히지 않도록 조심스럽게 처리)
        const_cast<AHSAIControllerBase*>(this)->LastKnownPosition = CurrentLocation;
        const_cast<AHSAIControllerBase*>(this)->LastPositionCheckTime = CurrentTime;
    }
    
    return false;
}

// 막힌 AI 복구
bool AHSAIControllerBase::RecoverFromStuck()
{
    if (!GetPawn())
    {
        return false;
    }
    
    bool bRecoverySuccess = false;
    
    // 통합 네비게이션 시스템에서 복구 시도
    if (RuntimeNavigation.IsValid())
    {
        bRecoverySuccess = RuntimeNavigation->RecoverStuckAI(this);
    }
    
    // 통합 시스템 복구가 실패한 경우 수동 복구 시도
    if (!bRecoverySuccess)
    {
        FVector SafeLocation = GetSafeLocationNearby(GetPawn()->GetActorLocation());
        
        if (SafeLocation != FVector::ZeroVector)
        {
            GetPawn()->SetActorLocation(SafeLocation);
            bRecoverySuccess = true;
        }
    }
    
    if (bRecoverySuccess)
    {
        // 복구 성공 시 상태 리셋
        LastSuccessfulMoveTime = GetWorld()->GetTimeSeconds();
        LastKnownPosition = GetPawn()->GetActorLocation();
        LastPositionCheckTime = GetWorld()->GetTimeSeconds();
        
        if (bShowNavigationDebug)
        {
            UE_LOG(LogTemp, Log, TEXT("HSAIControllerBase: 막힌 AI가 복구되었습니다. AI: %s"), *GetName());
        }
    }
    
    return bRecoverySuccess;
}

// 현재 네비게이션 상태 반환
FString AHSAIControllerBase::GetNavigationStatusString() const
{
    if (!GetPawn())
    {
        return TEXT("폰이 없음");
    }
    
    if (IsStuck())
    {
        return TEXT("막힘");
    }
    
    UPathFollowingComponent* PathFollowingComp = GetPathFollowingComponent();
    if (PathFollowingComp)
    {
        switch (PathFollowingComp->GetStatus())
        {
            case EPathFollowingStatus::Idle:
                return TEXT("대기");
            case EPathFollowingStatus::Waiting:
                return TEXT("대기 중");
            case EPathFollowingStatus::Moving:
                return TEXT("이동 중");
            case EPathFollowingStatus::Paused:
                return TEXT("일시 정지");
            default:
                return TEXT("알 수 없음");
        }
    }
    
    return TEXT("네비게이션 비활성화");
}

// ===== 네비게이션 시스템 통합 내부 함수들 =====

// 네비게이션 시스템 초기화
void AHSAIControllerBase::InitializeNavigationSystem()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    // 런타임 네비게이션 서브시스템 가져오기
    UGameInstance* GameInstance = World->GetGameInstance();
    if (GameInstance)
    {
        RuntimeNavigation = GameInstance->GetSubsystem<UHSRuntimeNavigation>();
    }
    
    // 네비게이션 통합 컴포넌트 찾기
    if (APawn* ControlledPawn = GetPawn())
    {
        NavigationIntegration = ControlledPawn->FindComponentByClass<UHSNavigationIntegration>();
    }
    
    // 자동 등록 실행
    if (bAutoRegisterWithNavigationSystem)
    {
        RegisterWithNavigationSystem();
    }
    
    if (bShowNavigationDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("HSAIControllerBase: 네비게이션 시스템 초기화 완료. AI: %s"), *GetName());
    }
}

// 네비게이션 시스템에 등록
void AHSAIControllerBase::RegisterWithNavigationSystem()
{
    // 런타임 네비게이션에 등록
    if (RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->RegisterAIController(this);
    }
    
    // 네비게이션 통합 컴포넌트에 등록
    if (NavigationIntegration.IsValid())
    {
        NavigationIntegration->RegisterAIController(this);
    }
    
    if (bShowNavigationDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("HSAIControllerBase: 네비게이션 시스템에 등록되었습니다. AI: %s"), *GetName());
    }
}

// 네비게이션 시스템에서 등록 해제
void AHSAIControllerBase::UnregisterFromNavigationSystem()
{
    // 현재 진행 중인 네비게이션 요청 취소
    if (CurrentNavigationRequestID.IsValid() && RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->CancelPathfindingRequest(CurrentNavigationRequestID);
        CurrentNavigationRequestID = FGuid();
    }
    
    // 런타임 네비게이션에서 등록 해제
    if (RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->UnregisterAIController(this);
    }
    
    // 네비게이션 통합 컴포넌트에서 등록 해제
    if (NavigationIntegration.IsValid())
    {
        NavigationIntegration->UnregisterAIController(this);
    }
    
    if (bShowNavigationDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("HSAIControllerBase: 네비게이션 시스템에서 등록 해제되었습니다. AI: %s"), *GetName());
    }
}

// 막힘 상태 업데이트
void AHSAIControllerBase::UpdateStuckDetection()
{
    if (!GetPawn() || !bEnableStuckDetection)
    {
        return;
    }
    
    float CurrentTime = GetWorld()->GetTimeSeconds();
    FVector CurrentLocation = GetPawn()->GetActorLocation();
    
    // 위치 변화 감지
    float DistanceMoved = FVector::Dist(CurrentLocation, LastKnownPosition);
    
    if (DistanceMoved > StuckDistanceThreshold)
    {
        // 이동이 감지된 경우 성공적인 이동으로 기록
        LastSuccessfulMoveTime = CurrentTime;
    }
    
    // 주기적으로 막힘 상태 체크 및 자동 복구
    if ((CurrentTime - LastPositionCheckTime) > 2.0f) // 2초마다 체크
    {
        if (IsStuck())
        {
            RecoverFromStuck();
        }
        
        LastKnownPosition = CurrentLocation;
        LastPositionCheckTime = CurrentTime;
    }
}

// 네비게이션 디버그 정보 그리기
void AHSAIControllerBase::DrawNavigationDebug()
{
    if (!bShowNavigationDebug || !GetPawn())
    {
        return;
    }
    
    FVector PawnLocation = GetPawn()->GetActorLocation();
    
    // 막힘 상태 시각화
    if (IsStuck())
    {
        DrawDebugSphere(GetWorld(), PawnLocation, 100.0f, 8, FColor::Red, false, 1.0f, 0, 3.0f);
        DrawDebugString(GetWorld(), PawnLocation + FVector(0, 0, 150), TEXT("STUCK"), nullptr, FColor::Red, 1.0f);
    }
    
    // 네비게이션 상태 표시
    FString StatusString = GetNavigationStatusString();
    DrawDebugString(GetWorld(), PawnLocation + FVector(0, 0, 200), StatusString, nullptr, FColor::Yellow, 1.0f);
    
    // 마지막 알려진 위치 표시
    DrawDebugSphere(GetWorld(), LastKnownPosition, 30.0f, 8, FColor::Blue, false, 1.0f, 0, 2.0f);
    DrawDebugLine(GetWorld(), PawnLocation, LastKnownPosition, FColor::Blue, false, 1.0f, 0, 1.0f);
    
    // 현재 네비게이션 요청 상태
    if (CurrentNavigationRequestID.IsValid())
    {
        DrawDebugSphere(GetWorld(), PawnLocation, 80.0f, 8, FColor::Green, false, 1.0f, 0, 2.0f);
        DrawDebugString(GetWorld(), PawnLocation + FVector(0, 0, 250), TEXT("NAV REQUEST ACTIVE"), nullptr, FColor::Green, 1.0f);
    }
}

// 네비게이션 시스템 준비 상태 확인
bool AHSAIControllerBase::IsNavigationSystemReady() const
{
    return RuntimeNavigation.IsValid() || GetWorld()->GetNavigationSystem() != nullptr;
}

// 주변의 안전한 위치 찾기
FVector AHSAIControllerBase::GetSafeLocationNearby(const FVector& OriginalLocation, float SearchRadius) const
{
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys)
    {
        return FVector::ZeroVector;
    }
    
    // 주변에서 네비게이션 가능한 위치 찾기
    FNavLocation NavLocation;
    if (NavSys->GetRandomReachablePointInRadius(OriginalLocation, SearchRadius, NavLocation))
    {
        return NavLocation.Location;
    }
    
    // 기본 위치에서 가장 가까운 네비게이션 위치 찾기
    if (NavSys->ProjectPointToNavigation(OriginalLocation, NavLocation, FVector(SearchRadius, SearchRadius, 200.0f)))
    {
        return NavLocation.Location;
    }
    
    return FVector::ZeroVector;
}

// 유효한 타겟인지 확인
bool AHSAIControllerBase::IsValidTarget(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    // 플레이어 캐릭터인지 확인
    AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(Actor);
    if (!PlayerCharacter)
    {
        return false;
    }

    // 살아있는지 확인
    UHSCombatComponent* CombatComponent = PlayerCharacter->FindComponentByClass<UHSCombatComponent>();
    if (!CombatComponent || !CombatComponent->IsAlive())
    {
        return false;
    }

    return true;
}

// 시각 자극 처리
void AHSAIControllerBase::HandleSightStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
    if (Stimulus.WasSuccessfullySensed())
    {
        // 플레이어를 발견한 경우
        if (!GetCurrentTarget() && OwnerEnemy)
        {
            OwnerEnemy->StartCombat(Actor);
        }
        
        // 마지막 알려진 위치 업데이트
        SetBlackboardValueAsVector(TEXT("LastKnownPlayerLocation"), Stimulus.StimulusLocation);
    }
    else
    {
        // 플레이어를 놓친 경우
        if (Actor == GetCurrentTarget() && OwnerEnemy)
        {
            OwnerEnemy->SetAIState(EHSEnemyAIState::Investigating);
        }
    }
}

// 청각 자극 처리
void AHSAIControllerBase::HandleHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
    if (Stimulus.WasSuccessfullySensed())
    {
        // 소리를 들은 경우 조사하러 이동
        SetBlackboardValueAsVector(TEXT("InvestigateLocation"), Stimulus.StimulusLocation);
        
        if (OwnerEnemy && OwnerEnemy->GetAIState() == EHSEnemyAIState::Idle)
        {
            OwnerEnemy->SetAIState(EHSEnemyAIState::Investigating);
        }
    }
}

// 데미지 자극 처리
void AHSAIControllerBase::HandleDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
    if (Stimulus.WasSuccessfullySensed())
    {
        // 데미지를 받은 경우 즉시 전투 시작
        if (OwnerEnemy && IsValidTarget(Actor))
        {
            OwnerEnemy->StartCombat(Actor);
        }
    }
}
