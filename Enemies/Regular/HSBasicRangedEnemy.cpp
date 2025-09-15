// HuntingSpirit Game - Basic Ranged Enemy Implementation
// 원거리 공격을 수행하는 기본 적 클래스 구현

#include "HSBasicRangedEnemy.h"
#include "HuntingSpirit/AI/HSAIControllerBase.h"
#include "HuntingSpirit/Combat/Projectiles/HSMagicProjectile.h"
#include "HuntingSpirit/Optimization/ObjectPool/HSObjectPool.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 생성자
AHSBasicRangedEnemy::AHSBasicRangedEnemy()
{
    // 기본 설정
    PrimaryActorTick.bCanEverTick = true;
    
    // 원거리 적 타입 설정
    EnemyType = EHSEnemyType::Ranged;
    EnemyName = TEXT("Basic Ranged Enemy");
    EnemyDescription = TEXT("A basic enemy that attacks from range using projectiles.");
    
    // 원거리 적에 맞는 기본 스탯 설정
    // MaxHealth는 HSCharacterBase에서 상속받음
    AttackDamageInfo.BaseDamage = ProjectileDamage;
    AttackDamageInfo.DamageType = EHSDamageType::Magical;
    
    // 원거리 적에 맞는 탐지 및 공격 범위 설정
    DetectionRange = 1000.0f;
    LoseTargetRange = 1500.0f;
    AttackRange = OptimalAttackRange;
    
    // 이동 속도 설정 (근접 적보다 느림)
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = 400.0f;
    }
}

// BeginPlay
void AHSBasicRangedEnemy::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeRangedEnemy();
    SetupProjectilePool();
    
    // 전술 평가 타이머 시작
    if (bUseDynamicTactics)
    {
        GetWorld()->GetTimerManager().SetTimer(
            TacticsEvaluationTimerHandle,
            this,
            &AHSBasicRangedEnemy::EvaluateTactics,
            TacticsEvaluationInterval,
            true
        );
    }
    
    // 옆걸음 방향 변경 타이머 시작
    if (bEnableStrafing)
    {
        GetWorld()->GetTimerManager().SetTimer(
            StrafeTimerHandle,
            this,
            &AHSBasicRangedEnemy::ChangeStrafeDirection,
            StrafeChangeInterval,
            true
        );
    }
}

// Tick
void AHSBasicRangedEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 전투 중이고 옆걸음이 활성화되어 있다면
    if (IsInCombat() && bEnableStrafing && CurrentTactic == EHSRangedEnemyTactic::Strafe)
    {
        UpdateStrafing(DeltaTime);
    }
}

// 초기화 함수
void AHSBasicRangedEnemy::InitializeRangedEnemy()
{
    // 프로젝타일 클래스가 설정되지 않았다면 기본값 사용
    if (!ProjectileClass)
    {
        ProjectileClass = AHSMagicProjectile::StaticClass();
    }
    
    // 공격 범위 업데이트
    AttackRange = OptimalAttackRange;
}

// 발사체 풀 설정
void AHSBasicRangedEnemy::SetupProjectilePool()
{
    // 월드에서 발사체 풀을 찾거나 생성
    if (!ProjectilePool)
    {
        // 기존 풀 찾기
        TArray<AActor*> FoundPools;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSObjectPool::StaticClass(), FoundPools);
        
        for (AActor* Pool : FoundPools)
        {
            AHSObjectPool* ObjectPool = Cast<AHSObjectPool>(Pool);
            if (ObjectPool && ObjectPool->GetPoolClass() == ProjectileClass)
            {
                ProjectilePool = ObjectPool;
                break;
            }
        }
        
        // 풀이 없다면 새로 생성
        if (!ProjectilePool)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
            ProjectilePool = GetWorld()->SpawnActor<AHSObjectPool>(
                AHSObjectPool::StaticClass(),
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                SpawnParams
            );
            
            if (ProjectilePool)
            {
                ProjectilePool->InitializePool(ProjectileClass, 20, GetWorld()); // 20개의 발사체로 풀 초기화
            }
        }
    }
}

// 전투 시작 오버라이드
void AHSBasicRangedEnemy::StartCombat(AActor* Target)
{
    Super::StartCombat(Target);
    
    // 초기 전술 평가
    EvaluateTacticalSituation();
}

