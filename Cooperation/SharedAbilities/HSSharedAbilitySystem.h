// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/NoExportTypes.h"
#include "../HSTeamManager.h"
#include "HSSharedAbilitySystem.generated.h"

// 공유 능력 타입 정의
UENUM(BlueprintType)
enum class ESharedAbilityType : uint8
{
    SAT_CombinedAttack      UMETA(DisplayName = "Combined Attack"),      // 연계 공격
    SAT_TeamBuff            UMETA(DisplayName = "Team Buff"),            // 팀 버프
    SAT_SynergyEffect       UMETA(DisplayName = "Synergy Effect"),       // 시너지 효과
    SAT_FormationBonus      UMETA(DisplayName = "Formation Bonus"),      // 포메이션 보너스
    SAT_CooperativeDefense  UMETA(DisplayName = "Cooperative Defense"),  // 협동 방어
    SAT_SharedResource      UMETA(DisplayName = "Shared Resource"),      // 자원 공유
    SAT_ReviveAssist        UMETA(DisplayName = "Revive Assist"),        // 부활 지원
    SAT_UltimateCombo       UMETA(DisplayName = "Ultimate Combo")        // 궁극기 콤보
};

// 공유 능력 조건
UENUM(BlueprintType)
enum class ESharedAbilityCondition : uint8
{
    SAC_MinimumPlayers      UMETA(DisplayName = "Minimum Players"),      // 최소 플레이어 수
    SAC_SpecificClasses     UMETA(DisplayName = "Specific Classes"),     // 특정 클래스 조합
    SAC_ProximityRequired   UMETA(DisplayName = "Proximity Required"),   // 근접 거리 필요
    SAC_HealthThreshold     UMETA(DisplayName = "Health Threshold"),     // 체력 임계값
    SAC_TimingSync          UMETA(DisplayName = "Timing Sync"),          // 타이밍 동기화
    SAC_ResourceCost        UMETA(DisplayName = "Resource Cost")         // 자원 소모
};

// 공유 능력 데이터
USTRUCT(BlueprintType)
struct FSharedAbilityData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FName AbilityID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FText AbilityName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    ESharedAbilityType AbilityType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    TArray<ESharedAbilityCondition> RequiredConditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    int32 MinimumPlayersRequired;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    float MaximumRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    float Cooldown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    float Duration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    float DamageMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    float DefenseMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    TArray<FName> RequiredPlayerClasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    UTexture2D* AbilityIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    UParticleSystem* ActivationEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    USoundBase* ActivationSound;

    FSharedAbilityData()
    {
        MinimumPlayersRequired = 2;
        MaximumRange = 1000.0f;
        Cooldown = 30.0f;
        Duration = 10.0f;
        DamageMultiplier = 1.5f;
        DefenseMultiplier = 1.0f;
    }
};

// 활성화된 공유 능력 정보
USTRUCT(BlueprintType)
struct FActiveSharedAbility
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName AbilityID;

    UPROPERTY(BlueprintReadOnly)
    TArray<class AHSCharacterBase*> ParticipatingPlayers;

    UPROPERTY(BlueprintReadOnly)
    float RemainingDuration;

    UPROPERTY(BlueprintReadOnly)
    float RemainingCooldown;

    UPROPERTY(BlueprintReadOnly)
    bool bIsActive;

    FActiveSharedAbility()
    {
        RemainingDuration = 0.0f;
        RemainingCooldown = 0.0f;
        bIsActive = false;
    }
};

// 공유 능력 시스템 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSharedAbilityActivated, const FName&, AbilityID, const TArray<AHSCharacterBase*>&, Participants);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSharedAbilityDeactivated, const FName&, AbilityID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSharedAbilityFailed, const FName&, AbilityID, const FString&, Reason);

/**
 * 플레이어 간 공유 능력을 관리하는 시스템
 * 협동 플레이의 핵심 메커니즘을 담당
 */
UCLASS(Blueprintable, BlueprintType)
class HUNTINGSPIRIT_API UHSSharedAbilitySystem : public UObject
{
    GENERATED_BODY()

public:
    UHSSharedAbilitySystem();

