// Fill out your copyright notice in the Description page of Project Settings.

#include "HSStatsComponent.h"
#include "HSAttributeSet.h"
#include "HSLevelSystem.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Engine/World.h"

UHSStatsComponent::UHSStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 속성 세트와 레벨 시스템 생성
	AttributeSet = CreateDefaultSubobject<UHSAttributeSet>(TEXT("AttributeSet"));
	LevelSystem = CreateDefaultSubobject<UHSLevelSystem>(TEXT("LevelSystem"));

	// 기본 회복 설정
	bAutoRegenerateHealth = false;
	bAutoRegenerateMana = true;
	bAutoRegenerateStamina = true;
	
	// 기본 회복 속도 설정
	HealthRegenRate = 5.0f;
	ManaRegenRate = 10.0f;
	StaminaRegenRate = 20.0f;
	
	bIsDead = false;
}

void UHSStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// 레벨 시스템 이벤트 바인딩
	if (LevelSystem)
	{
		LevelSystem->OnLevelChanged.AddUObject(this, &UHSStatsComponent::HandleLevelUp);
	}
	
	// 자동 회복 타이머 시작
	if (bAutoRegenerateHealth || bAutoRegenerateMana || bAutoRegenerateStamina)
	{
		GetWorld()->GetTimerManager().SetTimer(RegenerationTimerHandle, this, 
			&UHSStatsComponent::HandleRegeneration, 1.0f, true);
	}
}

float UHSStatsComponent::ApplyDamage(float DamageAmount, bool bIgnoreDefense)
{
	if (bIsDead || !AttributeSet)
	{
		return 0.0f;
	}
	
	float FinalDamage = DamageAmount;
	
	// 방어력 적용
	if (!bIgnoreDefense)
	{
		float DefensePower = AttributeSet->GetDefensePower();
		// 방어력 공식: 데미지 감소율 = 방어력 / (방어력 + 100)
		float DamageReduction = DefensePower / (DefensePower + 100.0f);
		FinalDamage = DamageAmount * (1.0f - DamageReduction);
	}
	
	// 최소 데미지는 1
	FinalDamage = FMath::Max(1.0f, FinalDamage);
	
	// 체력 감소
	float CurrentHealth = AttributeSet->GetHealth();
	float NewHealth = CurrentHealth - FinalDamage;
	AttributeSet->SetHealth(NewHealth);
	
	// 데미지 받음 이벤트 발생
	OnDamageReceived.Broadcast(FinalDamage, AttributeSet->GetHealth());
	
	// 죽음 체크
	if (AttributeSet->GetHealth() <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast(GetOwner());
	}
	
	return FinalDamage;
}

void UHSStatsComponent::Heal(float HealAmount)
{
	if (bIsDead || !AttributeSet)
	{
		return;
	}
	
	float CurrentHealth = AttributeSet->GetHealth();
	float NewHealth = CurrentHealth + HealAmount;
	AttributeSet->SetHealth(NewHealth);
}

bool UHSStatsComponent::ConsumeMana(float ManaAmount)
{
	if (!AttributeSet)
	{
		return false;
	}
	
	float CurrentMana = AttributeSet->GetMana();
	if (CurrentMana >= ManaAmount)
	{
		AttributeSet->SetMana(CurrentMana - ManaAmount);
		return true;
	}
	
	return false;
}

void UHSStatsComponent::RestoreMana(float ManaAmount)
{
	if (!AttributeSet)
	{
		return;
	}
	
	float CurrentMana = AttributeSet->GetMana();
	AttributeSet->SetMana(CurrentMana + ManaAmount);
}

bool UHSStatsComponent::ConsumeStamina(float StaminaAmount)
{
	if (!AttributeSet)
	{
		return false;
	}
	
	float CurrentStamina = AttributeSet->GetStamina();
	if (CurrentStamina >= StaminaAmount)
	{
		AttributeSet->SetStamina(CurrentStamina - StaminaAmount);
		return true;
	}
	
	return false;
}

void UHSStatsComponent::RestoreStamina(float StaminaAmount)
{
	if (!AttributeSet)
	{
		return;
	}
	
	float CurrentStamina = AttributeSet->GetStamina();
	AttributeSet->SetStamina(CurrentStamina + StaminaAmount);
}

void UHSStatsComponent::GainExperience(int32 ExpAmount)
{
	if (LevelSystem)
	{
		LevelSystem->AddExperience(ExpAmount);
	}
}