// 전투 종료 오버라이드
void AHSBasicRangedEnemy::EndCombat()
{
    Super::EndCombat();
    
    // 진행 중인 공격 취소
    GetWorld()->GetTimerManager().ClearTimer(BurstFireTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(BarrageTimerHandle);
    
    bIsPerformingAttack = false;
    RemainingBurstShots = 0;
    RemainingBarrageShots = 0;
    
    // 기본 전술로 복귀
    SetTactic(EHSRangedEnemyTactic::KeepDistance);
}

// 공격 수행
void AHSBasicRangedEnemy::PerformAttack()
{
    if (!CurrentTarget || bIsPerformingAttack)
    {
        return;
    }
    
    // 시야 확인
    if (!HasLineOfSight(CurrentTarget))
    {
        return;
    }
    
    // 거리 확인
    float Distance = GetDistanceToTarget(CurrentTarget);
    if (Distance > MaximumAttackRange || Distance < MinimumAttackRange)
    {
        return;
    }
    
    bIsPerformingAttack = true;
    OnRangedAttackStarted.Broadcast(PrimaryAttackType);
    
    // 공격 타입에 따라 다른 공격 수행
    switch (PrimaryAttackType)
    {
        case EHSRangedAttackType::SingleShot:
            FireProjectileAtActor(CurrentTarget);
            bIsPerformingAttack = false;
            OnRangedAttackCompleted.Broadcast();
            break;
            
        case EHSRangedAttackType::Burst:
            PerformBurstFire(BurstShotCount);
            break;
            
        case EHSRangedAttackType::Spread:
            PerformSpreadShot(SpreadProjectileCount, SpreadAngle);
            bIsPerformingAttack = false;
            OnRangedAttackCompleted.Broadcast();
            break;
            
        case EHSRangedAttackType::Barrage:
            PerformBarrage();
            break;
            
        default:
            FireProjectileAtActor(CurrentTarget);
            bIsPerformingAttack = false;
            OnRangedAttackCompleted.Broadcast();
            break;
    }
    
    // 공격 쿨다운 시작
    GetWorld()->GetTimerManager().SetTimer(
        AttackCooldownTimer,
        this,
        &AHSBasicRangedEnemy::OnAttackCooldownExpired,
        AttackCooldown,
        false
    );
}

// 발사체 발사 (위치 지정)
void AHSBasicRangedEnemy::FireProjectile(FVector TargetLocation)
{
    FVector StartLocation = GetProjectileSpawnLocation();
    FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
    
    // 정확도 적용
    float Accuracy = CalculateAccuracy(nullptr);
    Direction = ApplyAccuracySpread(Direction, Accuracy);
    
    // 발사체 생성 및 발사
    AHSMagicProjectile* Projectile = CreateProjectile(StartLocation, Direction);
    if (Projectile)
    {
        LaunchProjectile(Projectile, Direction);
        OnProjectileFired.Broadcast(Projectile, nullptr);
    }
}

// 발사체 발사 (액터 지정)
void AHSBasicRangedEnemy::FireProjectileAtActor(AActor* TargetActor)
{
    if (!TargetActor)
    {
        return;
    }
    
    FVector StartLocation = GetProjectileSpawnLocation();
    FVector TargetLocation = TargetActor->GetActorLocation();
    
    // 타겟의 속도를 고려한 예측 사격
    if (APawn* TargetPawn = Cast<APawn>(TargetActor))
    {
        FVector TargetVelocity = TargetPawn->GetVelocity();
        float Distance = FVector::Dist(StartLocation, TargetLocation);
        float TimeToTarget = Distance / ProjectileSpeed;
        TargetLocation += TargetVelocity * TimeToTarget * 0.8f; // 80% 예측
    }
    
    FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
    
    // 정확도 적용
    float Accuracy = CalculateAccuracy(TargetActor);
    Direction = ApplyAccuracySpread(Direction, Accuracy);
    
    // 발사체 생성 및 발사
    AHSMagicProjectile* Projectile = CreateProjectile(StartLocation, Direction);
    if (Projectile)
    {
        LaunchProjectile(Projectile, Direction);
        OnProjectileFired.Broadcast(Projectile, TargetActor);
    }
}

// 연발 공격
void AHSBasicRangedEnemy::PerformBurstFire(int32 BurstCount)
{
    RemainingBurstShots = BurstCount;
    FireBurstShot();
}

// 산탄 공격
void AHSBasicRangedEnemy::PerformSpreadShot(int32 ProjectileCount, float SpreadAngleParam)
{
    if (!CurrentTarget)
    {
        return;
    }
    
    FVector StartLocation = GetProjectileSpawnLocation();
    FVector BaseDirection = (CurrentTarget->GetActorLocation() - StartLocation).GetSafeNormal();
    
    // 각 발사체의 각도 계산
    float AngleStep = SpreadAngleParam / (ProjectileCount - 1);
    float StartAngle = -SpreadAngleParam / 2.0f;
    
    for (int32 i = 0; i < ProjectileCount; i++)
    {
        // 각도 계산
        float CurrentAngle = StartAngle + (AngleStep * i);
        
        // 방향 회전
        FRotator Rotation = UKismetMathLibrary::MakeRotFromX(BaseDirection);
        Rotation.Yaw += CurrentAngle;
        FVector Direction = Rotation.Vector();
        
        // 정확도 적용 (산탄은 정확도가 낮음)
        float SpreadAccuracy = BaseAccuracy * 0.7f;
        Direction = ApplyAccuracySpread(Direction, SpreadAccuracy);
        
        // 발사체 생성 및 발사
        AHSMagicProjectile* Projectile = CreateProjectile(StartLocation, Direction);
        if (Projectile)
        {
            // 산탄 발사체는 데미지가 낮음
            Projectile->InitializeProjectile(Direction, ProjectileSpeed * 0.8f, ProjectileDamage * 0.6f);
            OnProjectileFired.Broadcast(Projectile, CurrentTarget);
        }
    }
}

// 탄막 공격
void AHSBasicRangedEnemy::PerformBarrage()
{
    RemainingBarrageShots = BarrageProjectileCount;
    FireBarrageShot();
}

// 전술 설정
void AHSBasicRangedEnemy::SetTactic(EHSRangedEnemyTactic NewTactic)
{
    if (CurrentTactic != NewTactic)
    {
        CurrentTactic = NewTactic;
        
        // AI 컨트롤러에 전술 변경 알림
        if (AIController)
        {
            // 블랙보드에 전술 정보 업데이트
            AIController->SetBlackboardValueAsInt(TEXT("RangedTactic"), (int32)CurrentTactic);
        }
    }
}

// 전술 상황 평가
void AHSBasicRangedEnemy::EvaluateTacticalSituation()
{
    if (!CurrentTarget || !bUseDynamicTactics)
    {
        return;
    }
    
    float Distance = GetDistanceToTarget(CurrentTarget);
    bool bHasLOS = HasLineOfSight(CurrentTarget);
    
    // 거리가 너무 가까우면 후퇴
    if (Distance < MinimumAttackRange)
    {
        SetTactic(EHSRangedEnemyTactic::Retreat);
    }
    // 최적 거리에 있고 시야가 확보되면 옆걸음
    else if (IsAtOptimalRange() && bHasLOS && bEnableStrafing)
    {
        SetTactic(EHSRangedEnemyTactic::Strafe);
    }
    // 시야가 없으면 엄폐물 찾기
    else if (!bHasLOS && ShouldSeekCover())
    {
        SetTactic(EHSRangedEnemyTactic::FindCover);
    }
    // 거리가 멀면 접근
    else if (Distance > MaximumAttackRange)
    {
        SetTactic(EHSRangedEnemyTactic::Aggressive);
    }
    // 기본적으로 거리 유지
    else
    {
        SetTactic(EHSRangedEnemyTactic::KeepDistance);
    }
}

// 최적 거리 확인
bool AHSBasicRangedEnemy::IsAtOptimalRange() const
{
    if (!CurrentTarget)
    {
        return false;
    }
    
    float Distance = GetDistanceToTarget(CurrentTarget);
    float OptimalMin = OptimalAttackRange * 0.8f;
    float OptimalMax = OptimalAttackRange * 1.2f;
    
    return Distance >= OptimalMin && Distance <= OptimalMax;
}

// 시야 확인
bool AHSBasicRangedEnemy::HasLineOfSight(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }
    
    FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
    FVector EndLocation = Target->GetActorLocation();
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(Target);
    
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECC_Visibility,
        QueryParams
    );
    
    return !bHit;
}

