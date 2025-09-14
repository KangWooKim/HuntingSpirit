// Fill out your copyright notice in the Description page of Project Settings.

#include "HSLevelSystem.h"

UHSLevelSystem::UHSLevelSystem()
{
	// 기본값 설정
	CurrentLevel = 1;
	CurrentExperience = 0;
	AvailableStatPoints = 0;
	MaxLevel = 100;
	ExperienceMultiplier = 1.0f;
	
	// 기본 레벨 데이터 생성
	GenerateDefaultLevelData();
}

void UHSLevelSystem::AddExperience(int32 Amount)
{
	if (CurrentLevel >= MaxLevel)
	{
		return; // 최대 레벨 도달
	}
	
	// 경험치 배율 적용
	int32 ActualExpGain = FMath::RoundToInt(Amount * ExperienceMultiplier);
	CurrentExperience += ActualExpGain;
	
	// 경험치 획득 이벤트 발생
	OnExperienceGained.Broadcast(ActualExpGain);
	
	// 레벨업 체크
	while (CanLevelUp())
	{
		ProcessLevelUp();
	}
}

int32 UHSLevelSystem::GetExperienceToNextLevel() const
{
	if (CurrentLevel >= MaxLevel)
	{
		return 0;
	}
	
	if (LevelDataTable.IsValidIndex(CurrentLevel - 1))
	{
		return LevelDataTable[CurrentLevel - 1].RequiredExperience - CurrentExperience;
	}
	
	// 레벨 데이터가 없으면 계산
	return CalculateRequiredExperience(CurrentLevel) - CurrentExperience;
}

float UHSLevelSystem::GetLevelProgress() const
{
	if (CurrentLevel >= MaxLevel)
	{
		return 1.0f;
	}
	
	int32 RequiredExp = 0;
	if (LevelDataTable.IsValidIndex(CurrentLevel - 1))
	{
		RequiredExp = LevelDataTable[CurrentLevel - 1].RequiredExperience;
	}
	else
	{
		RequiredExp = CalculateRequiredExperience(CurrentLevel);
	}
	
	if (RequiredExp <= 0)
	{
		return 0.0f;
	}
	
	return static_cast<float>(CurrentExperience) / static_cast<float>(RequiredExp);
}

bool UHSLevelSystem::UseStatPoints(int32 Points)
{
	if (Points <= 0 || Points > AvailableStatPoints)
	{
		return false;
	}
	
	AvailableStatPoints -= Points;
	return true;
}

FLevelData UHSLevelSystem::GetLevelData(int32 Level) const
{
	if (LevelDataTable.IsValidIndex(Level - 1))
	{
		return LevelDataTable[Level - 1];
	}
	
	// 레벨 데이터가 없으면 기본값 생성
	FLevelData DefaultData;
	DefaultData.Level = Level;
	DefaultData.RequiredExperience = CalculateRequiredExperience(Level);
	DefaultData.StatPointsReward = 3 + (Level / 10); // 10레벨마다 추가 포인트
	
	return DefaultData;
}

void UHSLevelSystem::SetMaxLevel(int32 NewMaxLevel)
{
	MaxLevel = FMath::Max(1, NewMaxLevel);
	
	// 현재 레벨이 최대 레벨을 초과하는 경우 조정
	if (CurrentLevel > MaxLevel)
	{
		CurrentLevel = MaxLevel;
		CurrentExperience = 0;
	}
}

bool UHSLevelSystem::CanLevelUp() const
{
	if (CurrentLevel >= MaxLevel)
	{
		return false;
	}
	
	int32 RequiredExp = 0;
	if (LevelDataTable.IsValidIndex(CurrentLevel - 1))
	{
		RequiredExp = LevelDataTable[CurrentLevel - 1].RequiredExperience;
	}
	else
	{
		RequiredExp = CalculateRequiredExperience(CurrentLevel);
	}
	
	return CurrentExperience >= RequiredExp;
}

void UHSLevelSystem::ForceLevelUp()
{
	if (CurrentLevel >= MaxLevel)
	{
		return;
	}
	
	// 강제로 필요 경험치를 채움
	int32 RequiredExp = 0;
	if (LevelDataTable.IsValidIndex(CurrentLevel - 1))
	{
		RequiredExp = LevelDataTable[CurrentLevel - 1].RequiredExperience;
	}
	else
	{
		RequiredExp = CalculateRequiredExperience(CurrentLevel);
	}
	
	CurrentExperience = RequiredExp;
	ProcessLevelUp();
}

void UHSLevelSystem::ResetLevel()
{
	CurrentLevel = 1;
	CurrentExperience = 0;
	AvailableStatPoints = 0;
}

void UHSLevelSystem::ProcessLevelUp()
{
	if (CurrentLevel >= MaxLevel)
	{
		return;
	}
	
	// 초과 경험치 계산
	int32 RequiredExp = 0;
	int32 StatPointsAwarded = 3; // 기본 스탯 포인트
	
	if (LevelDataTable.IsValidIndex(CurrentLevel - 1))
	{
		RequiredExp = LevelDataTable[CurrentLevel - 1].RequiredExperience;
		StatPointsAwarded = LevelDataTable[CurrentLevel - 1].StatPointsReward;
	}
	else
	{
		RequiredExp = CalculateRequiredExperience(CurrentLevel);
		StatPointsAwarded = 3 + (CurrentLevel / 10);
	}
	
	// 초과 경험치 보존
	int32 OverflowExp = CurrentExperience - RequiredExp;
	CurrentExperience = FMath::Max(0, OverflowExp);
	
	// 레벨 증가
	CurrentLevel++;
	
	// 스탯 포인트 부여
	AvailableStatPoints += StatPointsAwarded;
	
	// 레벨업 이벤트 발생
	OnLevelChanged.Broadcast(CurrentLevel, StatPointsAwarded);
}

void UHSLevelSystem::GenerateDefaultLevelData()
{
	LevelDataTable.Empty();
	
	// 1-100 레벨까지의 기본 데이터 생성
	for (int32 Level = 1; Level <= 100; ++Level)
	{
		FLevelData Data;
		Data.Level = Level;
		Data.RequiredExperience = CalculateRequiredExperience(Level);
		
		// 스탯 포인트: 기본 3포인트, 10레벨마다 추가 1포인트
		Data.StatPointsReward = 3 + (Level / 10);
		
		// 특정 레벨에서 보너스 스탯 포인트
		if (Level % 25 == 0)
		{
			Data.StatPointsReward += 2; // 25, 50, 75, 100 레벨에서 추가 보너스
		}
		
		LevelDataTable.Add(Data);
	}
}

int32 UHSLevelSystem::CalculateRequiredExperience(int32 Level) const
{
	// 경험치 공식: BaseExp * (Level^1.5) + 추가 보정
	// 레벨이 올라갈수록 더 많은 경험치 필요
	
	const float BaseExp = 100.0f;
	const float LevelExponent = 1.5f;
	const float AdditionalExp = 50.0f * (Level - 1);
	
	int32 RequiredExp = FMath::RoundToInt(BaseExp * FMath::Pow(Level, LevelExponent) + AdditionalExp);
	
	// 레벨 구간별 추가 보정
	if (Level > 50)
	{
		RequiredExp *= 2; // 50레벨 이후 2배
	}
	else if (Level > 25)
	{
		RequiredExp = FMath::RoundToInt(RequiredExp * 1.5f); // 25레벨 이후 1.5배
	}
	
	return RequiredExp;
}
