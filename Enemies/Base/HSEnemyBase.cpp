// HuntingSpirit Game - Enemy Base Implementation

#include "HSEnemyBase.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Combat/HSHitReactionComponent.h"
#include "HuntingSpirit/AI/HSAIControllerBase.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "Perception/PawnSensingComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"

// 생성자
AHSEnemyBase::AHSEnemyBase()
{
    // Tick 활성화
    PrimaryActorTick.bCanEverTick = true;

    // 감지 컴포넌트 생성 (적 전용)
    PawnSensingComponent = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensingComponent"));
    PawnSensingComponent->SetPeripheralVisionAngle(90.0f);
    PawnSensingComponent->SightRadius = 800.0f;
    PawnSensingComponent->HearingThreshold = 600.0f;
    PawnSensingComponent->LOSHearingThreshold = 1200.0f;

    // 기본값 설정
    EnemyType = EHSEnemyType::Melee;
    EnemyRank = EHSEnemyRank::Normal;
    EnemyName = TEXT("Basic Enemy");
    EnemyDescription = TEXT("A basic enemy.");
    
    CurrentAIState = EHSEnemyAIState::Idle;
    CurrentTarget = nullptr;
    
    DetectionRange = 800.0f;
    LoseTargetRange = 1200.0f;
    SightAngle = 90.0f;
    AttackRange = 150.0f;
    AttackCooldown = 2.0f;
    
    bInCombat = false;
    bShouldPatrol = true;
    PatrolRadius = 500.0f;
    PatrolWaitTime = 3.0f;
    
    HealthScalePerRank = 1.5f;
    DamageScalePerRank = 1.3f;

    // 기본 공격 데미지 설정
    AttackDamageInfo.BaseDamage = 20.0f;
    AttackDamageInfo.DamageType = EHSDamageType::Physical;
    AttackDamageInfo.CalculationMode = EHSDamageCalculationMode::Fixed;

    // AI 컨트롤러 클래스 설정
    AIControllerClass = AHSAIControllerBase::StaticClass();

    // 캐시 초기화
    AIController = nullptr;
    BlackboardComponent = nullptr;
    BehaviorTreeComponent = nullptr;
}

// 게임 시작 시 호출
void AHSEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    // 적 초기화
    InitializeEnemy();
}

// 매 프레임 호출
void AHSEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 전투 중이 아닐 때만 플레이어 탐지 수행
    if (!bInCombat && CurrentAIState != EHSEnemyAIState::Dead)
    {
        AActor* NearestPlayer = FindNearestPlayer();
        if (NearestPlayer && CanSeeTarget(NearestPlayer))
        {
            float DistanceToPlayer = GetDistanceToTarget(NearestPlayer);
            if (DistanceToPlayer <= DetectionRange)
            {
                StartCombat(NearestPlayer);
            }
        }
    }

    // 타겟이 있지만 너무 멀어졌다면 전투 종료
    if (bInCombat && CurrentTarget)
    {
        float DistanceToTarget = GetDistanceToTarget(CurrentTarget);
        if (DistanceToTarget > LoseTargetRange)
        {
            EndCombat();
        }
    }

    // 블랙보드 업데이트
    UpdateBlackboard();
}

// 빙의될 때 호출
void AHSEnemyBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // AI 컨트롤러 설정
    SetupAIController();
}

// AI 상태 설정
void AHSEnemyBase::SetAIState(EHSEnemyAIState NewState)
{
    if (CurrentAIState != NewState)
    {
        EHSEnemyAIState OldState = CurrentAIState;
        CurrentAIState = NewState;

        // 상태 변경 이벤트 발생
        OnEnemyAIStateChanged.Broadcast(NewState);

        // 상태별 처리
        switch (NewState)
        {
            case EHSEnemyAIState::Dead:
                EndCombat();
                SetCharacterState(ECharacterState::Dead);
                break;

            case EHSEnemyAIState::Attacking:
                SetCharacterState(ECharacterState::Attacking);
                break;

            case EHSEnemyAIState::Chasing:
                if (OldState != EHSEnemyAIState::Attacking)
                {
                    SetCharacterState(ECharacterState::Running);
                }
                break;

            case EHSEnemyAIState::Idle:
            case EHSEnemyAIState::Patrol:
                if (OldState != EHSEnemyAIState::Attacking)
                {
                    SetCharacterState(ECharacterState::Walking);
                }
                break;

            default:
                break;
        }
    }
}

