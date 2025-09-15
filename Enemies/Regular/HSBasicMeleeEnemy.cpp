// HuntingSpirit Game - Basic Melee Enemy Implementation
// 기본 근접 공격 적 구현

#include "HSBasicMeleeEnemy.h"
#include "HuntingSpirit/AI/HSAIControllerBase.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Combat/HSHitReactionComponent.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"

// 생성자
AHSBasicMeleeEnemy::AHSBasicMeleeEnemy()
{
    // 기본 설정
    EnemyType = EHSEnemyType::Melee;
    EnemyName = TEXT("Basic Melee Enemy");
    EnemyDescription = TEXT("A basic melee enemy that attacks with close-range physical strikes.");

    // 기본 스탯 설정
    AttackRange = 150.0f;
    DetectionRange = 600.0f;
    AttackCooldown = 1.5f;

    // 근접 공격 기본 설정
    MeleeAttackAngle = 60.0f;
    MeleeKnockbackForce = 500.0f;
    bCanCombo = true;
    MaxComboCount = 3;

    // 돌진 공격 설정
    ChargeSpeed = 800.0f;
    ChargeDistance = 500.0f;
    ChargeDamageMultiplier = 1.5f;
    ChargePreparationTime = 0.5f;

    // 회전 공격 설정
    SpinAttackRadius = 200.0f;
    SpinAttackDuration = 1.0f;
    SpinAttackHitCount = 3;

    // 지면 강타 설정
    GroundSlamRadius = 300.0f;
    GroundSlamStunDuration = 1.5f;
    bGroundSlamCreatesShockwave = true;

    // 이동 속도 설정
    GetCharacterMovement()->MaxWalkSpeed = 350.0f;
}

// 게임 시작 시 호출
void AHSBasicMeleeEnemy::BeginPlay()
{
    Super::BeginPlay();

    // 근접 공격 패턴 초기화
    InitializeMeleeAttackPatterns();
}

// 매 프레임 호출
void AHSBasicMeleeEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 디버그 표시
#if WITH_EDITOR
    if (GetWorld() && GetWorld()->IsPlayInEditor())
    {
        DrawDebugMeleeAttack(AttackRange, MeleeAttackAngle);
    }
#endif
}

// 공격 수행 오버라이드
void AHSBasicMeleeEnemy::PerformAttack()
{
    if (!CanPerformMeleeAttack())
    {
        return;
    }

    // 공격 패턴 선택
    EHSMeleeAttackPattern SelectedPattern = SelectAttackPattern();
    
    // 선택된 패턴으로 공격 수행
    PerformMeleeAttack(SelectedPattern);
}

// 근접 공격 수행
void AHSBasicMeleeEnemy::PerformMeleeAttack(EHSMeleeAttackPattern Pattern)
{
    if (bIsAttacking || !CurrentTarget)
    {
        return;
    }

    bIsAttacking = true;
    bInCombat = true;

    // 타겟 방향으로 회전
    FVector DirectionToTarget = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    DirectionToTarget.Z = 0.0f;
    SetActorRotation(DirectionToTarget.Rotation());

    // 패턴에 따른 공격 실행
    switch (Pattern)
    {
    case EHSMeleeAttackPattern::SingleStrike:
        ExecuteSingleStrike();
        break;
    case EHSMeleeAttackPattern::DoubleStrike:
        ExecuteDoubleStrike();
        break;
    case EHSMeleeAttackPattern::SpinAttack:
        ExecuteSpinAttack();
        break;
    case EHSMeleeAttackPattern::ChargeAttack:
        ExecuteChargeAttack();
        break;
    case EHSMeleeAttackPattern::GroundSlam:
        ExecuteGroundSlam();
        break;
    }
}

