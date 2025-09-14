// HuntingSpirit Game - Hit Reaction Component Header
// 캐릭터의 피격 반응 처리를 담당하는 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HuntingSpirit/Combat/Damage/HSDamageType.h"
#include "HSHitReactionComponent.generated.h"

class AHSCharacterBase;
class UCameraShakeBase;
class UParticleSystem;
class USoundBase;
class UAnimMontage;

// 피격 방향 열거형
UENUM(BlueprintType)
enum class EHSHitDirection : uint8
{
    Front       UMETA(DisplayName = "Front"),
    Back        UMETA(DisplayName = "Back"),
    Left        UMETA(DisplayName = "Left"),
    Right       UMETA(DisplayName = "Right")
};

// 피격 반응 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHitReactionTriggered, const FHSDamageInfo&, DamageInfo, AActor*, DamageInstigator, EHSHitDirection, HitDirection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnKnockbackApplied, FVector, KnockbackDirection, float, KnockbackForce);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSHitReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 생성자
    UHSHitReactionComponent();

    // 피격 반응 처리 메인 함수
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void ProcessHitReaction(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    // 넉백 적용 함수
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction|Knockback")
    void ApplyKnockback(FVector KnockbackDirection, float Force, float Duration);

    // 히트스톱 적용 함수
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction|Hit Stop")
    void ApplyHitStop(float Duration);

    // 카메라 쉐이크 적용 함수
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction|Camera Shake")
    void ApplyCameraShake(float Intensity);

    // 피격 방향 계산 함수
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    EHSHitDirection CalculateHitDirection(AActor* DamageInstigator) const;

    // 피격 효과 재생 함수들
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction|Effects")
    void PlayHitEffect(const FHSDamageInfo& DamageInfo, const FVector& HitLocation);

    UFUNCTION(BlueprintCallable, Category = "Hit Reaction|Effects")
    void PlayHitSound(EHSDamageType DamageType);

    UFUNCTION(BlueprintCallable, Category = "Hit Reaction|Animation")
    void PlayHitAnimation(EHSHitDirection HitDirection, EHSDamageType DamageType);

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction Events")
    FOnHitReactionTriggered OnHitReactionTriggered;

    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction Events")
    FOnKnockbackApplied OnKnockbackApplied;

protected:
    // 컴포넌트 초기화
    virtual void BeginPlay() override;

    // 피격 효과 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Effects|Physical")
    UParticleSystem* PhysicalHitEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Effects|Magical")
    UParticleSystem* MagicalHitEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Effects|Fire")
    UParticleSystem* FireHitEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Effects|Ice")
    UParticleSystem* IceHitEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Effects|Lightning")
    UParticleSystem* LightningHitEffect;

    // 피격 사운드 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Sounds|Physical")
    USoundBase* PhysicalHitSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Sounds|Magical")
    USoundBase* MagicalHitSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Sounds|Fire")
    USoundBase* FireHitSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Sounds|Ice")
    USoundBase* IceHitSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Sounds|Lightning")
    USoundBase* LightningHitSound;

    // 크리티컬 히트 전용 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Critical")
    UParticleSystem* CriticalHitEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Critical")
    USoundBase* CriticalHitSound;

    // 피격 애니메이션 몽타주들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Animation|Front")
    UAnimMontage* FrontHitMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Animation|Back")
    UAnimMontage* BackHitMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Animation|Left")
    UAnimMontage* LeftHitMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Animation|Right")
    UAnimMontage* RightHitMontage;

    // 카메라 쉐이크 클래스들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Camera Shake|Light")
    TSubclassOf<UCameraShakeBase> LightCameraShake;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Camera Shake|Medium")
    TSubclassOf<UCameraShakeBase> MediumCameraShake;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Camera Shake|Heavy")
    TSubclassOf<UCameraShakeBase> HeavyCameraShake;

    // 피격 반응 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Settings")
    bool bEnableHitReactions = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Settings")
    bool bEnableKnockback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Settings")
    bool bEnableHitStop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Settings")
    bool bEnableCameraShake = true;

    // 넉백 저항력 (0.0 = 넉백 없음, 1.0 = 완전 넉백)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float KnockbackResistance = 0.0f;

    // 히트스톱 저항력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reaction|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HitStopResistance = 0.0f;

private:
    // 오너 캐릭터 캐시
    UPROPERTY()
    AHSCharacterBase* OwnerCharacter;

    // 타이머 핸들들
    FTimerHandle KnockbackTimerHandle;
    FTimerHandle HitStopTimerHandle;

    // 넉백 상태 변수들
    bool bIsKnockedBack = false;
    FVector OriginalVelocity;
    
    // 히트스톱 상태 변수들
    bool bIsHitStopped = false;
    float OriginalTimeDilation;

    // 내부 함수들
    void EndKnockback();
    void EndHitStop();
    
    // 효과 및 사운드 가져오기 함수들
    UParticleSystem* GetHitEffectForDamageType(EHSDamageType DamageType) const;
    USoundBase* GetHitSoundForDamageType(EHSDamageType DamageType) const;
    UAnimMontage* GetHitAnimationForDirection(EHSHitDirection Direction) const;
    TSubclassOf<UCameraShakeBase> GetCameraShakeForIntensity(float Intensity) const;

    // 유틸리티 함수들
    FVector CalculateKnockbackDirection(AActor* DamageInstigator) const;
    float CalculateAngleBetweenActors(AActor* DamageInstigator) const;
};