// 현재 타겟 설정
void AHSEnemyBase::SetCurrentTarget(AActor* NewTarget)
{
    if (CurrentTarget != NewTarget)
    {
        AActor* OldTarget = CurrentTarget;
        CurrentTarget = NewTarget;

        // 타겟 변경 이벤트 발생
        OnEnemyTargetChanged.Broadcast(OldTarget, NewTarget);

        // 마지막 알려진 플레이어 위치 업데이트
        if (NewTarget)
        {
            LastKnownPlayerLocation = NewTarget->GetActorLocation();
        }
    }
}

// 전투 시작
void AHSEnemyBase::StartCombat(AActor* Target)
{
    if (!Target || bInCombat)
    {
        return;
    }

    bInCombat = true;
    SetCurrentTarget(Target);
    SetAIState(EHSEnemyAIState::Chasing);

    // 달리기 속도로 변경
    SetRunSpeed(GetCharacterMovement()->MaxWalkSpeed * 1.5f);
}

// 전투 종료
void AHSEnemyBase::EndCombat()
{
    if (!bInCombat)
    {
        return;
    }

    bInCombat = false;
    SetCurrentTarget(nullptr);
    SetAIState(EHSEnemyAIState::Idle);

    // 공격 쿨다운 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(AttackCooldownTimer);

    // 걷기 속도로 복원
    SetWalkSpeed(GetCharacterMovement()->MaxWalkSpeed / 1.5f);
}

// 공격 수행
void AHSEnemyBase::PerformAttack()
{
    if (!CurrentTarget || GetWorld()->GetTimerManager().IsTimerActive(AttackCooldownTimer))
    {
        return;
    }

    // 공격 범위 확인
    float DistanceToTarget = GetDistanceToTarget(CurrentTarget);
    if (DistanceToTarget > AttackRange)
    {
        return;
    }

    // 공격 상태로 변경
    SetAIState(EHSEnemyAIState::Attacking);

    // 타겟에게 데미지 적용
    UHSCombatComponent* TargetCombat = CurrentTarget->FindComponentByClass<UHSCombatComponent>();
    if (TargetCombat)
    {
        FHSDamageResult DamageResult = TargetCombat->ApplyDamage(AttackDamageInfo, this);
        
        // 데미지 전달 이벤트 발생
        OnEnemyDamageDealt.Broadcast(DamageResult.FinalDamage, CurrentTarget);
    }

    // 공격 애니메이션 재생 (기본 공격 애니메이션 사용)
    PerformBasicAttack();

    // 공격 쿨다운 시작
    GetWorld()->GetTimerManager().SetTimer(AttackCooldownTimer, this, &AHSEnemyBase::OnAttackCooldownExpired, AttackCooldown, false);
}

// 타겟을 볼 수 있는지 확인
bool AHSEnemyBase::CanSeeTarget(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }

    // 거리 확인
    float Distance = GetDistanceToTarget(Target);
    if (Distance > DetectionRange)
    {
        return false;
    }

    // 시야각 확인
    FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FVector ForwardVector = GetActorForwardVector();
    
    float DotProduct = FVector::DotProduct(ForwardVector, ToTarget);
    float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    
    if (AngleToTarget > SightAngle * 0.5f)
    {
        return false;
    }

    // 라인 트레이스로 장애물 확인
    FHitResult HitResult;
    FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f); // 눈 높이
    FVector EndLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(Target);

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams);
    
    return !bHit; // 장애물이 없으면 볼 수 있음
}

