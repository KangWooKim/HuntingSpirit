// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HSLevelDataTable.h"
#include "HSStatsManager.generated.h"

class UDataTable;

/**
 * 스탯 관리자 싱글톤 클래스
 * 전역적으로 스탯 데이터와 계산을 관리
 * 메모리 효율성을 위한 Flyweight 패턴 적용
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSStatsManager : public UObject
{
	GENERATED_BODY()

public:
	/** 싱글톤 인스턴스 가져오기 */
	static UHSStatsManager* GetInstance();

	/** 레벨 데이터 테이블 설정 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager")
	void SetLevelDataTable(UDataTable* DataTable);

	/** 레벨 데이터 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager")
	FHSLevelDataTableRow GetLevelData(int32 Level) const;

	/** 직업별 스탯 성장률 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager")
	FHSClassStatGrowth GetClassGrowthData(const FName& ClassName) const;

	/** 데미지 계산 공식 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager|Combat")
	float CalculateDamage(float BaseDamage, float AttackPower, float DefensePower, bool bIsCritical = false, float CritMultiplier = 2.0f) const;

	/** 경험치 계산 공식 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager|Level")
	int32 CalculateExperienceReward(int32 EnemyLevel, int32 PlayerLevel, float BaseExp) const;

	/** 스탯 포인트 분배 추천 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager")
	TMap<FName, int32> GetRecommendedStatDistribution(const FName& ClassName, int32 AvailablePoints) const;

	/** 캐시 클리어 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager")
	void ClearCache();

	/** 디버그 정보 출력 */
	UFUNCTION(BlueprintCallable, Category = "Stats Manager|Debug", CallInEditor)
	void PrintDebugInfo() const;

protected:
	/** 싱글톤 인스턴스 */
	static UHSStatsManager* Instance;

	/** 레벨 데이터 테이블 */
	UPROPERTY()
	UDataTable* LevelDataTable;

	/** 직업별 성장률 데이터 (캐시) */
	UPROPERTY()
	TMap<FName, FHSClassStatGrowth> ClassGrowthCache;

	/** 레벨 데이터 캐시 */
	mutable TMap<int32, FHSLevelDataTableRow> LevelDataCache;

private:
	/** 기본 직업별 성장률 초기화 */
	void InitializeClassGrowthData();

	/** 캐시된 레벨 데이터 가져오기 */
	const FHSLevelDataTableRow* GetCachedLevelData(int32 Level) const;
};
