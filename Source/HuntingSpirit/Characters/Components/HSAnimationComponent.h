// HuntingSpirit Game - Animation Component Header
// 캐릭터 애니메이션 관리를 위한 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSAnimationComponent.generated.h"

// 캐릭터 애니메이션 상태 데이터 구조체
USTRUCT(BlueprintType)
struct FHSCharacterAnimationData
{
    GENERATED_BODY()

    // 걷기 애니메이션 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    class UAnimMontage* WalkMontage = nullptr;

    // 달리기 애니메이션 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    class UAnimMontage* RunMontage = nullptr;

    // 기본 공격 애니메이션 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    class UAnimMontage* BasicAttackMontage = nullptr;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    // 생성자
    UHSAnimationComponent();
    
    // 컴포넌트 초기화
    virtual void BeginPlay() override;
    
    // 매 프레임 호출
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 애니메이션 몽타주 재생
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayAnimMontage(class UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSectionName = NAME_None);

    // 애니메이션 몽타주 정지
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void StopAnimMontage(class UAnimMontage* Montage = nullptr);

    // 걷기 애니메이션 재생
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayWalkAnimation(float PlayRate = 1.0f);

    // 달리기 애니메이션 재생
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayRunAnimation(float PlayRate = 1.0f);

    // 기본 공격 애니메이션 재생
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayBasicAttackAnimation(float PlayRate = 1.0f);

    // 애니메이션 데이터 설정
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void SetAnimationData(const FHSCharacterAnimationData& InAnimationData);

    // 모션 매칭 켜기/끄기
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void SetMotionMatchingEnabled(bool bEnabled);

protected:
    // 캐릭터 소유자
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    // 애니메이션 인스턴스
    UPROPERTY()
    class UAnimInstance* AnimInstance;

    // 캐릭터 애니메이션 데이터
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    FHSCharacterAnimationData AnimationData;

    // 모션 매칭 활성화 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    bool bMotionMatchingEnabled;

private:
    // 애니메이션 인스턴스 및 소유자 캐릭터 참조 초기화
    void InitializeReferences();
};
