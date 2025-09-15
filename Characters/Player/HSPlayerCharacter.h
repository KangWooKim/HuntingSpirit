/**
 * @file HSPlayerCharacter.h
 * @brief 플레이어 캐릭터의 기본 클래스 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HSPlayerTypes.h"
#include "HSPlayerCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class UAnimMontage;

/**
 * @struct FHSCommonAnimationSet
 * @brief 플레이어 캐릭터 공통 애니메이션 데이터 구조체
 */
USTRUCT(BlueprintType)
struct FHSCommonAnimationSet
{
    GENERATED_BODY()

    /** @brief 대기 애니메이션 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* IdleMontage = nullptr;

    /** @brief 걷기 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* WalkMontage = nullptr;

    /** @brief 달리기 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* RunMontage = nullptr;

    /** @brief 기본 공격 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* BasicAttackMontage = nullptr;

    /** @brief 점프 애니메이션 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* JumpMontage = nullptr;

    /** @brief 착지 애니메이션 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* LandMontage = nullptr;

    /** @brief 피격 애니메이션 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* HitReactionMontage = nullptr;

    /** @brief 사망 애니메이션 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* DeathMontage = nullptr;
};

/**
 * @class AHSPlayerCharacter
 * @brief 플레이어 캐릭터의 기본 클래스
 * @details 모든 플레이어 캐릭터(전사, 시프, 마법사)의 공통 기능을 제공하는 기본 클래스
 */
UCLASS()
class HUNTINGSPIRIT_API AHSPlayerCharacter : public AHSCharacterBase
{
    GENERATED_BODY()

public:
    /** @brief 플레이어 캐릭터 생성자 */
    AHSPlayerCharacter();

    /** 
     * @brief 입력 컴포넌트 설정
     * @param PlayerInputComponent 설정할 입력 컴포넌트
     */
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /** 
     * @brief 매 프레임 호출되는 업데이트 함수
     * @param DeltaTime 이전 프레임과의 시간 차이
     */
    virtual void Tick(float DeltaTime) override;

    /** @brief 기본 공격 수행 */
    virtual void PerformBasicAttack() override;
    
    /** @brief 달리기 상태 토글 */
    virtual void ToggleSprint() override;
    
    /** 
     * @brief 대기 애니메이션 몽타주 반환
     * @return UAnimMontage* 대기 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetIdleAnimMontage() const;
    
    /** 
     * @brief 걷기 애니메이션 몽타주 반환
     * @return UAnimMontage* 걷기 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetWalkAnimMontage() const;
    
    /** 
     * @brief 달리기 애니메이션 몽타주 반환
     * @return UAnimMontage* 달리기 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetRunAnimMontage() const;
    
    /** 
     * @brief 공격 애니메이션 몽타주 반환
     * @return UAnimMontage* 공격 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetAttackAnimMontage() const;
    
    /** 
     * @brief 점프 애니메이션 몽타주 반환
     * @return UAnimMontage* 점프 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetJumpAnimMontage() const;
    
    /** 
     * @brief 피격 반응 애니메이션 몽타주 반환
     * @return UAnimMontage* 피격 반응 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetHitReactionAnimMontage() const;
    
    /** 
     * @brief 사망 애니메이션 몽타주 반환
     * @return UAnimMontage* 사망 애니메이션 몽타주
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetDeathAnimMontage() const;
    
    /** 
     * @brief 플레이어 클래스 타입 설정
     * @param NewPlayerClass 설정할 플레이어 클래스 타입
     */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void SetPlayerClass(EHSPlayerClass NewPlayerClass);
    
    /** 
     * @brief 플레이어 클래스 타입 반환
     * @return EHSPlayerClass 현재 플레이어 클래스 타입
     */
    UFUNCTION(BlueprintPure, Category = "Character")
    EHSPlayerClass GetPlayerClass() const { return PlayerClass; }
    
    /** 
     * @brief 스태미너 UI 표시 여부 확인
     * @return bool 스태미너 UI를 표시해야 하면 true
     */
    UFUNCTION(BlueprintCallable, Category = "Character|UI")
    bool ShouldShowStaminaBar() const;

protected:
    /** @brief 게임 시작 또는 스폰 시 호출되는 초기화 함수 */
    virtual void BeginPlay() override;

    /** @brief 카메라 설정을 위한 Spring Arm 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
    USpringArmComponent* CameraBoom;

    /** @brief 카메라 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
    UCameraComponent* FollowCamera;
    
    /** @brief 플레이어 캐릭터 클래스 타입 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    EHSPlayerClass PlayerClass;
    
    /** @brief 공통 애니메이션 세트 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    FHSCommonAnimationSet CommonAnimations;
    
    /** @brief 달리기 토글 입력 처리 함수 */
    void OnToggleSprintPressed();
    
    /** @brief 스태미너 UI 표시용 타이머 핸들 */
    FTimerHandle StaminaUITimerHandle;
    
    /** @brief 스태미너 UI 표시 기간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|UI")
    float StaminaUIDisplayTime;
    
    /** @brief 스태미너 UI 표시 여부 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|UI")
    bool bShowStaminaBar;
    
    /** 
     * @brief 공통 애니메이션 세트 반환
     * @return FHSCommonAnimationSet 공통 애니메이션 세트
     */
    FHSCommonAnimationSet GetCommonAnimationSet() const { return CommonAnimations; }

private:
    /** @brief 애니메이션 참조 설정 */
    void SetupAnimationReferences();
    
    /** @brief 스태미너 UI 표시 종료 */
    void HideStaminaBar();
};
