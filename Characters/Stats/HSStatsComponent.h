// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSStatsComponent.generated.h"

// 버프 타입 정의
UENUM(BlueprintType)
enum class EBuffType : uint8
{
    None            UMETA(DisplayName = "None"),
    Health          UMETA(DisplayName = "Health"),
    Mana            UMETA(DisplayName = "Mana"),
    Stamina         UMETA(DisplayName = "Stamina"),
    Attack          UMETA(DisplayName = "Attack"),
    Defense         UMETA(DisplayName = "Defense"),
    AttackSpeed     UMETA(DisplayName = "Attack Speed"),
    MovementSpeed   UMETA(DisplayName = "Movement Speed"),
    CriticalChance  UMETA(DisplayName = "Critical Chance"),
    AllStats        UMETA(DisplayName = "All Stats"),
    DamageReduction UMETA(DisplayName = "Damage Reduction"),
    ReviveSpeed     UMETA(DisplayName = "Revive Speed")
};

// 버프 데이터 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FBuffData
{
    GENERATED_BODY()
    
    // 버프 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    FString BuffID;
    
    // 버프 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    EBuffType BuffType = EBuffType::None;
    
    // 버프 값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    float Value = 0.0f;
    
    // 지속 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    float Duration = 0.0f;
    
    // 비율인지 여부 (true: 배율, false: 고정값)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    bool bIsPercentage = false;
    
    // 스택 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    bool bStackable = false;
    
    // 현재 스택 수
    UPROPERTY(BlueprintReadOnly, Category = "Buff")
    int32 CurrentStacks = 1;
    
    // 남은 시간
    UPROPERTY(BlueprintReadOnly, Category = "Buff")
    float RemainingTime = 0.0f;
    
    FBuffData()
    {
        BuffID = TEXT("");
        BuffType = EBuffType::None;
        Value = 0.0f;
        Duration = 0.0f;
        bIsPercentage = false;
        bStackable = false;
        CurrentStacks = 1;
        RemainingTime = 0.0f;
    }
};

class UHSAttributeSet;
class UHSLevelSystem;

/**
 * 캐릭터의 스탯을 관리하는 컴포넌트
 * AttributeSet과 LevelSystem을 통합 관리하여 캐릭터의 능력치와 성장을 처리
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSStatsComponent();

protected:
	virtual void BeginPlay() override;

public:
	/** 속성 세트 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	UHSAttributeSet* GetAttributeSet() const { return AttributeSet; }
	
	/** 레벨 시스템 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	UHSLevelSystem* GetLevelSystem() const { return LevelSystem; }

	/** 데미지 적용 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	float ApplyDamage(float DamageAmount, bool bIgnoreDefense = false);
	
	/** 체력 회복 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	void Heal(float HealAmount);
	
	/** 마나 소비 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Resource")
	bool ConsumeMana(float ManaAmount);
	
	/** 마나 회복 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Resource")
	void RestoreMana(float ManaAmount);
	
	/** 스태미너 소비 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Resource")
	bool ConsumeStamina(float StaminaAmount);
	
	/** 스태미너 회복 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Resource")
	void RestoreStamina(float StaminaAmount);

	/** 경험치 획득 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Level")
	void GainExperience(int32 ExpAmount);
	
	/** 치명타 여부 확인 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	bool IsCriticalHit() const;
	
	/** 치명타를 포함한 최종 데미지 계산 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	float CalculateFinalDamage(float BaseDamage) const;

	/** 죽음 체크 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	bool IsDead() const;

	/** 공격력 반환 */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetAttackPower() const;

	/** 체력 비율 반환 (0.0 ~ 1.0) */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetHealthPercent() const;

	/** 현재 체력 반환 */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetCurrentHealth() const;

	// 공유 능력 시스템을 위한 추가 메서드들
	
	/** 현재 마나 반환 */
	UFUNCTION(BlueprintPure, Category = "Stats|Resource")
	float GetCurrentMana() const;
	
	/** 현재 마나 설정 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Resource")
	void SetCurrentMana(float NewMana);
	
	/** 현재 스태미너 반환 */
	UFUNCTION(BlueprintPure, Category = "Stats|Resource")
	float GetCurrentStamina() const;
	
	/** 현재 스태미너 설정 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Resource")
	void SetCurrentStamina(float NewStamina);
	
	/** 버프 적용 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Buff")
	void ApplyBuff(const FBuffData& BuffData);
	
	/** 버프 제거 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Buff")
	void RemoveBuff(const FString& BuffID);
	
	/** 모든 버프 제거 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Buff")
	void ClearAllBuffs();
	
	/** 버프 존재 여부 확인 */
	UFUNCTION(BlueprintPure, Category = "Stats|Buff")
	bool HasBuff(const FString& BuffID) const;
	
	/** 활성 버프 목록 반환 */
	UFUNCTION(BlueprintPure, Category = "Stats|Buff")
	TArray<FBuffData> GetActiveBuffs() const;

	/** 스탯 초기화 (직업별 기본 스탯 설정) */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void InitializeStatsForClass(const FName& ClassName);

	/** 자동 회복 기능 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Regeneration")
	void EnableAutoRegeneration(bool bEnableHealth, bool bEnableMana, bool bEnableStamina);

	/** 델리게이트 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor* /*DeadActor*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDamageReceived, float /*Damage*/, float /*CurrentHealth*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatsLevelUp, int32 /*NewLevel*/, int32 /*StatPoints*/);
	
	FOnDeath OnDeath;
	FOnDamageReceived OnDamageReceived;
	FOnStatsLevelUp OnStatsLevelUp;

protected:
	/** 속성 세트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	UHSAttributeSet* AttributeSet;
	
	/** 레벨 시스템 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	UHSLevelSystem* LevelSystem;

	/** 자동 회복 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Regeneration")
	bool bAutoRegenerateHealth;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Regeneration")
	bool bAutoRegenerateMana;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Regeneration")
	bool bAutoRegenerateStamina;
	
	/** 회복 속도 (초당 회복량) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Regeneration")
	float HealthRegenRate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Regeneration")
	float ManaRegenRate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Regeneration")
	float StaminaRegenRate;

	/** 죽음 상태 */
	UPROPERTY(BlueprintReadOnly, Category = "Stats|Combat")
	bool bIsDead;
	
	/** 활성 버프 목록 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Buff")
	TArray<FBuffData> ActiveBuffs;
	
	/** 버프 타이머 핸들 */
	TMap<FString, FTimerHandle> BuffTimerHandles;

private:
	/** 자동 회복 타이머 핸들 */
	FTimerHandle RegenerationTimerHandle;
	
	/** 자동 회복 처리 */
	void HandleRegeneration();
	
	/** 레벨업 처리 */
	void HandleLevelUp(int32 NewLevel, int32 StatPoints);
	
	/** 전사 클래스 초기 스탯 */
	void InitializeWarriorStats();
	
	/** 시프 클래스 초기 스탯 */
	void InitializeThiefStats();
	
	/** 마법사 클래스 초기 스탯 */
	void InitializeMageStats();
	
	/** 버프 효과 적용/제거 */
	void ApplyBuffEffect(const FBuffData& BuffData, bool bApply);
};