// 단일 타격 실행
void AHSBasicMeleeEnemy::ExecuteSingleStrike()
{
    // 애니메이션 재생
    if (MeleeAttackPatterns.Num() > 0 && MeleeAttackPatterns[0].AttackAnimation)
    {
        PlayAnimMontage(MeleeAttackPatterns[0].AttackAnimation);
    }

    // 데미지 적용
    FTimerHandle DamageTimer;
    GetWorldTimerManager().SetTimer(DamageTimer, [this]()
    {
        if (MeleeAttackPatterns.Num() > 0)
        {
            ApplyMeleeDamage(MeleeAttackPatterns[0]);
        }
    }, 0.3f, false);

    // 공격 완료 타이머
    FTimerHandle AttackCompleteTimer;
    GetWorldTimerManager().SetTimer(AttackCompleteTimer, this, &AHSBasicMeleeEnemy::OnAttackAnimationFinished, 1.0f, false);

    // 콤보 카운트 증가
    if (bCanCombo)
    {
        CurrentComboCount++;
        GetWorldTimerManager().SetTimer(ComboResetTimer, this, &AHSBasicMeleeEnemy::OnComboWindowExpired, 1.5f, false);
    }
}

// 이중 타격 실행
void AHSBasicMeleeEnemy::ExecuteDoubleStrike()
{
    // 첫 번째 타격
    ExecuteSingleStrike();

    // 두 번째 타격 예약
    FTimerHandle SecondStrikeTimer;
    GetWorldTimerManager().SetTimer(SecondStrikeTimer, [this]()
    {
        if (CurrentTarget && IsTargetInMeleeRange())
        {
            // 두 번째 타격 애니메이션
            if (MeleeAttackPatterns.Num() > 1 && MeleeAttackPatterns[1].AttackAnimation)
            {
                PlayAnimMontage(MeleeAttackPatterns[1].AttackAnimation);
            }

            // 두 번째 타격 데미지
            FTimerHandle DamageTimer;
            GetWorldTimerManager().SetTimer(DamageTimer, [this]()
            {
                if (MeleeAttackPatterns.Num() > 1)
                {
                    ApplyMeleeDamage(MeleeAttackPatterns[1]);
                }
            }, 0.3f, false);
        }
    }, 0.6f, false);

    // 공격 완료 타이머 (두 타격 모두 완료 후)
    FTimerHandle AttackCompleteTimer;
    GetWorldTimerManager().SetTimer(AttackCompleteTimer, this, &AHSBasicMeleeEnemy::OnAttackAnimationFinished, 1.8f, false);
}

// 회전 공격 실행
void AHSBasicMeleeEnemy::ExecuteSpinAttack()
{
    // 회전 공격 애니메이션
    if (MeleeAttackPatterns.Num() > 2 && MeleeAttackPatterns[2].AttackAnimation)
    {
        PlayAnimMontage(MeleeAttackPatterns[2].AttackAnimation);
    }

    // 회전 공격 틱 시작
    int32 HitCount = 0;
    GetWorldTimerManager().SetTimer(SpinTickTimer, [this, HitCount]() mutable
    {
        if (HitCount < SpinAttackHitCount)
        {
            PerformSpinAttackTick();
            HitCount++;
        }
    }, SpinAttackDuration / SpinAttackHitCount, true);

    // 공격 완료 타이머
    FTimerHandle AttackCompleteTimer;
    GetWorldTimerManager().SetTimer(AttackCompleteTimer, [this]()
    {
        GetWorldTimerManager().ClearTimer(SpinTickTimer);
        OnAttackAnimationFinished();
    }, SpinAttackDuration, false);
}

// 돌진 공격 실행
void AHSBasicMeleeEnemy::ExecuteChargeAttack()
{
    if (!CurrentTarget)
    {
        OnAttackAnimationFinished();
        return;
    }

    // 돌진 준비
    bIsCharging = true;
    GetCharacterMovement()->MaxWalkSpeed = 0.0f; // 준비 중 이동 불가

    // 돌진 방향 계산
    FVector ChargeDirection = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    ChargeDirection.Z = 0.0f;
    ChargeTargetLocation = GetActorLocation() + (ChargeDirection * ChargeDistance);

    // 돌진 준비 애니메이션
    if (MeleeAttackPatterns.Num() > 3 && MeleeAttackPatterns[3].AttackAnimation)
    {
        PlayAnimMontage(MeleeAttackPatterns[3].AttackAnimation);
    }

    // 돌진 시작 타이머
    GetWorldTimerManager().SetTimer(ChargeTimer, this, &AHSBasicMeleeEnemy::OnChargePreparationComplete, ChargePreparationTime, false);
}