// 타겟까지의 거리 계산
float AHSEnemyBase::GetDistanceToTarget(AActor* Target) const
{
    if (!Target)
    {
        return FLT_MAX;
    }

    return FVector::Dist(GetActorLocation(), Target->GetActorLocation());
}

// 가장 가까운 플레이어 찾기
AActor* AHSEnemyBase::FindNearestPlayer() const
{
    TArray<AActor*> PlayerCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSPlayerCharacter::StaticClass(), PlayerCharacters);

    AActor* NearestPlayer = nullptr;
    float NearestDistance = FLT_MAX;

    for (AActor* Player : PlayerCharacters)
    {
        if (Player && IsPlayerCharacter(Player))
        {
            // 살아있는 플레이어만 대상으로 함
            UHSCombatComponent* PlayerCombat = Player->FindComponentByClass<UHSCombatComponent>();
            if (PlayerCombat && PlayerCombat->IsAlive())
            {
                float Distance = GetDistanceToTarget(Player);
                if (Distance < NearestDistance)
                {
                    NearestDistance = Distance;
                    NearestPlayer = Player;
                }
            }
        }
    }

    return NearestPlayer;
}

// 적 스탯 설정
void AHSEnemyBase::SetEnemyStats(float InitialHealth, float Damage, float MoveSpeed)
{
    UHSCombatComponent* Combat = GetCombatComponent();
    if (Combat)
    {
        Combat->SetMaxHealth(InitialHealth);
        Combat->SetCurrentHealth(InitialHealth);
    }

    AttackDamageInfo.BaseDamage = Damage;
    SetWalkSpeed(MoveSpeed);
    SetRunSpeed(MoveSpeed * 1.5f);
}

// 등급에 따른 스탯 스케일링
void AHSEnemyBase::ScaleStatsForRank()
{
    float HealthMultiplier = 1.0f;
    float DamageMultiplier = 1.0f;

    // 등급별 배수 계산
    int32 RankLevel = static_cast<int32>(EnemyRank);
    for (int32 i = 0; i < RankLevel; ++i)
    {
        HealthMultiplier *= HealthScalePerRank;
        DamageMultiplier *= DamageScalePerRank;
    }

    // 현재 스탯에 배수 적용
    UHSCombatComponent* Combat = GetCombatComponent();
    if (Combat)
    {
        float CurrentMaxHealth = Combat->GetMaxHealth();
        Combat->SetMaxHealth(CurrentMaxHealth * HealthMultiplier);
        Combat->SetCurrentHealth(Combat->GetMaxHealth());
    }

    AttackDamageInfo.BaseDamage *= DamageMultiplier;
}

// 적 초기화
void AHSEnemyBase::InitializeEnemy()
{
    // 스폰 위치 저장
    SpawnLocation = GetActorLocation();
    PatrolTarget = SpawnLocation;

    // 컴포넌트 설정
    SetupCombatComponent();
    SetupSensingComponent();

    // 등급에 따른 스탯 스케일링
    ScaleStatsForRank();
}

// AI 컨트롤러 설정
void AHSEnemyBase::SetupAIController()
{
    AIController = Cast<AHSAIControllerBase>(GetController());
    if (AIController)
    {
        // 블랙보드 및 비헤이비어 트리 컴포넌트 가져오기 (널 포인터 체크 수행)
        BlackboardComponent = AIController->GetBlackboardComponent();
        BehaviorTreeComponent = AIController->GetBehaviorTreeComponent();

        // 컴포넌트 유효성 체크
        if (!BlackboardComponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSEnemyBase: BlackboardComponent is null for enemy %s"), *GetName());
        }

        if (!BehaviorTreeComponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSEnemyBase: BehaviorTreeComponent is null for enemy %s"), *GetName());
        }

        // 비헤이비어 트리 실행 (비헤이비어 트리가 설정되었을 때만)
        if (BehaviorTree && AIController)
        {
            AIController->RunBehaviorTree(BehaviorTree);
        }
        else if (!BehaviorTree)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSEnemyBase: BehaviorTree asset not assigned for enemy %s"), *GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSEnemyBase: Failed to cast controller to AHSAIControllerBase for enemy %s"), *GetName());
    }
}