bool UHSStatsComponent::IsCriticalHit() const
{
	if (!AttributeSet)
	{
		return false;
	}
	
	float CritChance = AttributeSet->GetCriticalChance();
	return FMath::FRand() <= CritChance;
}

float UHSStatsComponent::CalculateFinalDamage(float BaseDamage) const
{
	if (!AttributeSet)
	{
		return BaseDamage;
	}
	
	float AttackPower = AttributeSet->GetAttackPower();
	float FinalDamage = BaseDamage + AttackPower;
	
	// 치명타 확인
	if (IsCriticalHit())
	{
		float CritDamage = AttributeSet->GetCriticalDamage();
		FinalDamage *= CritDamage;
	}
	
	return FinalDamage;
}

bool UHSStatsComponent::IsDead() const
{
	return bIsDead;
}

float UHSStatsComponent::GetAttackPower() const
{
	if (!AttributeSet)
	{
		return 0.0f;
	}
	
	return AttributeSet->GetAttackPower();
}

float UHSStatsComponent::GetHealthPercent() const
{
	if (!AttributeSet)
	{
		return 0.0f;
	}
	
	float CurrentHealth = AttributeSet->GetHealth();
	float MaxHealth = AttributeSet->GetMaxHealth();
	
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}
	
	return CurrentHealth / MaxHealth;
}

float UHSStatsComponent::GetCurrentHealth() const
{
	if (!AttributeSet)
	{
		return 0.0f;
	}
	
	return AttributeSet->GetHealth();
}

void UHSStatsComponent::InitializeStatsForClass(const FName& ClassName)
{
	if (ClassName == "Warrior")
	{
		InitializeWarriorStats();
	}
	else if (ClassName == "Thief")
	{
		InitializeThiefStats();
	}
	else if (ClassName == "Mage")
	{
		InitializeMageStats();
	}
}

void UHSStatsComponent::EnableAutoRegeneration(bool bEnableHealth, bool bEnableMana, bool bEnableStamina)
{
	bAutoRegenerateHealth = bEnableHealth;
	bAutoRegenerateMana = bEnableMana;
	bAutoRegenerateStamina = bEnableStamina;
	
	// 타이머 재설정
	if (bAutoRegenerateHealth || bAutoRegenerateMana || bAutoRegenerateStamina)
	{
		if (!RegenerationTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().SetTimer(RegenerationTimerHandle, this, 
				&UHSStatsComponent::HandleRegeneration, 1.0f, true);
		}
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(RegenerationTimerHandle);
	}
}

void UHSStatsComponent::HandleRegeneration()
{
	if (bIsDead || !AttributeSet)
	{
		return;
	}
	
	// 체력 회복
	if (bAutoRegenerateHealth)
	{
		float CurrentHealth = AttributeSet->GetHealth();
		float MaxHealth = AttributeSet->GetMaxHealth();
		if (CurrentHealth < MaxHealth)
		{
			AttributeSet->SetHealth(CurrentHealth + HealthRegenRate);
		}
	}
	
	// 마나 회복
	if (bAutoRegenerateMana)
	{
		float CurrentMana = AttributeSet->GetMana();
		float MaxMana = AttributeSet->GetMaxMana();
		if (CurrentMana < MaxMana)
		{
			AttributeSet->SetMana(CurrentMana + ManaRegenRate);
		}
	}
	
	// 스태미너 회복
	if (bAutoRegenerateStamina)
	{
		float CurrentStamina = AttributeSet->GetStamina();
		float MaxStamina = AttributeSet->GetMaxStamina();
		if (CurrentStamina < MaxStamina)
		{
			AttributeSet->SetStamina(CurrentStamina + StaminaRegenRate);
		}
	}
}

void UHSStatsComponent::HandleLevelUp(int32 NewLevel, int32 StatPoints)
{
	if (!AttributeSet)
	{
		return;
	}
	
	// 레벨업 시 최대치 증가
	float HealthIncrease = 10.0f * NewLevel;
	float ManaIncrease = 5.0f * NewLevel;
	float StaminaIncrease = 5.0f * NewLevel;
	
	AttributeSet->SetMaxHealth(AttributeSet->GetMaxHealth() + HealthIncrease);
	AttributeSet->SetMaxMana(AttributeSet->GetMaxMana() + ManaIncrease);
	AttributeSet->SetMaxStamina(AttributeSet->GetMaxStamina() + StaminaIncrease);
	
	// 현재 값도 최대치로 회복
	AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
	AttributeSet->SetMana(AttributeSet->GetMaxMana());
	AttributeSet->SetStamina(AttributeSet->GetMaxStamina());
	
	// 레벨업 이벤트 발생
	OnStatsLevelUp.Broadcast(NewLevel, StatPoints);
}

