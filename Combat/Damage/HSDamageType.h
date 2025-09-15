// HuntingSpirit Game - Damage Type Header
// 데미지 타입 및 데미지 관련 구조체 정의

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSDamageType.generated.h"

// 데미지 타입 열거형
UENUM(BlueprintType)
enum class EHSDamageType : uint8
{
    None            UMETA(DisplayName = "None"),
    Physical        UMETA(DisplayName = "Physical"),        // 물리 데미지
    Magical         UMETA(DisplayName = "Magical"),         // 마법 데미지
    Fire            UMETA(DisplayName = "Fire"),            // 화염 데미지
    Ice             UMETA(DisplayName = "Ice"),             // 빙결 데미지
    Lightning       UMETA(DisplayName = "Lightning"),       // 번개 데미지
    Poison          UMETA(DisplayName = "Poison"),          // 독 데미지
    Dark            UMETA(DisplayName = "Dark"),            // 암흑 데미지
    Holy            UMETA(DisplayName = "Holy")             // 신성 데미지
};

// 데미지 계산 모드
UENUM(BlueprintType)
enum class EHSDamageCalculationMode : uint8
{
    Fixed           UMETA(DisplayName = "Fixed"),           // 고정 데미지
    RandomRange     UMETA(DisplayName = "Random Range"),   // 범위 내 랜덤
    Percentage      UMETA(DisplayName = "Percentage"),     // 비율 데미지
    StatBased       UMETA(DisplayName = "Stat Based")      // 스탯 기반 데미지
};

// 상태 효과 타입
UENUM(BlueprintType)
enum class EHSStatusEffectType : uint8
{
    None            UMETA(DisplayName = "None"),
    Stun            UMETA(DisplayName = "Stun"),            // 기절
    Slow            UMETA(DisplayName = "Slow"),            // 둔화
    Burn            UMETA(DisplayName = "Burn"),            // 화상
    Freeze          UMETA(DisplayName = "Freeze"),          // 빙결
    Shock           UMETA(DisplayName = "Shock"),           // 감전
    PoisonDot       UMETA(DisplayName = "Poison DoT"),      // 독 지속 데미지
    Blind           UMETA(DisplayName = "Blind"),           // 실명
    Weakness        UMETA(DisplayName = "Weakness")         // 약화
};

// 상태 효과 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSStatusEffect
{
    GENERATED_BODY()

    // 상태 효과 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effect")
    EHSStatusEffectType EffectType = EHSStatusEffectType::None;

    // 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effect", meta = (ClampMin = "0.0"))
    float Duration = 0.0f;

    // 효과 강도 (둔화의 경우 이동속도 감소율, DoT의 경우 초당 데미지 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effect")
    float Intensity = 0.0f;

    // 적용 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ApplyChance = 1.0f;

    // 스택 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effect")
    bool bStackable = false;

    // 최대 스택 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effect", meta = (EditCondition = "bStackable", ClampMin = "1"))
    int32 MaxStacks = 1;

    // 생성자
    FHSStatusEffect()
    {
        EffectType = EHSStatusEffectType::None;
        Duration = 0.0f;
        Intensity = 0.0f;
        ApplyChance = 1.0f;
        bStackable = false;
        MaxStacks = 1;
    }
};

