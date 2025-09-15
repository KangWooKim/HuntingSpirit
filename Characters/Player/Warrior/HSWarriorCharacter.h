/**
 * @file HSWarriorCharacter.h
 * @brief 전사 캐릭터 클래스 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HSWarriorCharacter.generated.h"

/**
 * @enum EWarriorSkillType
 * @brief 전사 스킬 타입 열거형
 * @details 전사 캐릭터가 사용할 수 있는 스킬들을 정의
 */
UENUM(BlueprintType)
enum class EWarriorSkillType : uint8
{
    None            UMETA(DisplayName = "None"),               ///< 스킬 없음
    ShieldBlock     UMETA(DisplayName = "Shield Block"),      ///< Q 스킬 - 방어
    Charge          UMETA(DisplayName = "Charge"),            ///< W 스킬 - 돌진
    Whirlwind       UMETA(DisplayName = "Whirlwind"),         ///< E 스킬 - 회전베기
    BerserkerRage   UMETA(DisplayName = "Berserker Rage")     ///< R 스킬 - 광폭화 (궁극기)
};

/**
 * @struct FWarriorSkillData
 * @brief 전사 스킬 데이터 구조체
 * @details 전사 스킬의 애니메이션, 쿨다운, 데미지 등 정보를 포함
 */
USTRUCT(BlueprintType)
struct FWarriorSkillData
{
    GENERATED_BODY()

    /** @brief 스킬 애니메이션 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    UAnimMontage* SkillMontage = nullptr;

    /** @brief 스킬 쿨다운 시간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Cooldown = 5.0f;

    /** @brief 스킬 지속 시간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Duration = 1.0f;

    /** @brief 스킬 스태미너 소모량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float StaminaCost = 20.0f;

    /** @brief 스킬 데미지 (해당되는 경우) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Damage = 0.0f;

    /** @brief 스킬 범위 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Range = 200.0f;

    FWarriorSkillData()
    {
        SkillMontage = nullptr;
        Cooldown = 5.0f;
        Duration = 1.0f;
        StaminaCost = 20.0f;
        Damage = 0.0f;
        Range = 200.0f;
    }
};

/**
 * @class AHSWarriorCharacter
 * @brief 전사 캐릭터 클래스
 * @details 높은 체력과 방어력, 강력한 근접 공격을 가진 전투 전문가
 *          Q: 방어 - W: 돌진 - E: 회전베기 - R: 광폭화
 */
UCLASS()
class HUNTINGSPIRIT_API AHSWarriorCharacter : public AHSPlayerCharacter
{
    GENERATED_BODY()

public:
    /** @brief 전사 캐릭터 생성자 */
    AHSWarriorCharacter();
    
    /** @brief 게임 시작 또는 스폰 시 호출되는 초기화 함수 */
    virtual void BeginPlay() override;
    
    /** @brief 기본 공격 수행 (오버라이드) */
    virtual void PerformBasicAttack() override;
    
    /** @brief Q 스킬 - 방어 사용 */
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillQ();
    
    /** @brief W 스킬 - 돌진 사용 */
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillW();
    
    /** @brief E 스킬 - 회전베기 사용 */
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillE();
    
    /** @brief R 스킬 - 광폭화 사용 (궁극기) */
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillR();
    
    /** 
     * @brief 스킬 사용 가능 여부 확인
     * @param SkillType 확인할 스킬 타입
     * @return bool 스킬 사용 가능하면 true
     */
    UFUNCTION(BlueprintPure, Category = "Warrior|Skills")
    bool CanUseSkill(EWarriorSkillType SkillType) const;
    
    /** 
     * @brief 스킬 쿨다운 남은 시간 반환
     * @param SkillType 확인할 스킬 타입
     * @return float 쿨다운 남은 시간 (초)
     */
    UFUNCTION(BlueprintPure, Category = "Warrior|Skills")
    float GetSkillCooldownRemaining(EWarriorSkillType SkillType) const;
    
    /** 
     * @brief 스킬 데이터 반환
     * @param SkillType 가져올 스킬 타입
     * @return FWarriorSkillData 스킬 데이터
     */
    UFUNCTION(BlueprintPure, Category = "Warrior|Skills")
    FWarriorSkillData GetSkillData(EWarriorSkillType SkillType) const;

protected:
    /** @brief Q 스킬 - 방어 데이터 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData ShieldBlockData;
    
    /** @brief W 스킬 - 돌진 데이터 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData ChargeData;
    
    /** @brief E 스킬 - 회전베기 데이터 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData WhirlwindData;
    
    /** @brief R 스킬 - 광폭화 데이터 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData BerserkerRageData;
    
    /** @brief 스킬 쿨다운 타이머 핸들 맵 */
    UPROPERTY()
    TMap<EWarriorSkillType, FTimerHandle> SkillCooldownTimers;
    
    /** @brief 방어 상태 플래그 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Warrior|State")
    bool bIsBlocking = false;
    
    /** @brief 돌진 상태 플래그 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Warrior|State")
    bool bIsCharging = false;
    
    /** @brief 광폭화 상태 플래그 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Warrior|State")
    bool bIsBerserkerMode = false;
    
    /** @brief Q 스킬 실행 */
    void ExecuteShieldBlock();
    
    /** @brief W 스킬 실행 */
    void ExecuteCharge();
    
    /** @brief E 스킬 실행 */
    void ExecuteWhirlwind();
    
    /** @brief R 스킬 실행 */
    void ExecuteBerserkerRage();
    
    /** @brief 방어 상태 종료 */
    UFUNCTION()
    void EndBlocking();
    
    /** @brief 돌진 상태 종료 */
    UFUNCTION()
    void EndCharging();
    
    /** @brief 광폭화 상태 종료 */
    UFUNCTION()
    void EndBerserkerMode();
    
    /** @brief 전사 특화 스탯 설정 */
    void SetupWarriorStats();
    
    /** @brief 스킬 시스템 초기화 */
    void InitializeWarriorSkills();
    
    /** 
     * @brief 스킬 쿨다운 시작
     * @param SkillType 쿨다운을 시작할 스킬 타입
     * @param CooldownTime 쿨다운 시간 (초)
     */
    void StartSkillCooldown(EWarriorSkillType SkillType, float CooldownTime);
    
    /** @brief 방어 상태 타이머 핸들 */
    FTimerHandle BlockingTimerHandle;
    
    /** @brief 돌진 상태 타이머 핸들 */
    FTimerHandle ChargingTimerHandle;
    
    /** @brief 광폭화 상태 타이머 핸들 */
    FTimerHandle BerserkerTimerHandle;
};