    // 초기화 및 정리
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    void Initialize(class UHSTeamManager* InTeamManager);

    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    void Shutdown();

    // 능력 등록 및 관리
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    void RegisterSharedAbility(const FSharedAbilityData& AbilityData);

    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    void UnregisterSharedAbility(const FName& AbilityID);

    // 능력 활성화
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    bool TryActivateSharedAbility(const FName& AbilityID, const TArray<AHSCharacterBase*>& Participants);

    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    void DeactivateSharedAbility(const FName& AbilityID);

    // 능력 조건 확인
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    bool CanActivateSharedAbility(const FName& AbilityID, const TArray<AHSCharacterBase*>& Participants, FString& OutFailureReason);

    // 능력 정보 조회
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    FSharedAbilityData GetSharedAbilityData(const FName& AbilityID) const;

    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    TArray<FActiveSharedAbility> GetActiveSharedAbilities() const;

    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    bool IsSharedAbilityActive(const FName& AbilityID) const;

    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    float GetSharedAbilityCooldown(const FName& AbilityID) const;

    // 틱 업데이트
    void TickSharedAbilities(float DeltaTime);

    // 시너지 계산
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    float CalculateSynergyBonus(const TArray<AHSCharacterBase*>& Players) const;

    // 클래스 조합 확인
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    bool CheckClassCombination(const TArray<FName>& RequiredClasses, const TArray<AHSCharacterBase*>& Players) const;

    // 근접 거리 확인
    UFUNCTION(BlueprintCallable, Category = "Shared Ability")
    bool CheckProximity(const TArray<AHSCharacterBase*>& Players, float MaxRange) const;

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Shared Ability")
    FOnSharedAbilityActivated OnSharedAbilityActivated;

    UPROPERTY(BlueprintAssignable, Category = "Shared Ability")
    FOnSharedAbilityDeactivated OnSharedAbilityDeactivated;

    UPROPERTY(BlueprintAssignable, Category = "Shared Ability")
    FOnSharedAbilityFailed OnSharedAbilityFailed;

protected:
    // 능력 활성화 처리
    void ActivateAbility(const FName& AbilityID, const TArray<AHSCharacterBase*>& Participants);

    // 능력 비활성화 처리
    void DeactivateAbility(const FName& AbilityID);

    // 능력 효과 적용
    void ApplyAbilityEffects(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);

    // 능력 효과 제거
    void RemoveAbilityEffects(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);

    // 특정 능력 타입별 처리
    void ProcessCombinedAttack(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessTeamBuff(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessSynergyEffect(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessFormationBonus(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessCooperativeDefense(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessSharedResource(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessReviveAssist(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);
    void ProcessUltimateCombo(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants);

private:
    // 팀 매니저 참조
    UPROPERTY()
    class UHSTeamManager* TeamManager;

    // 등록된 공유 능력 데이터
    UPROPERTY()
    TMap<FName, FSharedAbilityData> RegisteredAbilities;

    // 활성화된 공유 능력
    UPROPERTY()
    TMap<FName, FActiveSharedAbility> ActiveAbilities;

    // 쿨다운 관리
    UPROPERTY()
    TMap<FName, float> AbilityCooldowns;

    // 메모리 풀링을 위한 능력 인스턴스 풀
    TArray<FActiveSharedAbility> AbilityPool;
    int32 AbilityPoolSize;

    // 성능 최적화를 위한 캐시
    mutable TMap<uint32, float> SynergyBonusCache;
    mutable float CacheInvalidationTimer;

    // 최적화된 거리 계산을 위한 공간 해시
    TMap<int32, TArray<AHSCharacterBase*>> SpatialHash;
    float SpatialHashCellSize;

    // 헬퍼 함수
    FActiveSharedAbility* GetPooledAbility();
    void ReturnAbilityToPool(FActiveSharedAbility* Ability);
    void UpdateSpatialHash(const TArray<AHSCharacterBase*>& Players);
    uint32 GetPlayerCombinationHash(const TArray<AHSCharacterBase*>& Players) const;
};
