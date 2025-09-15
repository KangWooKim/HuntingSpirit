/**
 * @file HSAnimationComponent.cpp
 * @brief HuntingSpirit 게임의 캐릭터 애니메이션 관리 컴포넌트 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#include "HSAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UHSAnimationComponent::UHSAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    bMotionMatchingEnabled = true;
}

void UHSAnimationComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeReferences();
}

void UHSAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UHSAnimationComponent::PlayAnimMontage(UAnimMontage* Montage, float PlayRate, FName StartSectionName)
{
    if (!Montage || !OwnerCharacter || !AnimInstance)
    {
        return false;
    }
    
    float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
    
    if (Duration > 0.0f && StartSectionName != NAME_None)
    {
        AnimInstance->Montage_JumpToSection(StartSectionName, Montage);
    }
    
    return Duration > 0.0f;
}

void UHSAnimationComponent::StopAnimMontage(UAnimMontage* Montage)
{
    if (!OwnerCharacter || !AnimInstance)
    {
        return;
    }
    
    if (!Montage)
    {
        Montage = AnimInstance->GetCurrentActiveMontage();
    }
    
    if (Montage)
    {
        AnimInstance->Montage_Stop(0.25f, Montage);
    }
}

bool UHSAnimationComponent::PlayWalkAnimation(float PlayRate)
{
    return PlayAnimMontage(AnimationData.WalkMontage, PlayRate);
}

bool UHSAnimationComponent::PlayRunAnimation(float PlayRate)
{
    return PlayAnimMontage(AnimationData.RunMontage, PlayRate);
}

bool UHSAnimationComponent::PlayBasicAttackAnimation(float PlayRate)
{
    return PlayAnimMontage(AnimationData.BasicAttackMontage, PlayRate);
}

void UHSAnimationComponent::SetAnimationData(const FHSCharacterAnimationData& InAnimationData)
{
    AnimationData = InAnimationData;
}

void UHSAnimationComponent::SetMotionMatchingEnabled(bool bEnabled)
{
    bMotionMatchingEnabled = bEnabled;
}

void UHSAnimationComponent::InitializeReferences()
{
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    
    if (OwnerCharacter && OwnerCharacter->GetMesh())
    {
        AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
    }
}