// 지면 강타 실행
void AHSBasicMeleeEnemy::ExecuteGroundSlam()
{
    // 지면 강타 애니메이션
    if (MeleeAttackPatterns.Num() > 4 && MeleeAttackPatterns[4].AttackAnimation)
    {
        PlayAnimMontage(MeleeAttackPatterns[4].AttackAnimation);
    }

    // 지면 강타 데미지 타이밍
    FTimerHandle SlamTimer;
    GetWorldTimerManager().SetTimer(SlamTimer, [this]()
    {
        // 범위 내 모든 적에게 데미지
        TArray<AActor*> HitActors = GetHitActorsInMeleeRange(GroundSlamRadius, 360.0f);
        
        for (AActor* HitActor : HitActors)
        {
            if (AHSCharacterBase* Character = Cast<AHSCharacterBase>(HitActor))
            {
                // 데미지 적용
                FHSDamageInfo DamageInfo = AttackDamageInfo;
                DamageInfo.BaseDamage *= 1.5f; // 지면 강타는 더 강한 데미지
                
                UGameplayStatics::ApplyPointDamage(
                    Character,
                    DamageInfo.BaseDamage,
                    GetActorLocation(),
                    FHitResult(),
                    GetController(),
                    this,
                    UDamageType::StaticClass()
                );

                // 스턴 효과
                if (UHSCombatComponent* CombatComp = Character->FindComponentByClass<UHSCombatComponent>())
                {
                    // 스턴 효과 적용 (구현 필요)
                }

                // 넉백 적용
                ApplyKnockback(Character, MeleeKnockbackForce * 2.0f);
            }
        }

        // 충격파 효과
        if (bGroundSlamCreatesShockwave)
        {
            PlayMeleeAttackEffects(GetActorLocation());
        }
    }, 0.5f, false);

    // 공격 완료 타이머
    FTimerHandle AttackCompleteTimer;
    GetWorldTimerManager().SetTimer(AttackCompleteTimer, this, &AHSBasicMeleeEnemy::OnAttackAnimationFinished, 1.5f, false);
}

// 공격 가능 여부 확인
bool AHSBasicMeleeEnemy::CanPerformMeleeAttack() const
{
    return !bIsAttacking && !bIsCharging && CurrentTarget && IsTargetInMeleeRange();
}

// 타겟이 근접 범위 내에 있는지 확인
bool AHSBasicMeleeEnemy::IsTargetInMeleeRange() const
{
    if (!CurrentTarget)
    {
        return false;
    }

    float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
    return Distance <= AttackRange;
}

// 현재 타겟까지의 거리 반환
float AHSBasicMeleeEnemy::GetDistanceToCurrentTarget() const
{
    if (!CurrentTarget)
    {
        return MAX_FLT;
    }

    return FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
}

// 공격 패턴 선택
EHSMeleeAttackPattern AHSBasicMeleeEnemy::SelectAttackPattern()
{
    // 콤보 중이면 콤보 공격 계속
    if (bCanCombo && CurrentComboCount > 0 && CurrentComboCount < MaxComboCount)
    {
        return EHSMeleeAttackPattern::SingleStrike;
    }

    // 거리와 상황에 따른 패턴 선택
    float DistanceToTarget = GetDistanceToCurrentTarget();

    // 매우 가까운 거리: 회전 공격 또는 지면 강타
    if (DistanceToTarget < 100.0f)
    {
        return FMath::RandBool() ? EHSMeleeAttackPattern::SpinAttack : EHSMeleeAttackPattern::GroundSlam;
    }
    // 중간 거리: 일반 공격
    else if (DistanceToTarget < AttackRange)
    {
        return FMath::RandBool() ? EHSMeleeAttackPattern::SingleStrike : EHSMeleeAttackPattern::DoubleStrike;
    }
    // 약간 먼 거리: 돌진 공격
    else if (DistanceToTarget < ChargeDistance)
    {
        return EHSMeleeAttackPattern::ChargeAttack;
    }

    // 기본값
    return EHSMeleeAttackPattern::SingleStrike;
}

