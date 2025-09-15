// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HSLevelSystem.generated.h"

/**
 * 레벨 데이터 구조체
 * 각 레벨별 필요 경험치와 보상 정보를 저장
 */
USTRUCT(BlueprintType)
struct FLevelData
{
	GENERATED_BODY()

	/** 레벨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	/** 다음 레벨까지 필요한 경험치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredExperience;

	/** 레벨업 시 획득하는 스탯 포인트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 StatPointsReward;

	FLevelData()
	{
		Level = 1;
		RequiredExperience = 100;
		StatPointsReward = 3;
	}
};

/**
 * 캐릭터의 레벨과 경험치를 관리하는 시스템
 * 레벨업, 경험치 획득, 스탯 포인트 분배 등을 처리
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSLevelSystem : public UObject
{
	GENERATED_BODY()

public:
	UHSLevelSystem();

	/** 경험치 추가 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	void AddExperience(int32 Amount);

	/** 현재 레벨 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	/** 현재 경험치 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	int32 GetCurrentExperience() const { return CurrentExperience; }

	/** 다음 레벨까지 필요한 경험치 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	int32 GetExperienceToNextLevel() const;

	/** 현재 레벨 진행도 (0.0 ~ 1.0) */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	float GetLevelProgress() const;

	/** 사용 가능한 스탯 포인트 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	int32 GetAvailableStatPoints() const { return AvailableStatPoints; }

	/** 스탯 포인트 사용 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	bool UseStatPoints(int32 Points);

	/** 레벨 데이터 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	FLevelData GetLevelData(int32 Level) const;

	/** 최대 레벨 설정 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	void SetMaxLevel(int32 NewMaxLevel);

	/** 레벨업 가능 여부 확인 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	bool CanLevelUp() const;

	/** 강제 레벨업 (치트/테스트용) */
	UFUNCTION(BlueprintCallable, Category = "Level System", meta = (CallInEditor = "true"))
	void ForceLevelUp();

	/** 레벨 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Level System")
	void ResetLevel();

	/** 델리게이트 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLevelChanged, int32 /*NewLevel*/, int32 /*StatPointsAwarded*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnExperienceGained, int32 /*ExperienceAmount*/);
	
	FOnLevelChanged OnLevelChanged;
	FOnExperienceGained OnExperienceGained;

protected:
	/** 현재 레벨 */
	UPROPERTY(BlueprintReadOnly, Category = "Level System")
	int32 CurrentLevel;

	/** 현재 경험치 */
	UPROPERTY(BlueprintReadOnly, Category = "Level System")
	int32 CurrentExperience;

	/** 사용 가능한 스탯 포인트 */
	UPROPERTY(BlueprintReadOnly, Category = "Level System")
	int32 AvailableStatPoints;

	/** 최대 레벨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level System")
	int32 MaxLevel;

	/** 레벨별 데이터 테이블 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level System")
	TArray<FLevelData> LevelDataTable;

	/** 경험치 획득 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level System", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ExperienceMultiplier;

private:
	/** 레벨업 처리 */
	void ProcessLevelUp();

	/** 기본 레벨 데이터 생성 */
	void GenerateDefaultLevelData();

	/** 특정 레벨의 필요 경험치 계산 */
	int32 CalculateRequiredExperience(int32 Level) const;
};
