// HuntingSpirit Game - Player Character Header
// 플레이어 캐릭터의 기본 클래스

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

// 공통 애니메이션 데이터 구조체
USTRUCT(BlueprintType)
struct FHSCommonAnimationSet
{
    GENERATED_BODY()

    // 대기 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* IdleMontage = nullptr;

    // 걷기 애니메이션 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* WalkMontage = nullptr;

    // 달리기 애니메이션 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* RunMontage = nullptr;

    // 기본 공격 애니메이션 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* BasicAttackMontage = nullptr;

    // 점프 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* JumpMontage = nullptr;

    // 착지 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* LandMontage = nullptr;

    // 피격 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* HitReactionMontage = nullptr;

    // 사망 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common Animation")
    UAnimMontage* DeathMontage = nullptr;
};

UCLASS()
class HUNTINGSPIRIT_API AHSPlayerCharacter : public AHSCharacterBase
{
    GENERATED_BODY()

public:
    // 플레이어 캐릭터 생성자
    AHSPlayerCharacter();

    // 입력 컴포넌트 설정
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;

    // 공격 수행 오버라이드
    virtual void PerformBasicAttack() override;
    
    // 달리기 토글 오버라이드
    virtual void ToggleSprint() override;
    
    // 공통 애니메이션 가져오기
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetIdleAnimMontage() const;
    
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetWalkAnimMontage() const;
    
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetRunAnimMontage() const;
    
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetAttackAnimMontage() const;
    
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetJumpAnimMontage() const;
    
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetHitReactionAnimMontage() const;
    
    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual UAnimMontage* GetDeathAnimMontage() const;
    
    // 플레이어 클래스 타입 설정/가져오기
    UFUNCTION(BlueprintCallable, Category = "Character")
    void SetPlayerClass(EHSPlayerClass NewPlayerClass);
    
    UFUNCTION(BlueprintPure, Category = "Character")
    EHSPlayerClass GetPlayerClass() const { return PlayerClass; }
    
    // 스태미너 UI 표시 여부
    UFUNCTION(BlueprintCallable, Category = "Character|UI")
    bool ShouldShowStaminaBar() const;

protected:
    // 게임 시작 또는 스폰 시 호출
    virtual void BeginPlay() override;

    // 카메라 설정을 위한 Spring Arm 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
    USpringArmComponent* CameraBoom;

    // 카메라 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
    UCameraComponent* FollowCamera;
    
    // 플레이어 캐릭터 클래스 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    EHSPlayerClass PlayerClass;
    
    // 공통 애니메이션 세트
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    FHSCommonAnimationSet CommonAnimations;
    
    // 달리기 토글 입력 처리 함수
    void OnToggleSprintPressed();
    
    // 스태미너 UI 표시용 타이머 핸들
    FTimerHandle StaminaUITimerHandle;
    
    // 스태미너 UI 표시 기간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|UI")
    float StaminaUIDisplayTime;
    
    // 스태미너 UI 표시 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|UI")
    bool bShowStaminaBar;
    
    // 공통 애니메이션 세트 가져오기
    FHSCommonAnimationSet GetCommonAnimationSet() const { return CommonAnimations; }

private:
    // 애니메이션 참조 설정
    void SetupAnimationReferences();
    
    // 스태미너 UI 표시 종료
    void HideStaminaBar();
};
