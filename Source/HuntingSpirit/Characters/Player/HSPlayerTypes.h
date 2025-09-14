// HuntingSpirit Game - Player Character Types
// 플레이어 캐릭터 유형 정의

#pragma once

#include "CoreMinimal.h"
#include "HSPlayerTypes.generated.h"

/**
 * 플레이어 캐릭터 직업 열거형
 */
UENUM(BlueprintType)
enum class EHSPlayerClass : uint8
{
    None        UMETA(DisplayName = "None"),
    Warrior     UMETA(DisplayName = "Warrior"),
    Thief       UMETA(DisplayName = "Thief"),
    Mage        UMETA(DisplayName = "Mage")
};