// 최적 위치 계산
FVector AHSBasicRangedEnemy::GetOptimalPosition(AActor* Target) const
{
    if (!Target)
    {
        return GetActorLocation();
    }
    
    FVector TargetLocation = Target->GetActorLocation();
    FVector Direction = (GetActorLocation() - TargetLocation).GetSafeNormal();
    
    return TargetLocation + (Direction * OptimalAttackRange);
}

// 옆걸음 위치 계산
FVector AHSBasicRangedEnemy::GetStrafePosition(bool bMoveRight) const
{
    if (!CurrentTarget)
    {
        return GetActorLocation();
    }
    
    FVector ToTarget = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FVector RightVector = FVector::CrossProduct(ToTarget, FVector::UpVector).GetSafeNormal();
    
    if (!bMoveRight)
    {
        RightVector *= -1.0f;
    }
    
    return GetActorLocation() + (RightVector * 200.0f);
}

// 발사체 생성
AHSMagicProjectile* AHSBasicRangedEnemy::CreateProjectile(FVector StartLocation, FVector Direction)
{
    if (!ProjectilePool || !ProjectileClass)
    {
        return nullptr;
    }
    
    // 풀에서 발사체 가져오기
    AHSMagicProjectile* Projectile = Cast<AHSMagicProjectile>(ProjectilePool->GetPooledObject());
    
    if (!Projectile)
    {
        // 풀이 비어있으면 직접 생성
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Instigator = this;
        
        Projectile = GetWorld()->SpawnActor<AHSMagicProjectile>(
            ProjectileClass,
            StartLocation,
            Direction.Rotation(),
            SpawnParams
        );
    }
    else
    {
        // 풀에서 가져온 발사체 위치 설정
        Projectile->SetActorLocation(StartLocation);
        Projectile->SetActorRotation(Direction.Rotation());
    }
    
    return Projectile;
}

