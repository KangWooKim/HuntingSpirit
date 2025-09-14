// Fill out your copyright notice in the Description page of Project Settings.

#include "HSStatsManager.h"
#include "Engine/DataTable.h"
#include "Kismet/KismetSystemLibrary.h"

// 싱글톤 인스턴스 초기화
UHSStatsManager* UHSStatsManager::Instance = nullptr;

UHSStatsManager* UHSStatsManager::GetInstance()
{
	if (!Instance)
	{
		Instance = NewObject<UHSStatsManager>(GetTransientPackage(), UHSStatsManager::StaticClass(), TEXT("StatsManager"), RF_Standalone);
		Instance->AddToRoot(); // GC 방지
		Instance->InitializeClassGrowthData();
	}
	return Instance;
}

void UHSStatsManager::SetLevelDataTable(UDataTable* DataTable)
{
	LevelDataTable = DataTable;
	// 캐시 클리어
	LevelDataCache.Empty();
}

FHSLevelDataTableRow UHSStatsManager::GetLevelData(int32 Level) const
{
	// 캐시에서 먼저 확인
	const FHSLevelDataTableRow* CachedData = GetCachedLevelData(Level);
	if (CachedData)
	{
		return *CachedData;
	}

	// 데이터 테이블에서 로드
	if (LevelDataTable)
	{
		FString RowName = FString::Printf(TEXT("Level_%d"), Level);
		FHSLevelDataTableRow* LevelData = LevelDataTable->FindRow<FHSLevelDataTableRow>(FName(*RowName), TEXT("GetLevelData"));
		if (LevelData)
		{
			// 캐시에 저장
			LevelDataCache.Add(Level, *LevelData);
			return *LevelData;
		}
	}

	// 기본값 반환
	return FHSLevelDataTableRow();
}

FHSClassStatGrowth UHSStatsManager::GetClassGrowthData(const FName& ClassName) const
{
	if (const FHSClassStatGrowth* GrowthData = ClassGrowthCache.Find(ClassName))
	{
		return *GrowthData;
	}

	// 기본값 반환
	return FHSClassStatGrowth();
}

float UHSStatsManager::CalculateDamage(float BaseDamage, float AttackPower, float DefensePower, bool bIsCritical, float CritMultiplier) const
{
	// 공격력 배수 (1.0 + 공격력/100)
	float AttackMultiplier = 1.0f + (AttackPower / 100.0f);
	
	// 방어력 감소 계산 (방어력이 높을수록 데미지 감소)
	float DefenseReduction = 100.0f / (100.0f + DefensePower);
	
	// 최종 데미지 계산
	float FinalDamage = BaseDamage * AttackMultiplier * DefenseReduction;
	
	// 크리티컬 적용
	if (bIsCritical)
	{
		FinalDamage *= CritMultiplier;
	}
	
	// 최소 데미지 보장 (1)
	return FMath::Max(1.0f, FinalDamage);
}

int32 UHSStatsManager::CalculateExperienceReward(int32 EnemyLevel, int32 PlayerLevel, float BaseExp) const
{
	// 레벨 차이에 따른 경험치 보정
	int32 LevelDifference = EnemyLevel - PlayerLevel;
	
	// 보정 계수 계산 (-5 ~ +5 레벨 범위에서 보정)
	float Multiplier = 1.0f;
	if (LevelDifference > 5)
	{
		Multiplier = 1.5f; // 높은 레벨 적 보너스
	}
	else if (LevelDifference >= -5)
	{
		// 선형 보간
		Multiplier = 1.0f + (LevelDifference * 0.1f);
	}
	else
	{
		Multiplier = 0.5f; // 낮은 레벨 적 패널티
	}
	
	// 최종 경험치 계산
	int32 FinalExp = FMath::RoundToInt(BaseExp * Multiplier);
	
	// 최소 경험치 보장
	return FMath::Max(1, FinalExp);
}

