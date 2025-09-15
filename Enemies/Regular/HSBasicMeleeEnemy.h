// HuntingSpirit Game - Basic Melee Enemy Header
// 기본 근접 공격 적 클래스

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HSBasicMeleeEnemy.generated.h"

// 근접 공격 패턴 열거형
UENUM(BlueprintType)
enum class EHSMeleeAttackPattern : uint8
{
    SingleStrike    UMETA(DisplayName = "Single Strike"),      // 단일 타격
    DoubleStrike    UMETA(DisplayName = "Double Strike"),      // 이중 타격
    SpinAttack      UMETA(DisplayName = "Spin Attack"),        // 회전 공격
    ChargeAttack    UMETA(DisplayName = "Charge Attack"),      // 돌진 공격
    GroundSlam      UMETA(DisplayName = "Ground Slam")         // 지면 강타
};

// 근접 공격 정보 구조체
USTRUCT(BlueprintType)
struct FHSMeleeAttackInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHSMeleeAttackPattern AttackPattern;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackDamage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackCooldown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UAnimMontage* AttackAnimation;

    FHSMeleeAttackInfo()
    {
        AttackPattern = EHSMeleeAttackPattern::SingleStrike;
        AttackDamage = 10.0f;
        AttackRange = 150.0f;
        AttackDuration = 1.0f;
        AttackCooldown = 2.0f;
        AttackAnimation = nullptr;
    }
};

UCLASS()
class HUNTINGSPIRIT_API AHSBasicMeleeEnemy : public AHSEnemyBase
{
    GENERATED_BODY()

public:
    // 생성자
    AHSBasicMeleeEnemy();

    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;

    // 공격 수행 오버라이드
    virtual void PerformAttack() override;

    // 근접 공격 패턴 함수들
    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void PerformMeleeAttack(EHSMeleeAttackPattern Pattern);

    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void ExecuteSingleStrike();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void ExecuteDoubleStrike();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void ExecuteSpinAttack();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void ExecuteChargeAttack();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void ExecuteGroundSlam();

    // 공격 검증 함수들
    UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
    bool CanPerformMeleeAttack() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
    bool IsTargetInMeleeRange() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
    float GetDistanceToCurrentTarget() const;

    // 공격 패턴 선택
    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    EHSMeleeAttackPattern SelectAttackPattern();

    // 차징 관련 함수들 (돌진 공격용)
    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void StartCharging(const FVector& ChargeDirection);

    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void StopCharging();

    UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
    bool IsCharging() const { return bIsCharging; }

    // 콤보 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
    void ResetCombo();

    UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
    int32 GetCurrentComboCount() const { return CurrentComboCount; }

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 근접 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    TArray<FHSMeleeAttackInfo> MeleeAttackPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float MeleeAttackAngle = 60.0f; // 공격 각도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float MeleeKnockbackForce = 500.0f; // 넉백 힘

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    bool bCanCombo = true; // 콤보 가능 여부

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    int32 MaxComboCount = 3; // 최대 콤보 수

    // 돌진 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float ChargeSpeed = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float ChargeDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float ChargeDamageMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float ChargePreparationTime = 0.5f;

    // 회전 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float SpinAttackRadius = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float SpinAttackDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    int32 SpinAttackHitCount = 3;

    // 지면 강타 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float GroundSlamRadius = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    float GroundSlamStunDuration = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
    bool bGroundSlamCreatesShockwave = true;

    // 현재 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Melee")
    bool bIsAttacking = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Melee")
    bool bIsCharging = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Melee")
    int32 CurrentComboCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Melee")
    FTimerHandle ComboResetTimer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Melee")
    FTimerHandle ChargeTimer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Melee")
    FVector ChargeTargetLocation;

    // 공격 효과 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Effects")
    UParticleSystem* MeleeAttackEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Effects")
    USoundBase* MeleeAttackSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Effects")
    TSubclassOf<UCameraShakeBase> MeleeAttackCameraShake;

private:
    // 내부 함수들
    void InitializeMeleeAttackPatterns();
    void ApplyMeleeDamage(const FHSMeleeAttackInfo& AttackInfo);
    TArray<AActor*> GetHitActorsInMeleeRange(float Range, float Angle);
    void ApplyKnockback(AActor* Target, float Force);
    void PlayMeleeAttackEffects(const FVector& Location);

    // 타이머 콜백 함수들
    UFUNCTION()
    void OnAttackAnimationFinished();

    UFUNCTION()
    void OnComboWindowExpired();

    UFUNCTION()
    void OnChargePreparationComplete();

    UFUNCTION()
    void OnChargeComplete();

    // 회전 공격 틱 함수
    UFUNCTION()
    void PerformSpinAttackTick();

    // 충돌 처리
    UFUNCTION()
    void OnChargeHit(AActor* HitActor);

    // 디버그
    void DrawDebugMeleeAttack(float Range, float Angle);

    // 스핀 공격 타이머
    FTimerHandle SpinTickTimer;
};
