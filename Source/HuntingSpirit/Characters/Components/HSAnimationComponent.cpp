// HuntingSpirit Game - Animation Component Implementation

#include "HSAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

// 생성자
UHSAnimationComponent::UHSAnimationComponent()
{
    // 컴포넌트 설정
    PrimaryComponentTick.bCanEverTick = true;
    
    // 모션 매칭 기본값 설정 (언리얼 엔진 5의 새로운 기능)
    bMotionMatchingEnabled = true;
}

// 컴포넌트 초기화
void UHSAnimationComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 소유자 캐릭터 및 애니메이션 인스턴스 초기화
    InitializeReferences();
}

// 매 프레임 호출
void UHSAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 애니메이션 몽타주 재생
bool UHSAnimationComponent::PlayAnimMontage(UAnimMontage* Montage, float PlayRate, FName StartSectionName)
{
    if (!Montage || !OwnerCharacter || !AnimInstance)
    {
        // 필요한 참조가 없으면 실패
        return false;
    }
    
    // AnimInstance를 통해 몽타주 재생
    float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
    
    // 시작 섹션이 지정된 경우 해당 섹션으로 이동
    if (Duration > 0.0f && StartSectionName != NAME_None)
    {
        AnimInstance->Montage_JumpToSection(StartSectionName, Montage);
    }
    
    return Duration > 0.0f;
}

// 애니메이션 몽타주 정지
void UHSAnimationComponent::StopAnimMontage(UAnimMontage* Montage)
{
    if (!OwnerCharacter || !AnimInstance)
    {
        return;
    }
    
    // 몽타주가 지정되지 않은 경우 현재 재생 중인 몽타주 사용
    if (!Montage)
    {
        Montage = AnimInstance->GetCurrentActiveMontage();
    }
    
    // 몽타주 정지
    if (Montage)
    {
        AnimInstance->Montage_Stop(0.25f, Montage);
    }
}

// 걷기 애니메이션 재생
bool UHSAnimationComponent::PlayWalkAnimation(float PlayRate)
{
    return PlayAnimMontage(AnimationData.WalkMontage, PlayRate);
}

// 달리기 애니메이션 재생
bool UHSAnimationComponent::PlayRunAnimation(float PlayRate)
{
    return PlayAnimMontage(AnimationData.RunMontage, PlayRate);
}

// 기본 공격 애니메이션 재생
bool UHSAnimationComponent::PlayBasicAttackAnimation(float PlayRate)
{
    return PlayAnimMontage(AnimationData.BasicAttackMontage, PlayRate);
}

// 애니메이션 데이터 설정
void UHSAnimationComponent::SetAnimationData(const FHSCharacterAnimationData& InAnimationData)
{
    AnimationData = InAnimationData;
}

// 모션 매칭 켜기/끄기
void UHSAnimationComponent::SetMotionMatchingEnabled(bool bEnabled)
{
    bMotionMatchingEnabled = bEnabled;
    
    // 애니메이션 블루프린트에서 모션 매칭 설정을 적용할 수 있도록 구현 필요
    // 언리얼 엔진 5의 모션 매칭 기능 활용
}

// 애니메이션 인스턴스 및 소유자 캐릭터 참조 초기화
void UHSAnimationComponent::InitializeReferences()
{
    // 소유자 캐릭터 얻기
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    
    // 애니메이션 인스턴스 얻기
    if (OwnerCharacter && OwnerCharacter->GetMesh())
    {
        AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
    }
}