// 돌진 시작
void AHSBasicMeleeEnemy::StartCharging(const FVector& ChargeDirection)
{
    if (bIsCharging)
    {
        return;
    }

    bIsCharging = true;
    GetCharacterMovement()->MaxWalkSpeed = ChargeSpeed;
    
    // 돌진 방향으로 이동
    GetCharacterMovement()->Velocity = ChargeDirection * ChargeSpeed;
}

// 돌진 중지
void AHSBasicMeleeEnemy::StopCharging()
{
    bIsCharging = false;
    GetCharacterMovement()->MaxWalkSpeed = 350.0f; // 기본 속도로 복구
    GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

// 콤보 리셋
void AHSBasicMeleeEnemy::ResetCombo()
{
    CurrentComboCount = 0;
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
}

// 근접 공격 패턴 초기화
void AHSBasicMeleeEnemy::InitializeMeleeAttackPatterns()
{
    // 기본 공격 패턴 설정
    if (MeleeAttackPatterns.Num() == 0)
    {
        // 단일 타격
        FHSMeleeAttackInfo SingleStrike;
        SingleStrike.AttackPattern = EHSMeleeAttackPattern::SingleStrike;
        SingleStrike.AttackDamage = 10.0f;
        SingleStrike.AttackRange = 150.0f;
        SingleStrike.AttackDuration = 1.0f;
        SingleStrike.AttackCooldown = 1.5f;
        MeleeAttackPatterns.Add(SingleStrike);

        // 이중 타격
        FHSMeleeAttackInfo DoubleStrike;
        DoubleStrike.AttackPattern = EHSMeleeAttackPattern::DoubleStrike;
        DoubleStrike.AttackDamage = 8.0f;
        DoubleStrike.AttackRange = 150.0f;
        DoubleStrike.AttackDuration = 1.8f;
        DoubleStrike.AttackCooldown = 2.0f;
        MeleeAttackPatterns.Add(DoubleStrike);

        // 회전 공격
        FHSMeleeAttackInfo SpinAttack;
        SpinAttack.AttackPattern = EHSMeleeAttackPattern::SpinAttack;
        SpinAttack.AttackDamage = 6.0f;
        SpinAttack.AttackRange = 200.0f;
        SpinAttack.AttackDuration = 1.0f;
        SpinAttack.AttackCooldown = 3.0f;
        MeleeAttackPatterns.Add(SpinAttack);

        // 돌진 공격
        FHSMeleeAttackInfo ChargeAttack;
        ChargeAttack.AttackPattern = EHSMeleeAttackPattern::ChargeAttack;
        ChargeAttack.AttackDamage = 15.0f;
        ChargeAttack.AttackRange = 100.0f;
        ChargeAttack.AttackDuration = 2.0f;
        ChargeAttack.AttackCooldown = 4.0f;
        MeleeAttackPatterns.Add(ChargeAttack);

        // 지면 강타
        FHSMeleeAttackInfo GroundSlam;
        GroundSlam.AttackPattern = EHSMeleeAttackPattern::GroundSlam;
        GroundSlam.AttackDamage = 20.0f;
        GroundSlam.AttackRange = 300.0f;
        GroundSlam.AttackDuration = 1.5f;
        GroundSlam.AttackCooldown = 5.0f;
        MeleeAttackPatterns.Add(GroundSlam);
    }

    // 기본 데미지 정보 설정
    AttackDamageInfo.BaseDamage = 10.0f;
    AttackDamageInfo.DamageType = EHSDamageType::Physical;
    AttackDamageInfo.CriticalChance = 0.1f;
    AttackDamageInfo.CriticalMultiplier = 1.5f;
}

// 근접 데미지 적용
void AHSBasicMeleeEnemy::ApplyMeleeDamage(const FHSMeleeAttackInfo& AttackInfo)
{
    TArray<AActor*> HitActors = GetHitActorsInMeleeRange(AttackInfo.AttackRange, MeleeAttackAngle);

    for (AActor* HitActor : HitActors)
    {
        if (AHSCharacterBase* Character = Cast<AHSCharacterBase>(HitActor))
        {
            // 데미지 정보 설정
            FHSDamageInfo DamageInfo = AttackDamageInfo;
            DamageInfo.BaseDamage = AttackInfo.AttackDamage;

            // 데미지 적용
            UGameplayStatics::ApplyPointDamage(
                Character,
                DamageInfo.BaseDamage,
                GetActorLocation(),
                FHitResult(),
                GetController(),
                this,
                UDamageType::StaticClass()
            );

            // 넉백 적용
            ApplyKnockback(Character, MeleeKnockbackForce);

            // 공격 효과 재생
            PlayMeleeAttackEffects(Character->GetActorLocation());

            // 이벤트 발생
            OnEnemyDamageDealt.Broadcast(DamageInfo.BaseDamage, Character);
        }
    }
}

// 근접 범위 내 액터 검색
TArray<AActor*> AHSBasicMeleeEnemy::GetHitActorsInMeleeRange(float Range, float Angle)
{
    TArray<AActor*> HitActors;
    TArray<FHitResult> HitResults;

    // 구체 스윕으로 범위 내 액터 검색
    FVector Start = GetActorLocation();
    FVector End = Start + GetActorForwardVector() * Range;

    FCollisionShape CollisionShape;
    CollisionShape.SetSphere(Range * 0.5f);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    GetWorld()->SweepMultiByChannel(
        HitResults,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        CollisionShape,
        QueryParams
    );

    // 각도 체크
    for (const FHitResult& Hit : HitResults)
    {
        if (AActor* HitActor = Hit.GetActor())
        {
            // 플레이어 캐릭터만 대상으로
            if (HitActor->IsA<AHSPlayerCharacter>())
            {
                // 각도 체크
                FVector DirectionToTarget = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                float DotProduct = FVector::DotProduct(GetActorForwardVector(), DirectionToTarget);
                float AttackAngleRad = FMath::DegreesToRadians(Angle * 0.5f);

                if (DotProduct >= FMath::Cos(AttackAngleRad))
                {
                    HitActors.Add(HitActor);
                }
            }
        }
    }

    return HitActors;
}

// 넉백 적용
void AHSBasicMeleeEnemy::ApplyKnockback(AActor* Target, float Force)
{
    if (ACharacter* Character = Cast<ACharacter>(Target))
    {
        FVector KnockbackDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        KnockbackDirection.Z = 0.3f; // 약간 위로 띄우기
        KnockbackDirection.Normalize();

        Character->LaunchCharacter(KnockbackDirection * Force, true, false);
    }
}

// 근접 공격 효과 재생
void AHSBasicMeleeEnemy::PlayMeleeAttackEffects(const FVector& Location)
{
    // 파티클 효과
    if (MeleeAttackEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MeleeAttackEffect, Location);
    }

    // 사운드 효과
    if (MeleeAttackSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), MeleeAttackSound, Location);
    }

    // 카메라 흔들림
    if (MeleeAttackCameraShake)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && PC->PlayerCameraManager)
        {
            PC->PlayerCameraManager->StartCameraShake(MeleeAttackCameraShake);
        }
    }
}

