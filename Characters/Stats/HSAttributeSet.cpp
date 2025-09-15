// Fill out your copyright notice in the Description page of Project Settings.

#include "HSAttributeSet.h"

UHSAttributeSet::UHSAttributeSet()
{
	// 기본 속성 값 초기화
	InitializeAttributes();
}

void UHSAttributeSet::InitializeAttributes()
{
	// 기본 체력
	MaxHealth = 100.0f;
	Health = MaxHealth;
	
	// 기본 마나
	MaxMana = 100.0f;
	Mana = MaxMana;
	
	// 기본 스태미너
	MaxStamina = 100.0f;
	Stamina = MaxStamina;
	
	// 기본 공격력 및 방어력
	AttackPower = 10.0f;
	DefensePower = 5.0f;
	
	// 기본 치명타 설정
	CriticalChance = 0.1f; // 10% 확률
	CriticalDamage = 2.0f; // 2배 데미지
	
	// 기본 속도 설정
	MovementSpeed = 600.0f;
	AttackSpeed = 1.0f;
}

void UHSAttributeSet::SetHealth(float NewHealth)
{
	float OldHealth = Health;
	Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	
	if (OldHealth != Health)
	{
		OnHealthChanged.Broadcast(OldHealth, Health);
	}
}

void UHSAttributeSet::SetMana(float NewMana)
{
	float OldMana = Mana;
	Mana = FMath::Clamp(NewMana, 0.0f, MaxMana);
	
	if (OldMana != Mana)
	{
		OnManaChanged.Broadcast(OldMana, Mana);
	}
}

void UHSAttributeSet::SetStamina(float NewStamina)
{
	float OldStamina = Stamina;
	Stamina = FMath::Clamp(NewStamina, 0.0f, MaxStamina);
	
	if (OldStamina != Stamina)
	{
		OnStaminaChanged.Broadcast(OldStamina, Stamina);
	}
}

void UHSAttributeSet::SetMaxHealth(float NewMaxHealth)
{
	MaxHealth = FMath::Max(1.0f, NewMaxHealth);
	// 현재 체력이 최대 체력을 초과하는 경우 조정
	if (Health > MaxHealth)
	{
		SetHealth(MaxHealth);
	}
}

void UHSAttributeSet::SetMaxMana(float NewMaxMana)
{
	MaxMana = FMath::Max(0.0f, NewMaxMana);
	// 현재 마나가 최대 마나를 초과하는 경우 조정
	if (Mana > MaxMana)
	{
		SetMana(MaxMana);
	}
}

void UHSAttributeSet::SetMaxStamina(float NewMaxStamina)
{
	MaxStamina = FMath::Max(0.0f, NewMaxStamina);
	// 현재 스태미너가 최대 스태미너를 초과하는 경우 조정
	if (Stamina > MaxStamina)
	{
		SetStamina(MaxStamina);
	}
}

void UHSAttributeSet::SetAttackPower(float NewAttackPower)
{
	AttackPower = FMath::Max(0.0f, NewAttackPower);
}

void UHSAttributeSet::SetDefensePower(float NewDefensePower)
{
	DefensePower = FMath::Max(0.0f, NewDefensePower);
}

void UHSAttributeSet::SetCriticalChance(float NewCriticalChance)
{
	CriticalChance = FMath::Clamp(NewCriticalChance, 0.0f, 1.0f);
}

void UHSAttributeSet::SetCriticalDamage(float NewCriticalDamage)
{
	CriticalDamage = FMath::Max(1.0f, NewCriticalDamage);
}

void UHSAttributeSet::SetMovementSpeed(float NewMovementSpeed)
{
	MovementSpeed = FMath::Max(0.0f, NewMovementSpeed);
}

void UHSAttributeSet::SetAttackSpeed(float NewAttackSpeed)
{
	AttackSpeed = FMath::Max(0.1f, NewAttackSpeed);
}

void UHSAttributeSet::ClampAttributeValues()
{
	// 모든 속성 값을 유효한 범위로 제한
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	Mana = FMath::Clamp(Mana, 0.0f, MaxMana);
	Stamina = FMath::Clamp(Stamina, 0.0f, MaxStamina);
	
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	MaxMana = FMath::Max(0.0f, MaxMana);
	MaxStamina = FMath::Max(0.0f, MaxStamina);
	
	AttackPower = FMath::Max(0.0f, AttackPower);
	DefensePower = FMath::Max(0.0f, DefensePower);
	
	CriticalChance = FMath::Clamp(CriticalChance, 0.0f, 1.0f);
	CriticalDamage = FMath::Max(1.0f, CriticalDamage);
	
	MovementSpeed = FMath::Max(0.0f, MovementSpeed);
	AttackSpeed = FMath::Max(0.1f, AttackSpeed);
}