// 전투 컴포넌트 설정
void AHSEnemyBase::SetupCombatComponent()
{
    UHSCombatComponent* Combat = GetCombatComponent();
    if (Combat)
    {
        // 기본 체력 설정
        Combat->SetMaxHealth(100.0f);
        Combat->SetCurrentHealth(100.0f);

        // 이벤트 바인딩
        Combat->OnDamageReceived.AddDynamic(this, &AHSEnemyBase::OnDamageReceived);
        Combat->OnCharacterDied.AddDynamic(this, &AHSEnemyBase::OnDeath);
    }
}

// 감지 컴포넌트 설정
void AHSEnemyBase::SetupSensingComponent()
{
    if (PawnSensingComponent)
    {
        // 감지 범위 및 각도 설정
        PawnSensingComponent->SightRadius = DetectionRange;
        PawnSensingComponent->SetPeripheralVisionAngle(SightAngle);

        // 이벤트 바인딩
        PawnSensingComponent->OnSeePawn.AddDynamic(this, &AHSEnemyBase::OnPawnSeen);
    }
}

// 폰이 감지되었을 때
void AHSEnemyBase::OnPawnSeen(APawn* SeenPawn)
{
    if (!SeenPawn || bInCombat)
    {
        return;
    }

    // 플레이어인지 확인
    if (IsPlayerCharacter(SeenPawn))
    {
        // 살아있는 플레이어만 타겟으로 설정
        UHSCombatComponent* PlayerCombat = SeenPawn->FindComponentByClass<UHSCombatComponent>();
        if (PlayerCombat && PlayerCombat->IsAlive())
        {
            StartCombat(SeenPawn);
        }
    }
}

// 폰을 잃었을 때
void AHSEnemyBase::OnPawnLost(APawn* LostPawn)
{
    if (LostPawn == CurrentTarget)
    {
        // 잠시 후 전투 종료 (즉시 종료하지 않고 조사 상태로 전환)
        SetAIState(EHSEnemyAIState::Investigating);
    }
}

// 데미지를 받았을 때
void AHSEnemyBase::OnDamageReceived(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    // 공격자가 있고 전투 중이 아니라면 전투 시작
    if (DamageInstigator && !bInCombat && IsPlayerCharacter(DamageInstigator))
    {
        StartCombat(DamageInstigator);
    }
}

// 사망했을 때
void AHSEnemyBase::OnDeath(AActor* DeadActor)
{
    SetAIState(EHSEnemyAIState::Dead);
    OnEnemyDeath.Broadcast(this);

    // AI 비활성화
    if (AIController)
    {
        AIController->BrainComponent->StopLogic(TEXT("Dead"));
    }
}

// 공격 쿨다운 만료
void AHSEnemyBase::OnAttackCooldownExpired()
{
    // 여전히 전투 중이라면 추격 상태로 복귀
    if (bInCombat)
    {
        SetAIState(EHSEnemyAIState::Chasing);
    }
}

// 플레이어 캐릭터인지 확인
bool AHSEnemyBase::IsPlayerCharacter(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    // 플레이어 캐릭터 클래스인지 확인
    AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(Actor);
    return PlayerCharacter != nullptr;
}

// 랜덤 순찰 지점 생성
FVector AHSEnemyBase::GetRandomPatrolPoint() const
{
    // 스폰 위치 중심으로 순찰 반지름 내의 랜덤 지점
    FVector RandomDirection = FMath::VRand();
    RandomDirection.Z = 0.0f; // 수평 이동만
    RandomDirection.Normalize();

    float RandomDistance = FMath::RandRange(100.0f, PatrolRadius);
    FVector RandomPoint = SpawnLocation + (RandomDirection * RandomDistance);

    // 네비게이션 메시 상의 유효한 지점인지 확인하고 보정
    FNavLocation NavLocation;
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (NavSystem && NavSystem->ProjectPointToNavigation(RandomPoint, NavLocation, FVector(200.0f, 200.0f, 500.0f)))
    {
        return NavLocation.Location;
    }

    // 유효하지 않으면 스폰 위치 반환
    return SpawnLocation;
}

