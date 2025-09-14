// HuntingSpirit Game - Warrior Character Header
// 전사 캐릭터 클래스

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HSWarriorCharacter.generated.h"

// 전사 스킬 타입 열거형
UENUM(BlueprintType)
enum class EWarriorSkillType : uint8
{
    None            UMETA(DisplayName = "None"),
    ShieldBlock     UMETA(DisplayName = "Shield Block"),     // Q 스킬 - 방어
    Charge          UMETA(DisplayName = "Charge"),          // W 스킬 - 돌진
    Whirlwind       UMETA(DisplayName = "Whirlwind"),       // E 스킬 - 회전베기
    BerserkerRage   UMETA(DisplayName = "Berserker Rage")   // R 스킬 - 광폭화 (궁극기)
};

// 전사 스킬 데이터 구조체
USTRUCT(BlueprintType)
struct FWarriorSkillData
{
    GENERATED_BODY()

    // 스킬 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    UAnimMontage* SkillMontage = nullptr;

    // 스킬 쿨다운 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Cooldown = 5.0f;

    // 스킬 지속 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Duration = 1.0f;

    // 스킬 스태미너 소모량
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float StaminaCost = 20.0f;

    // 스킬 데미지 (해당되는 경우)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Damage = 0.0f;

    // 스킬 범위
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
 * 전사 캐릭터 클래스
 * 높은 체력과 방어력, 강력한 근접 공격을 가진 전투 전문가
 * Q: 방어 - W: 돌진 - E: 회전베기 - R: 광폭화
 */
UCLASS()
class HUNTINGSPIRIT_API AHSWarriorCharacter : public AHSPlayerCharacter
{
    GENERATED_BODY()

public:
    // 생성자
    AHSWarriorCharacter();
    
    // 게임 시작 또는 스폰 시 호출
    virtual void BeginPlay() override;
    
    // 특수 공격 (오버라이드)
    virtual void PerformBasicAttack() override;
    
    // 전사 스킬 시스템 (QWER)
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillQ(); // 방어
    
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillW(); // 돌진
    
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillE(); // 회전베기
    
    UFUNCTION(BlueprintCallable, Category = "Warrior|Skills")
    void UseSkillR(); // 광폭화 (궁극기)
    
    // 스킬 사용 가능 여부 확인
    UFUNCTION(BlueprintPure, Category = "Warrior|Skills")
    bool CanUseSkill(EWarriorSkillType SkillType) const;
    
    // 스킬 쿨다운 남은 시간 반환
    UFUNCTION(BlueprintPure, Category = "Warrior|Skills")
    float GetSkillCooldownRemaining(EWarriorSkillType SkillType) const;
    
    // 스킬 데이터 가져오기
    UFUNCTION(BlueprintPure, Category = "Warrior|Skills")
    FWarriorSkillData GetSkillData(EWarriorSkillType SkillType) const;

protected:
    // 전사 스킬 데이터
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData ShieldBlockData;      // Q 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData ChargeData;           // W 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData WhirlwindData;        // E 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Warrior")
    FWarriorSkillData BerserkerRageData;    // R 스킬
    
    // 스킬 쿨다운 타이머 핸들
    UPROPERTY()
    TMap<EWarriorSkillType, FTimerHandle> SkillCooldownTimers;
    
    // 전사 상태 플래그
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Warrior|State")
    bool bIsBlocking = false;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Warrior|State")
    bool bIsCharging = false;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Warrior|State")
    bool bIsBerserkerMode = false;
    
    // 전사 스킬 구현 내부 함수
    void ExecuteShieldBlock();      // Q 스킬 실행
    void ExecuteCharge();           // W 스킬 실행
    void ExecuteWhirlwind();        // E 스킬 실행
    void ExecuteBerserkerRage();    // R 스킬 실행
    
    // 상태 및 효과 관리
    UFUNCTION()
    void EndBlocking();
    
    UFUNCTION()
    void EndCharging();
    
    UFUNCTION()
    void EndBerserkerMode();
    
    // 전사 특화 스탯 설정
    void SetupWarriorStats();
    
    // 스킬 초기화
    void InitializeWarriorSkills();
    
    // 스킬 쿨다운 시작
    void StartSkillCooldown(EWarriorSkillType SkillType, float CooldownTime);
    
    // 전사 상태 효과 타이머
    FTimerHandle BlockingTimerHandle;
    FTimerHandle ChargingTimerHandle;
    FTimerHandle BerserkerTimerHandle;
};
