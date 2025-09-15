/**
 * @file HSAnimationComponent.h
 * @brief HuntingSpirit 게임의 캐릭터 애니메이션 관리 컴포넌트
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSAnimationComponent.generated.h"

/**
 * @struct FHSCharacterAnimationData
 * @brief 캐릭터 애니메이션 상태 데이터를 저장하는 구조체
 */
USTRUCT(BlueprintType)
struct FHSCharacterAnimationData
{
    GENERATED_BODY()

    /** 걷기 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    class UAnimMontage* WalkMontage = nullptr;

    /** 달리기 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    class UAnimMontage* RunMontage = nullptr;

    /** 기본 공격 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    class UAnimMontage* BasicAttackMontage = nullptr;
};

/**
 * @class UHSAnimationComponent
 * @brief 캐릭터 애니메이션 관리를 담당하는 컴포넌트
 * @details 캐릭터의 애니메이션 몽타주 재생, 모션 매칭 등을 관리합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    /**
     * @brief 생성자
     */
    UHSAnimationComponent();
    
    /**
     * @brief 컴포넌트 초기화
     * @details 소유자 캐릭터와 애니메이션 인스턴스 참조를 설정합니다.
     */
    virtual void BeginPlay() override;
    
    /**
     * @brief 매 프레임 호출되는 틱 함수
     * @param DeltaTime 이전 프레임으로부터의 시간 간격
     * @param TickType 틱 타입
     * @param ThisTickFunction 이 컴포넌트의 틱 함수
     */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * @brief 애니메이션 몽타주를 재생합니다.
     * @param Montage 재생할 애니메이션 몽타주
     * @param PlayRate 재생 속도 (기본값: 1.0f)
     * @param StartSectionName 시작할 섹션 이름 (기본값: NAME_None)
     * @return 재생 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayAnimMontage(class UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSectionName = NAME_None);

    /**
     * @brief 애니메이션 몽타주를 정지합니다.
     * @param Montage 정지할 애니메이션 몽타주 (nullptr인 경우 현재 재생 중인 몽타주 정지)
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void StopAnimMontage(class UAnimMontage* Montage = nullptr);

    /**
     * @brief 걷기 애니메이션을 재생합니다.
     * @param PlayRate 재생 속도 (기본값: 1.0f)
     * @return 재생 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayWalkAnimation(float PlayRate = 1.0f);

    /**
     * @brief 달리기 애니메이션을 재생합니다.
     * @param PlayRate 재생 속도 (기본값: 1.0f)
     * @return 재생 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayRunAnimation(float PlayRate = 1.0f);

    /**
     * @brief 기본 공격 애니메이션을 재생합니다.
     * @param PlayRate 재생 속도 (기본값: 1.0f)
     * @return 재생 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool PlayBasicAttackAnimation(float PlayRate = 1.0f);

    /**
     * @brief 애니메이션 데이터를 설정합니다.
     * @param InAnimationData 설정할 애니메이션 데이터
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void SetAnimationData(const FHSCharacterAnimationData& InAnimationData);

    /**
     * @brief 모션 매칭 기능을 활성화/비활성화합니다.
     * @param bEnabled 활성화 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void SetMotionMatchingEnabled(bool bEnabled);

protected:
    /** 캐릭터 소유자 */
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    /** 애니메이션 인스턴스 */
    UPROPERTY()
    class UAnimInstance* AnimInstance;

    /** 캐릭터 애니메이션 데이터 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    FHSCharacterAnimationData AnimationData;

    /** 모션 매칭 활성화 여부 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    bool bMotionMatchingEnabled;

private:
    /**
     * @brief 애니메이션 인스턴스 및 소유자 캐릭터 참조를 초기화합니다.
     */
    void InitializeReferences();
};
