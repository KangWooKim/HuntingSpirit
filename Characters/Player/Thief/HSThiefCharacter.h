/**
 * @file HSThiefCharacter.h
 * @brief 시프(도적) 캐릭터 클래스 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HSThiefCharacter.generated.h"

/**
 * @enum EThiefSkillType
 * @brief 시프 스킬 타입 열거형
 * @details 시프 캐릭터가 사용할 수 있는 스킬들을 정의
 */
UENUM(BlueprintType)
enum class EThiefSkillType : uint8
{
    None            UMETA(DisplayName = "None"),               ///< 스킬 없음
    Stealth         UMETA(DisplayName = "Stealth"),           ///< Q 스킬 - 은신
    QuickDash       UMETA(DisplayName = "Quick Dash"),        ///< W 스킬 - 질주
    DodgeRoll       UMETA(DisplayName = "Dodge Roll"),        ///< E 스킬 - 회피
    MultiStrike     UMETA(DisplayName = "Multi Strike")       ///< R 스킬 - 연속공격 (궁극기)
};

// 시프 스킬 데이터 구조체
USTRUCT(BlueprintType)
struct FThiefSkillData
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

    // 스킬 범위/거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Range = 200.0f;

    FThiefSkillData()
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
 * 시프(도적) 캐릭터 클래스
 * 빠른 이동 속도와 고속 공격, 회피 능력을 가진 민첩한 전문가
 * Q: 은신 - W: 질주 - E: 회피 - R: 연속공격
 */
UCLASS()
class HUNTINGSPIRIT_API AHSThiefCharacter : public AHSPlayerCharacter
{
    GENERATED_BODY()

public:
    // 생성자
    AHSThiefCharacter();
    
    // 게임 시작 또는 스폰 시 호출
    virtual void BeginPlay() override;
    
    // 특수 공격 (오버라이드)
    virtual void PerformBasicAttack() override;
    
    // 달리기 시작 함수 (더 빠른 이동)
    virtual void StartSprinting() override;
    
    // 달리기 종료 함수
    virtual void StopSprinting() override;
    
    // 시프 스킬 시스템 (QWER)
    UFUNCTION(BlueprintCallable, Category = "Thief|Skills")
    void UseSkillQ(); // 은신
    
    UFUNCTION(BlueprintCallable, Category = "Thief|Skills")
    void UseSkillW(); // 질주
    
    UFUNCTION(BlueprintCallable, Category = "Thief|Skills")
    void UseSkillE(); // 회피
    
    UFUNCTION(BlueprintCallable, Category = "Thief|Skills")
    void UseSkillR(); // 연속공격 (궁극기)
    
    // 스킬 사용 가능 여부 확인
    UFUNCTION(BlueprintPure, Category = "Thief|Skills")
    bool CanUseSkill(EThiefSkillType SkillType) const;
    
    // 스킬 쿨다운 남은 시간 반환
    UFUNCTION(BlueprintPure, Category = "Thief|Skills")
    float GetSkillCooldownRemaining(EThiefSkillType SkillType) const;
    
    // 스킬 데이터 가져오기
    UFUNCTION(BlueprintPure, Category = "Thief|Skills")
    FThiefSkillData GetSkillData(EThiefSkillType SkillType) const;
    
    // 은신 상태 확인
    UFUNCTION(BlueprintPure, Category = "Thief|State")
    bool IsStealthed() const { return bIsStealthed; }

protected:
    // 시프 스킬 데이터
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Thief")
    FThiefSkillData StealthData;        // Q 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Thief")
    FThiefSkillData QuickDashData;      // W 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Thief")
    FThiefSkillData DodgeRollData;      // E 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Thief")
    FThiefSkillData MultiStrikeData;    // R 스킬
    
    // 스킬 쿨다운 타이머 핸들
    UPROPERTY()
    TMap<EThiefSkillType, FTimerHandle> SkillCooldownTimers;
    
    // 시프 상태 플래그
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thief|State")
    bool bIsStealthed = false;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thief|State")
    bool bIsQuickDashing = false;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thief|State")
    bool bIsMultiStriking = false;
    
    // 시프 특화 능력 - 질주 속도 배율
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thief|Abilities")
    float SprintSpeedMultiplier = 2.0f;
    
    // 은신 시 투명도
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thief|Abilities")
    float StealthOpacity = 0.3f;
    
    // 시프 스킬 구현 내부 함수
    void ExecuteStealth();          // Q 스킬 실행
    void ExecuteQuickDash();        // W 스킬 실행
    void ExecuteDodgeRoll();        // E 스킬 실행
    void ExecuteMultiStrike();      // R 스킬 실행
    
    // 상태 및 효과 관리
    UFUNCTION()
    void EndStealth();
    
    UFUNCTION()
    void EndQuickDash();
    
    UFUNCTION()
    void EndMultiStrike();
    
    // 시프 특화 스탯 설정
    void SetupThiefStats();
    
    // 스킬 초기화
    void InitializeThiefSkills();
    
    // 스킬 쿨다운 시작
    void StartSkillCooldown(EThiefSkillType SkillType, float CooldownTime);
    
    // 시프 상태 효과 타이머
    FTimerHandle StealthTimerHandle;
    FTimerHandle QuickDashTimerHandle;
    FTimerHandle MultiStrikeTimerHandle;
    
    // 연속공격 관련
    int32 MultiStrikeCombo = 0;
    FTimerHandle MultiStrikeComboTimerHandle;
    
    // 연속공격 다음 타격 실행
    UFUNCTION()
    void ExecuteNextMultiStrike();
};