// 공격 애니메이션 완료
void AHSBasicMeleeEnemy::OnAttackAnimationFinished()
{
    bIsAttacking = false;
    StopAnimMontage();

    // 공격 쿨다운 시작
    GetWorldTimerManager().SetTimer(AttackCooldownTimer, this, &AHSBasicMeleeEnemy::OnAttackCooldownExpired, AttackCooldown, false);
}

// 콤보 윈도우 만료
void AHSBasicMeleeEnemy::OnComboWindowExpired()
{
    ResetCombo();
}

// 돌진 준비 완료
void AHSBasicMeleeEnemy::OnChargePreparationComplete()
{
    if (!CurrentTarget)
    {
        StopCharging();
        OnAttackAnimationFinished();
        return;
    }

    // 돌진 시작
    FVector ChargeDirection = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    ChargeDirection.Z = 0.0f;
    StartCharging(ChargeDirection);

    // 돌진 완료 타이머
    GetWorldTimerManager().SetTimer(ChargeTimer, this, &AHSBasicMeleeEnemy::OnChargeComplete, 1.0f, false);
}

// 돌진 완료
void AHSBasicMeleeEnemy::OnChargeComplete()
{
    StopCharging();
    
    // 돌진 종료 위치에서 데미지 적용
    if (MeleeAttackPatterns.Num() > 3)
    {
        FHSMeleeAttackInfo ChargeAttackInfo = MeleeAttackPatterns[3];
        ChargeAttackInfo.AttackDamage *= ChargeDamageMultiplier;
        ApplyMeleeDamage(ChargeAttackInfo);
    }

    OnAttackAnimationFinished();
}

