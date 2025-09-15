// HuntingSpirit Game - Hit Reaction Component Implementation

#include "HSHitReactionComponent.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Camera/CameraShakeBase.h"
#include "Sound/SoundBase.h"

// 생성자
UHSHitReactionComponent::UHSHitReactionComponent()
{
    // Tick 비활성화 (필요시에만 활성화)
    PrimaryComponentTick.bCanEverTick = false;

    // 기본값 설정
    bEnableHitReactions = true;
    bEnableKnockback = true;
    bEnableHitStop = true;
    bEnableCameraShake = true;
    KnockbackResistance = 0.0f;
    HitStopResistance = 0.0f;

    // 상태 초기화
    bIsKnockedBack = false;
    bIsHitStopped = false;
    OriginalTimeDilation = 1.0f;

    // 오너 캐릭터 캐시 초기화
    OwnerCharacter = nullptr;
}

// 컴포넌트 초기화
void UHSHitReactionComponent::BeginPlay()
{
    Super::BeginPlay();

    // 오너 캐릭터 캐시
    OwnerCharacter = Cast<AHSCharacterBase>(GetOwner());

    // 원본 시간 배속 저장
    if (OwnerCharacter)
    {
        OriginalTimeDilation = OwnerCharacter->CustomTimeDilation;
    }
}

// 피격 반응 처리 메인 함수
void UHSHitReactionComponent::ProcessHitReaction(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    if (!bEnableHitReactions || !OwnerCharacter || !DamageInstigator)
    {
        return;
    }

    // 피격 방향 계산
    EHSHitDirection HitDirection = CalculateHitDirection(DamageInstigator);

    // 피격 반응 이벤트 발생
    OnHitReactionTriggered.Broadcast(DamageInfo, DamageInstigator, HitDirection);

    // 피격 위치 계산 (캐릭터 중심으로부터 공격자 방향)
    FVector HitLocation = OwnerCharacter->GetActorLocation();
    if (DamageInstigator)
    {
        FVector DirectionToInstigator = (DamageInstigator->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
        HitLocation += DirectionToInstigator * OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
    }

    // 피격 효과 재생
    PlayHitEffect(DamageInfo, HitLocation);

    // 피격 사운드 재생
    PlayHitSound(DamageInfo.DamageType);

    // 피격 애니메이션 재생
    PlayHitAnimation(HitDirection, DamageInfo.DamageType);

    // 넉백 적용
    if (bEnableKnockback && DamageInfo.KnockbackForce > 0.0f)
    {
        FVector KnockbackDirection = CalculateKnockbackDirection(DamageInstigator);
        ApplyKnockback(KnockbackDirection, DamageInfo.KnockbackForce, DamageInfo.KnockbackDuration);
    }

    // 히트스톱 적용
    if (bEnableHitStop && DamageInfo.HitStopDuration > 0.0f)
    {
        ApplyHitStop(DamageInfo.HitStopDuration);
    }

    // 카메라 쉐이크 적용
    if (bEnableCameraShake && DamageInfo.CameraShakeIntensity > 0.0f)
    {
        ApplyCameraShake(DamageInfo.CameraShakeIntensity);
    }
}

// 넉백 적용 함수
void UHSHitReactionComponent::ApplyKnockback(FVector KnockbackDirection, float Force, float Duration)
{
    if (!OwnerCharacter || bIsKnockedBack || Force <= 0.0f)
    {
        return;
    }

    // 넉백 저항력 적용
    float EffectiveForce = Force * (1.0f - KnockbackResistance);
    if (EffectiveForce <= 0.0f)
    {
        return;
    }

    UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
    if (!MovementComp)
    {
        return;
    }

    // 현재 속도 저장
    OriginalVelocity = MovementComp->Velocity;

    // 넉백 상태 설정
    bIsKnockedBack = true;

    // 넉백 방향이 정규화되지 않은 경우 정규화
    KnockbackDirection.Normalize();

    // 넉백 적용 (Z축 제거하여 수평 넉백만 적용)
    FVector HorizontalKnockback = FVector(KnockbackDirection.X, KnockbackDirection.Y, 0.0f) * EffectiveForce;
    MovementComp->AddImpulse(HorizontalKnockback, true);

    // 넉백 이벤트 발생
    OnKnockbackApplied.Broadcast(KnockbackDirection, EffectiveForce);

    // 넉백 종료 타이머 설정
    if (Duration > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(KnockbackTimerHandle, this, &UHSHitReactionComponent::EndKnockback, Duration, false);
    }
}

// 히트스톱 적용 함수
void UHSHitReactionComponent::ApplyHitStop(float Duration)
{
    if (!OwnerCharacter || bIsHitStopped || Duration <= 0.0f)
    {
        return;
    }

    // 히트스톱 저항력 적용
    float EffectiveDuration = Duration * (1.0f - HitStopResistance);
    if (EffectiveDuration <= 0.0f)
    {
        return;
    }

    // 히트스톱 상태 설정
    bIsHitStopped = true;

    // 시간 배속 감소 (거의 정지에 가깝게)
    OwnerCharacter->CustomTimeDilation = 0.1f;

    // 히트스톱 종료 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(HitStopTimerHandle, this, &UHSHitReactionComponent::EndHitStop, EffectiveDuration, false);
}

// 카메라 쉐이크 적용 함수
void UHSHitReactionComponent::ApplyCameraShake(float Intensity)
{
    if (!OwnerCharacter || Intensity <= 0.0f)
    {
        return;
    }

    // 플레이어 캐릭터인 경우에만 카메라 쉐이크 적용
    APlayerController* PlayerController = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PlayerController)
    {
        return;
    }

    // 강도에 따른 카메라 쉐이크 클래스 선택
    TSubclassOf<UCameraShakeBase> ShakeClass = GetCameraShakeForIntensity(Intensity);
    if (ShakeClass)
    {
        PlayerController->ClientStartCameraShake(ShakeClass, Intensity);
    }
}