// 발사체 발사
void AHSBasicRangedEnemy::LaunchProjectile(AHSMagicProjectile* Projectile, FVector Direction)
{
    if (!Projectile)
    {
        return;
    }
    
    // 발사체 초기화
    Projectile->InitializeProjectile(Direction, ProjectileSpeed, ProjectileDamage);
    Projectile->SetOwnerPool(ProjectilePool);
    
    // 발사체 소유자 설정
    Projectile->SetOwner(this);
    Projectile->SetInstigator(this);
}

// 정확도 계산
float AHSBasicRangedEnemy::CalculateAccuracy(AActor* Target) const
{
    float Accuracy = BaseAccuracy;
    
    // 거리에 따른 정확도 감소
    if (Target)
    {
        float Distance = GetDistanceToTarget(Target);
        Accuracy -= (Distance * AccuracyPenaltyPerMeter);
    }
    
    // 이동 중이면 정확도 감소
    if (GetVelocity().Size() > 10.0f)
    {
        Accuracy -= MovementAccuracyPenalty;
    }
    
    return FMath::Clamp(Accuracy, 0.1f, 1.0f);
}

// 정확도에 따른 방향 분산 적용
FVector AHSBasicRangedEnemy::ApplyAccuracySpread(FVector BaseDirection, float Accuracy) const
{
    if (Accuracy >= 1.0f)
    {
        return BaseDirection;
    }
    
    // 분산 각도 계산 (정확도가 낮을수록 분산이 큼)
    float MaxSpreadAngle = 30.0f; // 최대 30도 분산
    float CurrentSpreadAngle = MaxSpreadAngle * (1.0f - Accuracy);
    
    // 랜덤 회전 적용
    float RandomYaw = FMath::RandRange(-CurrentSpreadAngle, CurrentSpreadAngle);
    float RandomPitch = FMath::RandRange(-CurrentSpreadAngle, CurrentSpreadAngle);
    
    FRotator Rotation = BaseDirection.Rotation();
    Rotation.Yaw += RandomYaw;
    Rotation.Pitch += RandomPitch;
    
    return Rotation.Vector();
}