void UHSStatsComponent::InitializeWarriorStats()
{
	if (!AttributeSet)
	{
		return;
	}
	
	// 전사 특화 스탯
	AttributeSet->SetMaxHealth(150.0f);
	AttributeSet->SetHealth(150.0f);
	AttributeSet->SetMaxMana(50.0f);
	AttributeSet->SetMana(50.0f);
	AttributeSet->SetMaxStamina(120.0f);
	AttributeSet->SetStamina(120.0f);
	
	AttributeSet->SetAttackPower(15.0f);
	AttributeSet->SetDefensePower(10.0f);
	AttributeSet->SetCriticalChance(0.15f);
	AttributeSet->SetCriticalDamage(2.0f);
	AttributeSet->SetMovementSpeed(550.0f);
	AttributeSet->SetAttackSpeed(0.9f);
}

void UHSStatsComponent::InitializeThiefStats()
{
	if (!AttributeSet)
	{
		return;
	}
	
	// 시프 특화 스탯
	AttributeSet->SetMaxHealth(100.0f);
	AttributeSet->SetHealth(100.0f);
	AttributeSet->SetMaxMana(80.0f);
	AttributeSet->SetMana(80.0f);
	AttributeSet->SetMaxStamina(150.0f);
	AttributeSet->SetStamina(150.0f);
	
	AttributeSet->SetAttackPower(12.0f);
	AttributeSet->SetDefensePower(5.0f);
	AttributeSet->SetCriticalChance(0.25f);
	AttributeSet->SetCriticalDamage(2.5f);
	AttributeSet->SetMovementSpeed(700.0f);
	AttributeSet->SetAttackSpeed(1.4f);
}

void UHSStatsComponent::InitializeMageStats()
{
	if (!AttributeSet)
	{
		return;
	}
	
	// 마법사 특화 스탯
	AttributeSet->SetMaxHealth(80.0f);
	AttributeSet->SetHealth(80.0f);
	AttributeSet->SetMaxMana(150.0f);
	AttributeSet->SetMana(150.0f);
	AttributeSet->SetMaxStamina(80.0f);
	AttributeSet->SetStamina(80.0f);
	
	AttributeSet->SetAttackPower(20.0f); // 마법 공격력
	AttributeSet->SetDefensePower(3.0f);
	AttributeSet->SetCriticalChance(0.20f);
	AttributeSet->SetCriticalDamage(2.2f);
	AttributeSet->SetMovementSpeed(600.0f);
	AttributeSet->SetAttackSpeed(1.2f);
}

// 공유 능력 시스템을 위한 추가 메서드 구현

float UHSStatsComponent::GetCurrentMana() const
{
	if (!AttributeSet)
	{
		return 0.0f;
	}
	
	return AttributeSet->GetMana();
}

void UHSStatsComponent::SetCurrentMana(float NewMana)
{
	if (!AttributeSet)
	{
		return;
	}
	
	AttributeSet->SetMana(NewMana);
}

float UHSStatsComponent::GetCurrentStamina() const
{
	if (!AttributeSet)
	{
		return 0.0f;
	}
	
	return AttributeSet->GetStamina();
}

void UHSStatsComponent::SetCurrentStamina(float NewStamina)
{
	if (!AttributeSet)
	{
		return;
	}
	
	AttributeSet->SetStamina(NewStamina);
}

void UHSStatsComponent::ApplyBuff(const FBuffData& BuffData)
{
	if (BuffData.BuffID.IsEmpty() || BuffData.BuffType == EBuffType::None)
	{
		return;
	}
	
	// 기존 버프 확인
	FBuffData* ExistingBuff = ActiveBuffs.FindByPredicate([&BuffData](const FBuffData& Buff)
	{
		return Buff.BuffID == BuffData.BuffID;
	});
	
	if (ExistingBuff)
	{
		if (BuffData.bStackable && ExistingBuff->CurrentStacks < 10) // 최대 10스택
		{
			ExistingBuff->CurrentStacks++;
			ExistingBuff->Value += BuffData.Value;
			ExistingBuff->RemainingTime = BuffData.Duration;
		}
		else
		{
			// 스택 불가능한 경우 시간만 갱신
			ExistingBuff->RemainingTime = BuffData.Duration;
		}
	}
	else
	{
		// 새로운 버프 추가
		FBuffData NewBuff = BuffData;
		NewBuff.RemainingTime = BuffData.Duration;
		ActiveBuffs.Add(NewBuff);
		
		// 즉시 적용 버프 처리
		ApplyBuffEffect(NewBuff, true);
		
		// 지속 시간이 있는 버프는 타이머 설정
		if (BuffData.Duration > 0.0f)
		{
			FTimerHandle& TimerHandle = BuffTimerHandles.FindOrAdd(BuffData.BuffID);
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, 
				[this, BuffID = BuffData.BuffID]() { RemoveBuff(BuffID); }, 
				BuffData.Duration, false);
		}
	}
}

