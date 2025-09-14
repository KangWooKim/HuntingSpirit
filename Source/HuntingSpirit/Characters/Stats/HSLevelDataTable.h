// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HSLevelDataTable.generated.h"

/**
 * 레벨 데이터 테이블 구조체
 * DataTable을 사용하여 레벨 정보를 효율적으로 관리
 * Flyweight 패턴 적용으로 메모리 최적화
 */
USTRUCT(BlueprintType)
struct FHSLevelDataTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 레벨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 Level;

	/** 다음 레벨까지 필요한 경험치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 RequiredExperience;

	/** 레벨업 시 획득하는 스탯 포인트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 StatPointsReward;

	/** 레벨업 시 추가 체력 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float HealthBonus;

	/** 레벨업 시 추가 마나 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float ManaBonus;

	/** 레벨업 시 추가 스태미너 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float StaminaBonus;

	FHSLevelDataTableRow()
	{
		Level = 1;
		RequiredExperience = 100;
		StatPointsReward = 3;
		HealthBonus = 10.0f;
		ManaBonus = 5.0f;
		StaminaBonus = 5.0f;
	}
};

/**
 * 스탯 증가량 계산을 위한 구조체
 * 레벨업 시 직업별 스탯 증가량 정의
 */
USTRUCT(BlueprintType)
struct FHSClassStatGrowth
{
	GENERATED_BODY()

	/** 레벨당 체력 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	float HealthPerLevel;

	/** 레벨당 마나 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	float ManaPerLevel;

	/** 레벨당 스태미너 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	float StaminaPerLevel;

	/** 레벨당 공격력 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	float AttackPowerPerLevel;

	/** 레벨당 방어력 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	float DefensePowerPerLevel;

	FHSClassStatGrowth()
	{
		HealthPerLevel = 10.0f;
		ManaPerLevel = 5.0f;
		StaminaPerLevel = 5.0f;
		AttackPowerPerLevel = 2.0f;
		DefensePowerPerLevel = 1.0f;
	}
};

/**
 * 버프/디버프 데이터 구조체
 * 상태 효과를 효율적으로 관리
 */
USTRUCT(BlueprintType)
struct FHSStatModifierEffect
{
	GENERATED_BODY()

	/** 효과 ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName EffectID;

	/** 효과 타입 (버프/디버프) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDebuff;

	/** 지속 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration;

	/** 틱 간격 (0이면 지속 효과) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TickInterval;

	/** 스탯 변경량 (곱연산) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, float> StatModifiers;

	/** 스탯 변경량 (덧셈) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, float> StatAdditions;

	FHSStatModifierEffect()
	{
		EffectID = NAME_None;
		bIsDebuff = false;
		Duration = 0.0f;
		TickInterval = 0.0f;
	}
};