// 회전 공격 틱
void AHSBasicMeleeEnemy::PerformSpinAttackTick()
{
    // 회전
    FRotator CurrentRotation = GetActorRotation();
    CurrentRotation.Yaw += 120.0f; // 120도씩 회전
    SetActorRotation(CurrentRotation);

    // 데미지 적용
    if (MeleeAttackPatterns.Num() > 2)
    {
        ApplyMeleeDamage(MeleeAttackPatterns[2]);
    }
}

// 돌진 중 충돌
void AHSBasicMeleeEnemy::OnChargeHit(AActor* HitActor)
{
    if (bIsCharging && HitActor && HitActor->IsA<AHSPlayerCharacter>())
    {
        // 돌진 데미지 적용
        if (MeleeAttackPatterns.Num() > 3)
        {
            FHSMeleeAttackInfo ChargeAttackInfo = MeleeAttackPatterns[3];
            ChargeAttackInfo.AttackDamage *= ChargeDamageMultiplier;
            
            // 단일 대상에게만 데미지
            FHSDamageInfo DamageInfo = AttackDamageInfo;
            DamageInfo.BaseDamage = ChargeAttackInfo.AttackDamage;

            UGameplayStatics::ApplyPointDamage(
                HitActor,
                DamageInfo.BaseDamage,
                GetActorLocation(),
                FHitResult(),
                GetController(),
                this,
                UDamageType::StaticClass()
            );

            // 강한 넉백
            ApplyKnockback(HitActor, MeleeKnockbackForce * 2.0f);
        }

        // 돌진 중단
        StopCharging();
        OnAttackAnimationFinished();
    }
}

// 디버그 표시
void AHSBasicMeleeEnemy::DrawDebugMeleeAttack(float Range, float Angle)
{
    if (!GetWorld())
    {
        return;
    }

    FVector Start = GetActorLocation();
    float AngleRad = FMath::DegreesToRadians(Angle * 0.5f);

    // 공격 범위 호 그리기
    for (int32 i = -1; i <= 1; i += 2)
    {
        FVector Direction = GetActorForwardVector().RotateAngleAxis(Angle * 0.5f * i, FVector::UpVector);
        FVector End = Start + Direction * Range;
        DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.1f);
    }

    // 공격 범위 호 그리기
    int32 Segments = 10;
    for (int32 i = 0; i < Segments; i++)
    {
        float Angle1 = -AngleRad + (2 * AngleRad * i / Segments);
        float Angle2 = -AngleRad + (2 * AngleRad * (i + 1) / Segments);

        FVector Dir1 = GetActorForwardVector().RotateAngleAxis(FMath::RadiansToDegrees(Angle1), FVector::UpVector);
        FVector Dir2 = GetActorForwardVector().RotateAngleAxis(FMath::RadiansToDegrees(Angle2), FVector::UpVector);

        FVector End1 = Start + Dir1 * Range;
        FVector End2 = Start + Dir2 * Range;

        DrawDebugLine(GetWorld(), End1, End2, FColor::Red, false, 0.1f);
    }
}