// 데미지 정보 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSDamageInfo
{
    GENERATED_BODY()

    // 기본 데미지 양
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0"))
    float BaseDamage = 0.0f;

    // 데미지 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    EHSDamageType DamageType = EHSDamageType::Physical;

    // 데미지 계산 모드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    EHSDamageCalculationMode CalculationMode = EHSDamageCalculationMode::Fixed;

    // 최소 데미지 (RandomRange 모드에서 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (EditCondition = "CalculationMode == EHSDamageCalculationMode::RandomRange", ClampMin = "0.0"))
    float MinDamage = 0.0f;

    // 최대 데미지 (RandomRange 모드에서 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (EditCondition = "CalculationMode == EHSDamageCalculationMode::RandomRange", ClampMin = "0.0"))
    float MaxDamage = 0.0f;

    // 크리티컬 히트 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Critical", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CriticalChance = 0.0f;

    // 크리티컬 히트 배수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Critical", meta = (ClampMin = "1.0"))
    float CriticalMultiplier = 2.0f;

    // 방어력 관통력 (0.0 ~ 1.0, 0.5 = 50% 관통)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmorPenetration = 0.0f;

    // 적용할 상태 효과들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status Effects")
    TArray<FHSStatusEffect> StatusEffects;

    // 넉백 강도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
    float KnockbackForce = 0.0f;

    // 넉백 지속시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
    float KnockbackDuration = 0.0f;

    // 히트스톱 지속시간 (타격감을 위한 프레임 정지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Effects", meta = (ClampMin = "0.0"))
    float HitStopDuration = 0.0f;

    // 카메라 쉐이크 강도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Effects", meta = (ClampMin = "0.0"))
    float CameraShakeIntensity = 0.0f;

    // 방어력 무시 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    bool bIgnoreArmor = false;

    // 생성자
    FHSDamageInfo()
    {
        BaseDamage = 0.0f;
        DamageType = EHSDamageType::Physical;
        CalculationMode = EHSDamageCalculationMode::Fixed;
        MinDamage = 0.0f;
        MaxDamage = 0.0f;
        CriticalChance = 0.0f;
        CriticalMultiplier = 2.0f;
        ArmorPenetration = 0.0f;
        KnockbackForce = 0.0f;
        KnockbackDuration = 0.0f;
        HitStopDuration = 0.0f;
        CameraShakeIntensity = 0.0f;
        bIgnoreArmor = false;
    }

    // 실제 데미지 계산 함수
    FORCEINLINE float CalculateFinalDamage() const
    {
        float FinalDamage = BaseDamage;

        switch (CalculationMode)
        {
            case EHSDamageCalculationMode::Fixed:
                FinalDamage = BaseDamage;
                break;
                
            case EHSDamageCalculationMode::RandomRange:
                FinalDamage = FMath::RandRange(MinDamage, MaxDamage);
                break;
                
            case EHSDamageCalculationMode::Percentage:
                // 비율 데미지는 외부에서 계산 (타겟의 최대 체력 필요)
                FinalDamage = BaseDamage;
                break;
                
            case EHSDamageCalculationMode::StatBased:
                // 스탯 기반 데미지는 외부에서 계산 (공격자의 스탯 필요)
                FinalDamage = BaseDamage;
                break;
                
            default:
                FinalDamage = BaseDamage;
                break;
        }

        return FinalDamage;
    }

    // 크리티컬 히트 여부 판정
    FORCEINLINE bool IsCriticalHit() const
    {
        return FMath::RandRange(0.0f, 1.0f) <= CriticalChance;
    }

    // 상태 효과 적용 여부 판정
    FORCEINLINE bool ShouldApplyStatusEffect(const FHSStatusEffect& Effect) const
    {
        return FMath::RandRange(0.0f, 1.0f) <= Effect.ApplyChance;
    }
};

// 데미지 적용 결과 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSDamageResult
{
    GENERATED_BODY()

    // 적용된 최종 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Damage Result")
    float FinalDamage = 0.0f;

    // 크리티컬 히트 여부
    UPROPERTY(BlueprintReadOnly, Category = "Damage Result")
    bool bCriticalHit = false;

    // 적용된 상태 효과들
    UPROPERTY(BlueprintReadOnly, Category = "Damage Result")
    TArray<FHSStatusEffect> AppliedStatusEffects;

    // 방어력에 의해 감소된 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Damage Result")
    float MitigatedDamage = 0.0f;

    // 대상이 사망했는지 여부
    UPROPERTY(BlueprintReadOnly, Category = "Damage Result")
    bool bTargetKilled = false;

    // 생성자
    FHSDamageResult()
    {
        FinalDamage = 0.0f;
        bCriticalHit = false;
        MitigatedDamage = 0.0f;
        bTargetKilled = false;
    }
};

// 데미지 타입별 저항력 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSDamageResistance
{
    GENERATED_BODY()

    // 물리 데미지 저항 (0.0 ~ 1.0, 0.5 = 50% 감소)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PhysicalResistance = 0.0f;

    // 마법 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MagicalResistance = 0.0f;

    // 화염 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FireResistance = 0.0f;

    // 빙결 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float IceResistance = 0.0f;

    // 번개 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LightningResistance = 0.0f;

    // 독 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PoisonResistance = 0.0f;

    // 암흑 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DarkResistance = 0.0f;

    // 신성 데미지 저항
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HolyResistance = 0.0f;

    // 생성자
    FHSDamageResistance()
    {
        PhysicalResistance = 0.0f;
        MagicalResistance = 0.0f;
        FireResistance = 0.0f;
        IceResistance = 0.0f;
        LightningResistance = 0.0f;
        PoisonResistance = 0.0f;
        DarkResistance = 0.0f;
        HolyResistance = 0.0f;
    }

    // 특정 데미지 타입에 대한 저항력 반환
    FORCEINLINE float GetResistanceForDamageType(EHSDamageType DamageType) const
    {
        switch (DamageType)
        {
            case EHSDamageType::Physical:   return PhysicalResistance;
            case EHSDamageType::Magical:    return MagicalResistance;
            case EHSDamageType::Fire:       return FireResistance;
            case EHSDamageType::Ice:        return IceResistance;
            case EHSDamageType::Lightning:  return LightningResistance;
            case EHSDamageType::Poison:     return PoisonResistance;
            case EHSDamageType::Dark:       return DarkResistance;
            case EHSDamageType::Holy:       return HolyResistance;
            default:                        return 0.0f;
        }
    }
};
