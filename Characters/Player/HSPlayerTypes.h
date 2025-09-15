/**
 * @file HSPlayerTypes.h
 * @brief 플레이어 캐릭터 관련 타입 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#pragma once

#include "CoreMinimal.h"
#include "HSPlayerTypes.generated.h"

/**
 * @enum EHSPlayerClass
 * @brief 플레이어 캐릭터 직업 열거형
 * @details 게임에서 선택 가능한 플레이어 직업들을 정의
 */
UENUM(BlueprintType)
enum class EHSPlayerClass : uint8
{
    None        UMETA(DisplayName = "None"),      ///< 직업 없음
    Warrior     UMETA(DisplayName = "Warrior"),   ///< 전사 (대검 사용, 높은 체력과 방어력)
    Thief       UMETA(DisplayName = "Thief"),     ///< 시프 (빠른 공격, 높은 이동속도)
    Mage        UMETA(DisplayName = "Mage")       ///< 마법사 (원거리 마법 공격, 높은 마나)
};