void UHSStatsComponent::RemoveBuff(const FString& BuffID)
{
	int32 BuffIndex = ActiveBuffs.IndexOfByPredicate([&BuffID](const FBuffData& Buff)
	{
		return Buff.BuffID == BuffID;
	});
	
	if (BuffIndex != INDEX_NONE)
	{
		FBuffData& BuffToRemove = ActiveBuffs[BuffIndex];
		
		// 버프 효과 제거
		ApplyBuffEffect(BuffToRemove, false);
		
		// 버프 목록에서 제거
		ActiveBuffs.RemoveAt(BuffIndex);
		
		// 타이머 제거
		if (FTimerHandle* TimerHandle = BuffTimerHandles.Find(BuffID))
		{
			GetWorld()->GetTimerManager().ClearTimer(*TimerHandle);
			BuffTimerHandles.Remove(BuffID);
		}
	}
}

void UHSStatsComponent::ClearAllBuffs()
{
	// 모든 버프 효과 제거
	for (const FBuffData& Buff : ActiveBuffs)
	{
		ApplyBuffEffect(Buff, false);
	}
	
	// 버프 목록 초기화
	ActiveBuffs.Empty();
	
	// 모든 타이머 제거
	for (auto& TimerPair : BuffTimerHandles)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerPair.Value);
	}
	BuffTimerHandles.Empty();
}

bool UHSStatsComponent::HasBuff(const FString& BuffID) const
{
	return ActiveBuffs.ContainsByPredicate([&BuffID](const FBuffData& Buff)
	{
		return Buff.BuffID == BuffID;
	});
}

TArray<FBuffData> UHSStatsComponent::GetActiveBuffs() const
{
	return ActiveBuffs;
}

void UHSStatsComponent::ApplyBuffEffect(const FBuffData& BuffData, bool bApply)
{
	if (!AttributeSet)
	{
		return;
	}
	
	float Multiplier = bApply ? 1.0f : -1.0f;
	float Value = BuffData.Value * Multiplier;
	
	if (BuffData.bIsPercentage)
	{
		// 비율 기반 버프
		switch (BuffData.BuffType)
		{
			case EBuffType::Health:
				AttributeSet->SetMaxHealth(AttributeSet->GetMaxHealth() * (1.0f + Value));
				break;
			case EBuffType::Attack:
				AttributeSet->SetAttackPower(AttributeSet->GetAttackPower() * (1.0f + Value));
				break;
			case EBuffType::Defense:
				AttributeSet->SetDefensePower(AttributeSet->GetDefensePower() * (1.0f + Value));
				break;
			case EBuffType::AttackSpeed:
				AttributeSet->SetAttackSpeed(AttributeSet->GetAttackSpeed() * (1.0f + Value));
				break;
			case EBuffType::MovementSpeed:
				AttributeSet->SetMovementSpeed(AttributeSet->GetMovementSpeed() * (1.0f + Value));
				break;
			case EBuffType::CriticalChance:
				AttributeSet->SetCriticalChance(AttributeSet->GetCriticalChance() + Value);
				break;
		}
	}
	else
	{
		// 고정값 버프
		switch (BuffData.BuffType)
		{
			case EBuffType::Health:
				AttributeSet->SetMaxHealth(AttributeSet->GetMaxHealth() + Value);
				break;
			case EBuffType::Attack:
				AttributeSet->SetAttackPower(AttributeSet->GetAttackPower() + Value);
				break;
			case EBuffType::Defense:
				AttributeSet->SetDefensePower(AttributeSet->GetDefensePower() + Value);
				break;
			case EBuffType::AttackSpeed:
				AttributeSet->SetAttackSpeed(AttributeSet->GetAttackSpeed() + Value);
				break;
			case EBuffType::MovementSpeed:
				AttributeSet->SetMovementSpeed(AttributeSet->GetMovementSpeed() + Value);
				break;
			case EBuffType::CriticalChance:
				AttributeSet->SetCriticalChance(AttributeSet->GetCriticalChance() + Value);
				break;
		}
	}
}