// 연발 사격
void AHSBasicRangedEnemy::FireBurstShot()
{
    if (RemainingBurstShots <= 0 || !CurrentTarget)
    {
        bIsPerformingAttack = false;
        OnRangedAttackCompleted.Broadcast();
        return;
    }
    
    // 발사
    FireProjectileAtActor(CurrentTarget);
    RemainingBurstShots--;
    
    // 다음 발사 예약
    if (RemainingBurstShots > 0)
    {
        GetWorld()->GetTimerManager().SetTimer(
            BurstFireTimerHandle,
            this,
            &AHSBasicRangedEnemy::FireBurstShot,
            BurstShotInterval,
            false
        );
    }
    else
    {
        bIsPerformingAttack = false;
        OnRangedAttackCompleted.Broadcast();
    }
}

// 탄막 사격
void AHSBasicRangedEnemy::FireBarrageShot()
{
    if (RemainingBarrageShots <= 0 || !CurrentTarget)
    {
        bIsPerformingAttack = false;
        OnRangedAttackCompleted.Broadcast();
        return;
    }
    
    // 랜덤 방향으로 발사
    FVector TargetLocation = CurrentTarget->GetActorLocation();
    FVector RandomOffset = FVector(
        FMath::RandRange(-200.0f, 200.0f),
        FMath::RandRange(-200.0f, 200.0f),
        FMath::RandRange(-50.0f, 50.0f)
    );
    
    FireProjectile(TargetLocation + RandomOffset);
    RemainingBarrageShots--;
    
    // 다음 발사 예약
    if (RemainingBarrageShots > 0)
    {
        GetWorld()->GetTimerManager().SetTimer(
            BarrageTimerHandle,
            this,
            &AHSBasicRangedEnemy::FireBarrageShot,
            BarrageInterval,
            false
        );
    }
    else
    {
        bIsPerformingAttack = false;
        OnRangedAttackCompleted.Broadcast();
    }
}

// 전술 평가 타이머
void AHSBasicRangedEnemy::EvaluateTactics()
{
    EvaluateTacticalSituation();
}

// 옆걸음 방향 변경
void AHSBasicRangedEnemy::ChangeStrafeDirection()
{
    bStrafingRight = !bStrafingRight;
}

// 옆걸음 이동 업데이트
void AHSBasicRangedEnemy::UpdateStrafing(float DeltaTime)
{
    if (!CurrentTarget || CurrentTactic != EHSRangedEnemyTactic::Strafe)
    {
        return;
    }
    
    // 타겟을 바라보면서 옆으로 이동
    FVector ToTarget = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FVector RightVector = FVector::CrossProduct(ToTarget, FVector::UpVector).GetSafeNormal();
    
    if (!bStrafingRight)
    {
        RightVector *= -1.0f;
    }
    
    // 이동 방향으로 이동
    AddMovementInput(RightVector, 1.0f);
    
    // 타겟을 계속 바라보도록 회전
    FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), CurrentTarget->GetActorLocation());
    SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));
}

// 발사체 생성 위치
FVector AHSBasicRangedEnemy::GetProjectileSpawnLocation() const
{
    return GetActorLocation() + GetActorRotation().RotateVector(ProjectileSpawnOffset);
}

// 후퇴 필요 여부
bool AHSBasicRangedEnemy::ShouldRetreat() const
{
    if (!CurrentTarget)
    {
        return false;
    }
    
    return GetDistanceToTarget(CurrentTarget) < MinimumAttackRange;
}

// 엄폐물 찾기 필요 여부
bool AHSBasicRangedEnemy::ShouldSeekCover() const
{
    // 체력이 50% 이하면 엄폐물 찾기 고려
    if (CombatComponent)
    {
        return CombatComponent->GetHealthPercentage() < 0.5f;
    }
    
    return false;
}

// 타겟까지의 거리
float AHSBasicRangedEnemy::GetTargetDistance() const
{
    if (!CurrentTarget)
    {
        return 0.0f;
    }
    
    return FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
}