TMap<FName, int32> UHSStatsManager::GetRecommendedStatDistribution(const FName& ClassName, int32 AvailablePoints) const
{
	TMap<FName, int32> Distribution;
	
	// 직업별 추천 스탯 분배 비율
	if (ClassName == "Warrior")
	{
		// 전사: 힘 40%, 체력 30%, 민첩 20%, 지능 10%
		Distribution.Add("Strength", FMath::RoundToInt(AvailablePoints * 0.4f));
		Distribution.Add("MaxHealth", FMath::RoundToInt(AvailablePoints * 0.3f));
		Distribution.Add("Agility", FMath::RoundToInt(AvailablePoints * 0.2f));
		Distribution.Add("Intelligence", AvailablePoints - Distribution["Strength"] - Distribution["MaxHealth"] - Distribution["Agility"]);
	}
	else if (ClassName == "Thief")
	{
		// 시프: 민첩 40%, 힘 25%, 체력 20%, 지능 15%
		Distribution.Add("Agility", FMath::RoundToInt(AvailablePoints * 0.4f));
		Distribution.Add("Strength", FMath::RoundToInt(AvailablePoints * 0.25f));
		Distribution.Add("MaxHealth", FMath::RoundToInt(AvailablePoints * 0.2f));
		Distribution.Add("Intelligence", AvailablePoints - Distribution["Agility"] - Distribution["Strength"] - Distribution["MaxHealth"]);
	}
	else if (ClassName == "Mage")
	{
		// 마법사: 지능 40%, 마나 30%, 체력 20%, 민첩 10%
		Distribution.Add("Intelligence", FMath::RoundToInt(AvailablePoints * 0.4f));
		Distribution.Add("MaxMana", FMath::RoundToInt(AvailablePoints * 0.3f));
		Distribution.Add("MaxHealth", FMath::RoundToInt(AvailablePoints * 0.2f));
		Distribution.Add("Agility", AvailablePoints - Distribution["Intelligence"] - Distribution["MaxMana"] - Distribution["MaxHealth"]);
	}
	
	return Distribution;
}

void UHSStatsManager::ClearCache()
{
	LevelDataCache.Empty();
	UE_LOG(LogTemp, Warning, TEXT("HSStatsManager: 캐시가 클리어되었습니다."));
}

void UHSStatsManager::PrintDebugInfo() const
{
	UE_LOG(LogTemp, Warning, TEXT("===== HSStatsManager 디버그 정보 ====="));
	UE_LOG(LogTemp, Warning, TEXT("레벨 데이터 테이블: %s"), LevelDataTable ? *LevelDataTable->GetName() : TEXT("없음"));
	UE_LOG(LogTemp, Warning, TEXT("캐시된 레벨 데이터 수: %d"), LevelDataCache.Num());
	UE_LOG(LogTemp, Warning, TEXT("등록된 직업 수: %d"), ClassGrowthCache.Num());
	
	// 직업별 성장률 정보 출력
	for (const auto& ClassPair : ClassGrowthCache)
	{
		const FHSClassStatGrowth& Growth = ClassPair.Value;
		UE_LOG(LogTemp, Warning, TEXT("직업: %s - 체력: %.2f, 마나: %.2f, 스태미너: %.2f, 공격력: %.2f, 방어력: %.2f"),
			*ClassPair.Key.ToString(),
			Growth.HealthPerLevel,
			Growth.ManaPerLevel,
			Growth.StaminaPerLevel,
			Growth.AttackPowerPerLevel,
			Growth.DefensePowerPerLevel);
	}
	UE_LOG(LogTemp, Warning, TEXT("===================================="));
}

void UHSStatsManager::InitializeClassGrowthData()
{
	// 전사 성장률
	FHSClassStatGrowth WarriorGrowth;
	WarriorGrowth.HealthPerLevel = 15.0f;
	WarriorGrowth.ManaPerLevel = 3.0f;
	WarriorGrowth.StaminaPerLevel = 10.0f;
	WarriorGrowth.AttackPowerPerLevel = 3.0f;
	WarriorGrowth.DefensePowerPerLevel = 2.0f;
	ClassGrowthCache.Add("Warrior", WarriorGrowth);
	
	// 시프 성장률
	FHSClassStatGrowth ThiefGrowth;
	ThiefGrowth.HealthPerLevel = 10.0f;
	ThiefGrowth.ManaPerLevel = 5.0f;
	ThiefGrowth.StaminaPerLevel = 15.0f;
	ThiefGrowth.AttackPowerPerLevel = 2.5f;
	ThiefGrowth.DefensePowerPerLevel = 1.5f;
	ClassGrowthCache.Add("Thief", ThiefGrowth);
	
	// 마법사 성장률
	FHSClassStatGrowth MageGrowth;
	MageGrowth.HealthPerLevel = 8.0f;
	MageGrowth.ManaPerLevel = 15.0f;
	MageGrowth.StaminaPerLevel = 5.0f;
	MageGrowth.AttackPowerPerLevel = 4.0f;
	MageGrowth.DefensePowerPerLevel = 1.0f;
	ClassGrowthCache.Add("Mage", MageGrowth);
}

const FHSLevelDataTableRow* UHSStatsManager::GetCachedLevelData(int32 Level) const
{
	return LevelDataCache.Find(Level);
}