// 블랙보드 업데이트
void AHSEnemyBase::UpdateBlackboard()
{
    // 블랙보드 컴포넌트 유효성 체크
    if (!BlackboardComponent)
    {
        return;
    }

    // 현재 타겟 설정
    BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), CurrentTarget);

    // 마지막 알려진 플레이어 위치 설정
    BlackboardComponent->SetValueAsVector(TEXT("LastKnownPlayerLocation"), LastKnownPlayerLocation);

    // AI 상태 설정 (열거형을 정수로 변환)
    BlackboardComponent->SetValueAsInt(TEXT("AIState"), static_cast<int32>(CurrentAIState));

    // 전투 상태 설정
    BlackboardComponent->SetValueAsBool(TEXT("InCombat"), bInCombat);

    // 순찰 관련 설정
    BlackboardComponent->SetValueAsVector(TEXT("SpawnLocation"), SpawnLocation);
    BlackboardComponent->SetValueAsVector(TEXT("PatrolTarget"), PatrolTarget);
    BlackboardComponent->SetValueAsBool(TEXT("ShouldPatrol"), bShouldPatrol);
}

// 체력 설정
void AHSEnemyBase::SetHealth(float NewHealth)
{
    CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
    
    // 체력이 0이면 사망 처리
    if (CurrentHealth <= 0.0f && !bIsDead)
    {
        Die();
    }
}

// 최대 체력 설정
void AHSEnemyBase::SetMaxHealth(float NewMaxHealth)
{
    MaxHealth = FMath::Max(1.0f, NewMaxHealth);
    
    // 현재 체력이 최대 체력보다 높으면 조정
    if (CurrentHealth > MaxHealth)
    {
        CurrentHealth = MaxHealth;
    }
}

// 체력 퍼센트 반환
float AHSEnemyBase::GetHealthPercent() const
{
    return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

// 언리얼 엔진의 표준 데미지 처리 함수 오버라이드
float AHSEnemyBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 부모 클래스의 TakeDamage 호출
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    
    // 이미 죽은 상태면 무시
    if (bIsDead || ActualDamage <= 0.0f)
    {
        return 0.0f;
    }
    
    // 체력 감소
    SetHealth(CurrentHealth - ActualDamage);
    
    // 전투 시작 (공격자가 플레이어인 경우)
    if (DamageCauser && !bInCombat && IsPlayerCharacter(DamageCauser))
    {
        StartCombat(DamageCauser);
    }
    
    // 데미지 받음 이벤트 생성
    FHSDamageInfo DamageInfo;
    DamageInfo.BaseDamage = ActualDamage;
    DamageInfo.DamageType = EHSDamageType::Physical;
    OnDamageReceived(ActualDamage, DamageInfo, DamageCauser);
    
    return ActualDamage;
}

// 사망 처리
void AHSEnemyBase::Die()
{
    if (bIsDead)
    {
        return;
    }
    
    bIsDead = true;
    CurrentHealth = 0.0f;
    
    // AI 상태를 사망으로 변경
    SetAIState(EHSEnemyAIState::Dead);
    
    // 사망 이벤트 발생
    OnEnemyDeath.Broadcast(this);
    
    // AI 비활성화
    if (AIController)
    {
        AIController->BrainComponent->StopLogic(TEXT("Dead"));
    }
    
    // 충돌 비활성화
    SetActorEnableCollision(false);
    
    // 움직임 정지
    GetCharacterMovement()->DisableMovement();
    
    // 캡슐 컴포넌트 충돌 비활성화
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // 일정 시간 후 제거 (나중에 오브젝트 풀로 교체 예정)
    SetLifeSpan(5.0f);
}
