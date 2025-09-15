// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HSAttributeSet.generated.h"

/**
 * 캐릭터의 기본 속성들을 관리하는 클래스
 * 체력, 마나, 스태미너, 공격력, 방어력 등의 속성을 정의하고 관리
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSAttributeSet : public UObject
{
	GENERATED_BODY()

public:
	UHSAttributeSet();

	/** 속성 초기화 */
	void InitializeAttributes();

	/** 속성 값 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetHealth() const { return Health; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetMaxHealth() const { return MaxHealth; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetMana() const { return Mana; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetMaxMana() const { return MaxMana; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetStamina() const { return Stamina; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetMaxStamina() const { return MaxStamina; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetAttackPower() const { return AttackPower; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetDefensePower() const { return DefensePower; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetCriticalChance() const { return CriticalChance; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetCriticalDamage() const { return CriticalDamage; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetMovementSpeed() const { return MovementSpeed; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetAttackSpeed() const { return AttackSpeed; }

	/** 속성 값 설정하기 */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetHealth(float NewHealth);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetMana(float NewMana);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetStamina(float NewStamina);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetMaxHealth(float NewMaxHealth);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetMaxMana(float NewMaxMana);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetMaxStamina(float NewMaxStamina);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetAttackPower(float NewAttackPower);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetDefensePower(float NewDefensePower);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetCriticalChance(float NewCriticalChance);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetCriticalDamage(float NewCriticalDamage);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetMovementSpeed(float NewMovementSpeed);
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetAttackSpeed(float NewAttackSpeed);

	/** 속성 델리게이트 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeChanged, float /*OldValue*/, float /*NewValue*/);
	
	FOnAttributeChanged OnHealthChanged;
	FOnAttributeChanged OnManaChanged;
	FOnAttributeChanged OnStaminaChanged;

protected:
	/** 체력 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float Health;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float MaxHealth;

	/** 마나 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float Mana;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float MaxMana;

	/** 스태미너 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float Stamina;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float MaxStamina;

	/** 공격력 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float AttackPower;

	/** 방어력 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float DefensePower;

	/** 치명타 확률 (0.0 ~ 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float CriticalChance;

	/** 치명타 데미지 배율 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float CriticalDamage;

	/** 이동 속도 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float MovementSpeed;

	/** 공격 속도 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float AttackSpeed;

	/** 속성 값 범위 제한 */
	void ClampAttributeValues();
};