// 피격 방향 계산 함수
EHSHitDirection UHSHitReactionComponent::CalculateHitDirection(AActor* DamageInstigator) const
{
    if (!OwnerCharacter || !DamageInstigator)
    {
        return EHSHitDirection::Front;
    }

    // 공격자와의 각도 계산
    float Angle = CalculateAngleBetweenActors(DamageInstigator);

    // 각도에 따른 방향 결정 (45도씩 구분)
    if (Angle >= -45.0f && Angle <= 45.0f)
    {
        return EHSHitDirection::Front;
    }
    else if (Angle > 45.0f && Angle <= 135.0f)
    {
        return EHSHitDirection::Right;
    }
    else if (Angle > 135.0f || Angle <= -135.0f)
    {
        return EHSHitDirection::Back;
    }
    else
    {
        return EHSHitDirection::Left;
    }
}

// 피격 효과 재생 함수
void UHSHitReactionComponent::PlayHitEffect(const FHSDamageInfo& DamageInfo, const FVector& HitLocation)
{
    if (!OwnerCharacter)
    {
        return;
    }

    UParticleSystem* HitEffect = nullptr;

    // 크리티컬 히트인 경우 전용 효과 사용
    if (DamageInfo.IsCriticalHit() && CriticalHitEffect)
    {
        HitEffect = CriticalHitEffect;
    }
    else
    {
        HitEffect = GetHitEffectForDamageType(DamageInfo.DamageType);
    }

    // 파티클 효과 재생
    if (HitEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitEffect, HitLocation);
    }
}

// 피격 사운드 재생 함수
void UHSHitReactionComponent::PlayHitSound(EHSDamageType DamageType)
{
    USoundBase* HitSound = GetHitSoundForDamageType(DamageType);
    
    if (HitSound && OwnerCharacter)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), HitSound, OwnerCharacter->GetActorLocation());
    }
}

// 피격 애니메이션 재생 함수
void UHSHitReactionComponent::PlayHitAnimation(EHSHitDirection HitDirection, EHSDamageType DamageType)
{
    if (!OwnerCharacter)
    {
        return;
    }

    UAnimMontage* HitMontage = GetHitAnimationForDirection(HitDirection);
    
    if (HitMontage)
    {
        UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
        if (AnimInstance && !AnimInstance->Montage_IsPlaying(HitMontage))
        {
            AnimInstance->Montage_Play(HitMontage);
        }
    }
}

// 넉백 종료 함수
void UHSHitReactionComponent::EndKnockback()
{
    bIsKnockedBack = false;
    GetWorld()->GetTimerManager().ClearTimer(KnockbackTimerHandle);
}

// 히트스톱 종료 함수
void UHSHitReactionComponent::EndHitStop()
{
    if (OwnerCharacter)
    {
        OwnerCharacter->CustomTimeDilation = OriginalTimeDilation;
    }
    
    bIsHitStopped = false;
    GetWorld()->GetTimerManager().ClearTimer(HitStopTimerHandle);
}

// 데미지 타입별 피격 효과 가져오기
UParticleSystem* UHSHitReactionComponent::GetHitEffectForDamageType(EHSDamageType DamageType) const
{
    switch (DamageType)
    {
        case EHSDamageType::Physical:   return PhysicalHitEffect;
        case EHSDamageType::Magical:    return MagicalHitEffect;
        case EHSDamageType::Fire:       return FireHitEffect;
        case EHSDamageType::Ice:        return IceHitEffect;
        case EHSDamageType::Lightning:  return LightningHitEffect;
        default:                        return PhysicalHitEffect;
    }
}

// 데미지 타입별 피격 사운드 가져오기
USoundBase* UHSHitReactionComponent::GetHitSoundForDamageType(EHSDamageType DamageType) const
{
    switch (DamageType)
    {
        case EHSDamageType::Physical:   return PhysicalHitSound;
        case EHSDamageType::Magical:    return MagicalHitSound;
        case EHSDamageType::Fire:       return FireHitSound;
        case EHSDamageType::Ice:        return IceHitSound;
        case EHSDamageType::Lightning:  return LightningHitSound;
        default:                        return PhysicalHitSound;
    }
}

// 피격 방향별 애니메이션 가져오기
UAnimMontage* UHSHitReactionComponent::GetHitAnimationForDirection(EHSHitDirection Direction) const
{
    switch (Direction)
    {
        case EHSHitDirection::Front:    return FrontHitMontage;
        case EHSHitDirection::Back:     return BackHitMontage;
        case EHSHitDirection::Left:     return LeftHitMontage;
        case EHSHitDirection::Right:    return RightHitMontage;
        default:                        return FrontHitMontage;
    }
}

// 강도별 카메라 쉐이크 클래스 가져오기
TSubclassOf<UCameraShakeBase> UHSHitReactionComponent::GetCameraShakeForIntensity(float Intensity) const
{
    if (Intensity <= 0.3f)
    {
        return LightCameraShake;
    }
    else if (Intensity <= 0.7f)
    {
        return MediumCameraShake;
    }
    else
    {
        return HeavyCameraShake;
    }
}

// 넉백 방향 계산 함수
FVector UHSHitReactionComponent::CalculateKnockbackDirection(AActor* DamageInstigator) const
{
    if (!OwnerCharacter || !DamageInstigator)
    {
        return FVector::ForwardVector;
    }

    // 공격자로부터 피격자로의 방향 벡터
    FVector Direction = (OwnerCharacter->GetActorLocation() - DamageInstigator->GetActorLocation()).GetSafeNormal();
    
    // Z축 제거 (수평 넉백만)
    Direction.Z = 0.0f;
    Direction.Normalize();

    return Direction;
}

// 액터 간 각도 계산 함수
float UHSHitReactionComponent::CalculateAngleBetweenActors(AActor* DamageInstigator) const
{
    if (!OwnerCharacter || !DamageInstigator)
    {
        return 0.0f;
    }

    // 캐릭터의 전방 방향
    FVector ForwardVector = OwnerCharacter->GetActorForwardVector();
    
    // 공격자로의 방향
    FVector ToInstigator = (DamageInstigator->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
    
    // Z축 제거
    ForwardVector.Z = 0.0f;
    ToInstigator.Z = 0.0f;
    
    ForwardVector.Normalize();
    ToInstigator.Normalize();

    // 내적을 이용한 각도 계산
    float DotProduct = FVector::DotProduct(ForwardVector, ToInstigator);
    float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

    // 외적을 이용한 방향 결정 (좌우 구분)
    FVector CrossProduct = FVector::CrossProduct(ForwardVector, ToInstigator);
    if (CrossProduct.Z < 0.0f)
    {
        Angle = -Angle;
    }

    return Angle;
}
