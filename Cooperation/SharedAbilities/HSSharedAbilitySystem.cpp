// Copyright Epic Games, Inc. All Rights Reserved.

#include "HSSharedAbilitySystem.h"
#include "../../Characters/Base/HSCharacterBase.h"
#include "../HSTeamManager.h"
#include "../../Characters/Stats/HSStatsComponent.h"
#include "../../Combat/HSCombatComponent.h"
#include "../../Combat/Damage/HSDamageType.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"
#include "HAL/PlatformMath.h"
#include "Math/UnrealMathUtility.h"

// 최적화를 위한 상수 정의
static constexpr float SPATIAL_HASH_CELL_SIZE = 500.0f;
static constexpr int32 ABILITY_POOL_INITIAL_SIZE = 10;
static constexpr float SYNERGY_CACHE_LIFETIME = 5.0f;
static constexpr float PROXIMITY_CHECK_SQUARED = true; // 제곱 거리 사용으로 최적화

UHSSharedAbilitySystem::UHSSharedAbilitySystem()
{
    AbilityPoolSize = ABILITY_POOL_INITIAL_SIZE;
    SpatialHashCellSize = SPATIAL_HASH_CELL_SIZE;
    CacheInvalidationTimer = 0.0f;
    
    // 메모리 풀 사전 할당
    AbilityPool.Reserve(AbilityPoolSize);
    for (int32 i = 0; i < AbilityPoolSize; ++i)
    {
        AbilityPool.Add(FActiveSharedAbility());
    }
}

void UHSSharedAbilitySystem::Initialize(UHSTeamManager* InTeamManager)
{
    TeamManager = InTeamManager;
    
    // 기본 공유 능력들 등록 (데이터 테이블에서 로드하는 것이 이상적)
    // 여기서는 예시로 하드코딩
    FSharedAbilityData CombinedAttack;
    CombinedAttack.AbilityID = TEXT("CombinedAttack_Basic");
    CombinedAttack.AbilityName = FText::FromString(TEXT("기본 연계 공격"));
    CombinedAttack.Description = FText::FromString(TEXT("두 명 이상의 플레이어가 동시에 공격하여 추가 데미지를 입힙니다."));
    CombinedAttack.AbilityType = ESharedAbilityType::SAT_CombinedAttack;
    CombinedAttack.RequiredConditions.Add(ESharedAbilityCondition::SAC_MinimumPlayers);
    CombinedAttack.RequiredConditions.Add(ESharedAbilityCondition::SAC_ProximityRequired);
    CombinedAttack.MinimumPlayersRequired = 2;
    CombinedAttack.MaximumRange = 800.0f;
    CombinedAttack.Cooldown = 15.0f;
    CombinedAttack.Duration = 0.0f; // 즉시 발동
    CombinedAttack.DamageMultiplier = 2.0f;
    RegisterSharedAbility(CombinedAttack);

    FSharedAbilityData TeamBuff;
    TeamBuff.AbilityID = TEXT("TeamBuff_Defense");
    TeamBuff.AbilityName = FText::FromString(TEXT("팀 방어 버프"));
    TeamBuff.Description = FText::FromString(TEXT("근처의 모든 팀원에게 방어력 버프를 부여합니다."));
    TeamBuff.AbilityType = ESharedAbilityType::SAT_TeamBuff;
    TeamBuff.RequiredConditions.Add(ESharedAbilityCondition::SAC_MinimumPlayers);
    TeamBuff.MinimumPlayersRequired = 3;
    TeamBuff.MaximumRange = 1500.0f;
    TeamBuff.Cooldown = 30.0f;
    TeamBuff.Duration = 20.0f;
    TeamBuff.DefenseMultiplier = 1.5f;
    RegisterSharedAbility(TeamBuff);

    UE_LOG(LogTemp, Log, TEXT("HSSharedAbilitySystem 초기화 완료 - %d개의 기본 능력 등록됨"), RegisteredAbilities.Num());
}

void UHSSharedAbilitySystem::Shutdown()
{
    // 활성 능력 모두 비활성화
    TArray<FName> ActiveAbilityIDs;
    ActiveAbilities.GetKeys(ActiveAbilityIDs);
    
    for (const FName& AbilityID : ActiveAbilityIDs)
    {
        DeactivateSharedAbility(AbilityID);
    }
    
    RegisteredAbilities.Empty();
    ActiveAbilities.Empty();
    AbilityCooldowns.Empty();
    SynergyBonusCache.Empty();
    SpatialHash.Empty();
    
    TeamManager = nullptr;
}

void UHSSharedAbilitySystem::RegisterSharedAbility(const FSharedAbilityData& AbilityData)
{
    if (AbilityData.AbilityID.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("RegisterSharedAbility: 유효하지 않은 AbilityID"));
        return;
    }
    
    RegisteredAbilities.Add(AbilityData.AbilityID, AbilityData);
    UE_LOG(LogTemp, Log, TEXT("공유 능력 등록됨: %s"), *AbilityData.AbilityID.ToString());
}

void UHSSharedAbilitySystem::UnregisterSharedAbility(const FName& AbilityID)
{
    // 활성화되어 있다면 먼저 비활성화
    if (IsSharedAbilityActive(AbilityID))
    {
        DeactivateSharedAbility(AbilityID);
    }
    
    RegisteredAbilities.Remove(AbilityID);
    AbilityCooldowns.Remove(AbilityID);
    
    UE_LOG(LogTemp, Log, TEXT("공유 능력 등록 해제됨: %s"), *AbilityID.ToString());
}

bool UHSSharedAbilitySystem::TryActivateSharedAbility(const FName& AbilityID, const TArray<AHSCharacterBase*>& Participants)
{
    FString FailureReason;
    if (!CanActivateSharedAbility(AbilityID, Participants, FailureReason))
    {
        OnSharedAbilityFailed.Broadcast(AbilityID, FailureReason);
        UE_LOG(LogTemp, Warning, TEXT("공유 능력 활성화 실패 [%s]: %s"), *AbilityID.ToString(), *FailureReason);
        return false;
    }
    
    ActivateAbility(AbilityID, Participants);
    return true;
}

void UHSSharedAbilitySystem::DeactivateSharedAbility(const FName& AbilityID)
{
    FActiveSharedAbility* ActiveAbility = ActiveAbilities.Find(AbilityID);
    if (!ActiveAbility || !ActiveAbility->bIsActive)
    {
        return;
    }
    
    DeactivateAbility(AbilityID);
}

bool UHSSharedAbilitySystem::CanActivateSharedAbility(const FName& AbilityID, const TArray<AHSCharacterBase*>& Participants, FString& OutFailureReason)
{
    // 능력이 등록되어 있는지 확인
    const FSharedAbilityData* AbilityData = RegisteredAbilities.Find(AbilityID);
    if (!AbilityData)
    {
        OutFailureReason = TEXT("등록되지 않은 능력입니다.");
        return false;
    }
    
    // 이미 활성화되어 있는지 확인
    if (IsSharedAbilityActive(AbilityID))
    {
        OutFailureReason = TEXT("이미 활성화된 능력입니다.");
        return false;
    }
    
    // 쿨다운 확인
    const float* Cooldown = AbilityCooldowns.Find(AbilityID);
    if (Cooldown && *Cooldown > 0.0f)
    {
        OutFailureReason = FString::Printf(TEXT("쿨다운 중입니다. (%.1f초 남음)"), *Cooldown);
        return false;
    }
    
    // 조건 확인
    for (ESharedAbilityCondition Condition : AbilityData->RequiredConditions)
    {
        switch (Condition)
        {
        case ESharedAbilityCondition::SAC_MinimumPlayers:
            if (Participants.Num() < AbilityData->MinimumPlayersRequired)
            {
                OutFailureReason = FString::Printf(TEXT("최소 %d명의 플레이어가 필요합니다."), AbilityData->MinimumPlayersRequired);
                return false;
            }
            break;
            
        case ESharedAbilityCondition::SAC_SpecificClasses:
            if (!CheckClassCombination(AbilityData->RequiredPlayerClasses, Participants))
            {
                OutFailureReason = TEXT("필요한 클래스 조합이 맞지 않습니다.");
                return false;
            }
            break;
            
        case ESharedAbilityCondition::SAC_ProximityRequired:
            if (!CheckProximity(Participants, AbilityData->MaximumRange))
            {
                OutFailureReason = TEXT("플레이어들이 너무 멀리 떨어져 있습니다.");
                return false;
            }
            break;
            
        case ESharedAbilityCondition::SAC_HealthThreshold:
            // 체력 임계값 확인 로직 (필요시 구현)
            break;
            
        case ESharedAbilityCondition::SAC_TimingSync:
            // 타이밍 동기화 확인 로직 (필요시 구현)
            break;
            
        case ESharedAbilityCondition::SAC_ResourceCost:
            // 자원 소모 확인 로직 (필요시 구현)
            break;
        }
    }
    
    return true;
}

FSharedAbilityData UHSSharedAbilitySystem::GetSharedAbilityData(const FName& AbilityID) const
{
    const FSharedAbilityData* Data = RegisteredAbilities.Find(AbilityID);
    return Data ? *Data : FSharedAbilityData();
}

TArray<FActiveSharedAbility> UHSSharedAbilitySystem::GetActiveSharedAbilities() const
{
    TArray<FActiveSharedAbility> Result;
    Result.Reserve(ActiveAbilities.Num());
    
    for (const auto& Pair : ActiveAbilities)
    {
        if (Pair.Value.bIsActive)
        {
            Result.Add(Pair.Value);
        }
    }
    
    return Result;
}

bool UHSSharedAbilitySystem::IsSharedAbilityActive(const FName& AbilityID) const
{
    const FActiveSharedAbility* ActiveAbility = ActiveAbilities.Find(AbilityID);
    return ActiveAbility && ActiveAbility->bIsActive;
}

float UHSSharedAbilitySystem::GetSharedAbilityCooldown(const FName& AbilityID) const
{
    const float* Cooldown = AbilityCooldowns.Find(AbilityID);
    return Cooldown ? *Cooldown : 0.0f;
}

void UHSSharedAbilitySystem::TickSharedAbilities(float DeltaTime)
{
    // 캐시 무효화 타이머 업데이트
    CacheInvalidationTimer += DeltaTime;
    if (CacheInvalidationTimer >= SYNERGY_CACHE_LIFETIME)
    {
        SynergyBonusCache.Empty();
        CacheInvalidationTimer = 0.0f;
    }
    
    // 활성 능력 업데이트
    TArray<FName> AbilitiesToDeactivate;
    
    for (auto& Pair : ActiveAbilities)
    {
        FActiveSharedAbility& ActiveAbility = Pair.Value;
        
        if (!ActiveAbility.bIsActive)
            continue;
            
        // 지속 시간 업데이트
        if (ActiveAbility.RemainingDuration > 0.0f)
        {
            ActiveAbility.RemainingDuration -= DeltaTime;
            
            if (ActiveAbility.RemainingDuration <= 0.0f)
            {
                AbilitiesToDeactivate.Add(Pair.Key);
            }
        }
    }
    
    // 쿨다운 업데이트
    TArray<FName> CooldownsToRemove;
    
    for (auto& Pair : AbilityCooldowns)
    {
        Pair.Value -= DeltaTime;
        
        if (Pair.Value <= 0.0f)
        {
            CooldownsToRemove.Add(Pair.Key);
        }
    }
    
    // 비활성화 처리
    for (const FName& AbilityID : AbilitiesToDeactivate)
    {
        DeactivateAbility(AbilityID);
    }
    
    // 쿨다운 제거
    for (const FName& AbilityID : CooldownsToRemove)
    {
        AbilityCooldowns.Remove(AbilityID);
        UE_LOG(LogTemp, Verbose, TEXT("공유 능력 쿨다운 완료: %s"), *AbilityID.ToString());
    }
}

float UHSSharedAbilitySystem::CalculateSynergyBonus(const TArray<AHSCharacterBase*>& Players) const
{
    if (Players.Num() < 2)
        return 1.0f;
    
    // 캐시 확인
    uint32 CombinationHash = GetPlayerCombinationHash(Players);
    const float* CachedBonus = SynergyBonusCache.Find(CombinationHash);
    if (CachedBonus)
    {
        return *CachedBonus;
    }
    
    // 시너지 보너스 계산
    float SynergyBonus = 1.0f;
    
    // 기본 인원수 보너스
    SynergyBonus += (Players.Num() - 1) * 0.1f;
    
    // 클래스 다양성 보너스
    TSet<FName> UniqueClasses;
    for (const AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            UniqueClasses.Add(Player->GetClass()->GetFName());
        }
    }
    
    if (UniqueClasses.Num() >= 3)
    {
        SynergyBonus += 0.25f; // 3개 이상의 다른 클래스가 있을 때 25% 추가 보너스
    }
    
    // 캐시에 저장
    SynergyBonusCache.Add(CombinationHash, SynergyBonus);
    
    return SynergyBonus;
}

bool UHSSharedAbilitySystem::CheckClassCombination(const TArray<FName>& RequiredClasses, const TArray<AHSCharacterBase*>& Players) const
{
    if (RequiredClasses.Num() == 0)
        return true;
    
    TArray<FName> PlayerClasses;
    for (const AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            PlayerClasses.Add(Player->GetClass()->GetFName());
        }
    }
    
    // 모든 필수 클래스가 있는지 확인
    for (const FName& RequiredClass : RequiredClasses)
    {
        if (!PlayerClasses.Contains(RequiredClass))
        {
            return false;
        }
    }
    
    return true;
}

bool UHSSharedAbilitySystem::CheckProximity(const TArray<AHSCharacterBase*>& Players, float MaxRange) const
{
    if (Players.Num() < 2)
        return true;
    
    const float MaxRangeSquared = MaxRange * MaxRange;
    
    // 모든 플레이어 쌍 간의 거리 확인 (최적화: 제곱 거리 사용)
    for (int32 i = 0; i < Players.Num() - 1; ++i)
    {
        if (!Players[i])
            continue;
            
        const FVector Location1 = Players[i]->GetActorLocation();
        
        for (int32 j = i + 1; j < Players.Num(); ++j)
        {
            if (!Players[j])
                continue;
                
            const FVector Location2 = Players[j]->GetActorLocation();
            const float DistanceSquared = FVector::DistSquared(Location1, Location2);
            
            if (DistanceSquared > MaxRangeSquared)
            {
                return false;
            }
        }
    }
    
    return true;
}

void UHSSharedAbilitySystem::ActivateAbility(const FName& AbilityID, const TArray<AHSCharacterBase*>& Participants)
{
    const FSharedAbilityData* AbilityData = RegisteredAbilities.Find(AbilityID);
    if (!AbilityData)
        return;
    
    // 메모리 풀에서 능력 인스턴스 가져오기
    FActiveSharedAbility* NewAbility = GetPooledAbility();
    if (!NewAbility)
    {
        // 풀이 부족하면 새로 생성
        ActiveAbilities.Add(AbilityID, FActiveSharedAbility());
        NewAbility = ActiveAbilities.Find(AbilityID);
    }
    else
    {
        ActiveAbilities.Add(AbilityID, *NewAbility);
        NewAbility = ActiveAbilities.Find(AbilityID);
    }
    
    // 능력 정보 설정
    NewAbility->AbilityID = AbilityID;
    NewAbility->ParticipatingPlayers = Participants;
    NewAbility->RemainingDuration = AbilityData->Duration;
    NewAbility->RemainingCooldown = 0.0f;
    NewAbility->bIsActive = true;
    
    // 효과 적용
    ApplyAbilityEffects(*AbilityData, Participants);
    
    // 쿨다운 설정
    AbilityCooldowns.Add(AbilityID, AbilityData->Cooldown);
    
    // 이벤트 브로드캐스트
    OnSharedAbilityActivated.Broadcast(AbilityID, Participants);
    
    UE_LOG(LogTemp, Log, TEXT("공유 능력 활성화됨: %s (참가자: %d명)"), *AbilityID.ToString(), Participants.Num());
}

void UHSSharedAbilitySystem::DeactivateAbility(const FName& AbilityID)
{
    FActiveSharedAbility* ActiveAbility = ActiveAbilities.Find(AbilityID);
    if (!ActiveAbility || !ActiveAbility->bIsActive)
        return;
    
    const FSharedAbilityData* AbilityData = RegisteredAbilities.Find(AbilityID);
    if (AbilityData)
    {
        RemoveAbilityEffects(*AbilityData, ActiveAbility->ParticipatingPlayers);
    }
    
    // 능력을 풀로 반환
    ReturnAbilityToPool(ActiveAbility);
    ActiveAbilities.Remove(AbilityID);
    
    // 이벤트 브로드캐스트
    OnSharedAbilityDeactivated.Broadcast(AbilityID);
    
    UE_LOG(LogTemp, Log, TEXT("공유 능력 비활성화됨: %s"), *AbilityID.ToString());
}

void UHSSharedAbilitySystem::ApplyAbilityEffects(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    switch (AbilityData.AbilityType)
    {
    case ESharedAbilityType::SAT_CombinedAttack:
        ProcessCombinedAttack(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_TeamBuff:
        ProcessTeamBuff(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_SynergyEffect:
        ProcessSynergyEffect(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_FormationBonus:
        ProcessFormationBonus(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_CooperativeDefense:
        ProcessCooperativeDefense(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_SharedResource:
        ProcessSharedResource(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_ReviveAssist:
        ProcessReviveAssist(AbilityData, Participants);
        break;
        
    case ESharedAbilityType::SAT_UltimateCombo:
        ProcessUltimateCombo(AbilityData, Participants);
        break;
    }
    
    // 시각 효과 재생
    if (AbilityData.ActivationEffect)
    {
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant)
            {
                UGameplayStatics::SpawnEmitterAttached(
                    AbilityData.ActivationEffect,
                    Participant->GetRootComponent(),
                    NAME_None,
                    FVector::ZeroVector,
                    FRotator::ZeroRotator,
                    EAttachLocation::SnapToTarget,
                    true
                );
            }
        }
    }
    
    // 사운드 효과 재생
    if (AbilityData.ActivationSound && Participants.Num() > 0 && Participants[0])
    {
        UGameplayStatics::PlaySoundAtLocation(
            Participants[0]->GetWorld(),
            AbilityData.ActivationSound,
            Participants[0]->GetActorLocation()
        );
    }
}

void UHSSharedAbilitySystem::RemoveAbilityEffects(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 능력 타입에 따른 효과 제거는 각 Process 함수에서 처리
    // 여기서는 공통적인 정리 작업 수행
    
    switch (AbilityData.AbilityType)
    {
    case ESharedAbilityType::SAT_TeamBuff:
        // 버프 제거
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetStatsComponent())
            {
                // 버프 제거 로직
                Participant->GetStatsComponent()->RemoveBuff(TEXT("SharedAbility_TeamBuff"));
            }
        }
        break;
        
    case ESharedAbilityType::SAT_FormationBonus:
        // 포메이션 보너스 제거
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetStatsComponent())
            {
                Participant->GetStatsComponent()->RemoveBuff(TEXT("SharedAbility_FormationBonus"));
            }
        }
        break;
        
    case ESharedAbilityType::SAT_CooperativeDefense:
        // 협동 방어 제거
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetStatsComponent())
            {
                Participant->GetStatsComponent()->RemoveBuff(TEXT("SharedAbility_CooperativeDefense"));
            }
        }
        break;
        
    default:
        break;
    }
}

void UHSSharedAbilitySystem::ProcessCombinedAttack(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 연계 공격 처리
    float SynergyBonus = CalculateSynergyBonus(Participants);
    float TotalDamageMultiplier = AbilityData.DamageMultiplier * SynergyBonus;
    
    // 각 참가자의 다음 공격에 데미지 보너스 적용
    for (AHSCharacterBase* Participant : Participants)
    {
        if (Participant && Participant->GetCombatComponent())
        {
            // 다음 공격에 대한 데미지 배율 설정
            Participant->GetCombatComponent()->SetNextAttackDamageMultiplier(TotalDamageMultiplier);
            
            // 시각적 표시 (빛나는 무기 효과 등)
            // 이 부분은 각 캐릭터의 무기 시스템과 연동 필요
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("연계 공격 발동! 데미지 배율: %.2f (시너지 보너스: %.2f)"), 
        TotalDamageMultiplier, SynergyBonus);
}

void UHSSharedAbilitySystem::ProcessTeamBuff(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 팀 버프 처리
    float SynergyBonus = CalculateSynergyBonus(Participants);
    
    for (AHSCharacterBase* Participant : Participants)
    {
        if (Participant && Participant->GetStatsComponent())
        {
            // 버프 적용
            FBuffData TeamBuff;
            TeamBuff.BuffID = TEXT("SharedAbility_TeamBuff");
            TeamBuff.BuffType = EBuffType::Defense;
            TeamBuff.Value = AbilityData.DefenseMultiplier * SynergyBonus;
            TeamBuff.Duration = AbilityData.Duration;
            TeamBuff.bIsPercentage = true;
            
            Participant->GetStatsComponent()->ApplyBuff(TeamBuff);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("팀 버프 발동! 방어력 증가: %.1f%%"), 
        (AbilityData.DefenseMultiplier * SynergyBonus - 1.0f) * 100.0f);
}

void UHSSharedAbilitySystem::ProcessSynergyEffect(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 시너지 효과 처리
    // 클래스별 특수 시너지 구현
    TMap<FName, int32> ClassCount;
    
    for (AHSCharacterBase* Participant : Participants)
    {
        if (Participant)
        {
            FName ClassName = Participant->GetClass()->GetFName();
            ClassCount.FindOrAdd(ClassName)++;
        }
    }
    
    // 전사 + 마법사 시너지: 마법 검 효과
    if (ClassCount.Contains(TEXT("HSWarriorCharacter")) && ClassCount.Contains(TEXT("HSMageCharacter")))
    {
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetClass()->GetFName() == TEXT("HSWarriorCharacter"))
            {
                // 전사에게 마법 데미지 추가
                if (Participant->GetCombatComponent())
                {
                    Participant->GetCombatComponent()->AddElementalDamage(EHSDamageType::Magical, 50.0f);
                }
            }
        }
    }
    
    // 시프 + 전사 시너지: 공격 속도 증가
    if (ClassCount.Contains(TEXT("HSThiefCharacter")) && ClassCount.Contains(TEXT("HSWarriorCharacter")))
    {
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetStatsComponent())
            {
                FBuffData AttackSpeedBuff;
                AttackSpeedBuff.BuffID = TEXT("SharedAbility_SynergyAttackSpeed");
                AttackSpeedBuff.BuffType = EBuffType::AttackSpeed;
                AttackSpeedBuff.Value = 1.3f;
                AttackSpeedBuff.Duration = AbilityData.Duration;
                AttackSpeedBuff.bIsPercentage = true;
                
                Participant->GetStatsComponent()->ApplyBuff(AttackSpeedBuff);
            }
        }
    }
}

void UHSSharedAbilitySystem::ProcessFormationBonus(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 포메이션 보너스 처리
    if (Participants.Num() < 3)
        return;
    
    // 삼각형 포메이션 확인
    bool bIsTriangleFormation = false;
    if (Participants.Num() >= 3)
    {
        // 간단한 삼각형 포메이션 체크 (3명이 대략적인 삼각형을 이루는지)
        FVector Center = FVector::ZeroVector;
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant)
            {
                Center += Participant->GetActorLocation();
            }
        }
        Center /= Participants.Num();
        
        // 중심에서 각 플레이어까지의 거리가 비슷한지 확인
        float AverageDistance = 0.0f;
        TArray<float> Distances;
        
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant)
            {
                float Distance = FVector::Dist(Center, Participant->GetActorLocation());
                Distances.Add(Distance);
                AverageDistance += Distance;
            }
        }
        
        AverageDistance /= Distances.Num();
        
        // 거리 편차가 작으면 포메이션으로 인정
        float Variance = 0.0f;
        for (float Distance : Distances)
        {
            Variance += FMath::Square(Distance - AverageDistance);
        }
        Variance /= Distances.Num();
        
        bIsTriangleFormation = (Variance < 10000.0f); // 100 유닛 이내의 편차
    }
    
    if (bIsTriangleFormation)
    {
        // 포메이션 보너스 적용
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetStatsComponent())
            {
                FBuffData FormationBuff;
                FormationBuff.BuffID = TEXT("SharedAbility_FormationBonus");
                FormationBuff.BuffType = EBuffType::AllStats;
                FormationBuff.Value = 1.2f;
                FormationBuff.Duration = AbilityData.Duration;
                FormationBuff.bIsPercentage = true;
                
                Participant->GetStatsComponent()->ApplyBuff(FormationBuff);
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("포메이션 보너스 발동! 모든 스탯 20%% 증가"));
    }
}

void UHSSharedAbilitySystem::ProcessCooperativeDefense(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 협동 방어 처리
    // 받는 데미지를 팀원들에게 분산
    for (AHSCharacterBase* Participant : Participants)
    {
        if (Participant && Participant->GetStatsComponent())
        {
            // 데미지 분산 버프 적용
            FBuffData DefenseBuff;
            DefenseBuff.BuffID = TEXT("SharedAbility_CooperativeDefense");
            DefenseBuff.BuffType = EBuffType::DamageReduction;
            DefenseBuff.Value = 0.7f; // 30% 데미지 감소
            DefenseBuff.Duration = AbilityData.Duration;
            DefenseBuff.bIsPercentage = true;
            
            Participant->GetStatsComponent()->ApplyBuff(DefenseBuff);
            
            // 데미지 분산 로직은 HSCombatComponent에서 처리
            if (Participant->GetCombatComponent())
            {
                Participant->GetCombatComponent()->EnableDamageSharing(Participants, 0.3f);
            }
        }
    }
}

void UHSSharedAbilitySystem::ProcessSharedResource(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 자원 공유 처리
    // 모든 참가자의 자원(마나, 스태미나 등)을 평균화
    float TotalMana = 0.0f;
    float TotalStamina = 0.0f;
    int32 ValidParticipants = 0;
    
    // 총 자원 계산
    for (AHSCharacterBase* Participant : Participants)
    {
        if (Participant && Participant->GetStatsComponent())
        {
            TotalMana += Participant->GetStatsComponent()->GetCurrentMana();
            TotalStamina += Participant->GetStatsComponent()->GetCurrentStamina();
            ValidParticipants++;
        }
    }
    
    if (ValidParticipants > 0)
    {
        float AverageMana = TotalMana / ValidParticipants;
        float AverageStamina = TotalStamina / ValidParticipants;
        
        // 평균값으로 설정
        for (AHSCharacterBase* Participant : Participants)
        {
            if (Participant && Participant->GetStatsComponent())
            {
                Participant->GetStatsComponent()->SetCurrentMana(AverageMana);
                Participant->GetStatsComponent()->SetCurrentStamina(AverageStamina);
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("자원 공유 완료! 평균 마나: %.1f, 평균 스태미나: %.1f"), 
            AverageMana, AverageStamina);
    }
}

void UHSSharedAbilitySystem::ProcessReviveAssist(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 부활 지원 처리
    // 다운된 플레이어를 빠르게 부활
    for (AHSCharacterBase* Participant : Participants)
    {
        if (Participant && Participant->GetStatsComponent())
        {
            // 부활 속도 증가 버프
            FBuffData ReviveBuff;
            ReviveBuff.BuffID = TEXT("SharedAbility_ReviveAssist");
            ReviveBuff.BuffType = EBuffType::ReviveSpeed;
            ReviveBuff.Value = 2.0f; // 2배 빠른 부활
            ReviveBuff.Duration = AbilityData.Duration;
            ReviveBuff.bIsPercentage = true;
            
            Participant->GetStatsComponent()->ApplyBuff(ReviveBuff);
        }
    }
}

void UHSSharedAbilitySystem::ProcessUltimateCombo(const FSharedAbilityData& AbilityData, const TArray<AHSCharacterBase*>& Participants)
{
    // 궁극기 콤보 처리
    // 모든 참가자의 궁극기를 연계하여 강력한 효과 발동
    
    // 화면 전체에 강력한 효과 발동
    if (Participants.Num() > 0 && Participants[0])
    {
        UWorld* World = Participants[0]->GetWorld();
        if (World)
        {
            // 거대한 폭발 효과
            FVector ComboCenter = FVector::ZeroVector;
            for (AHSCharacterBase* Participant : Participants)
            {
                if (Participant)
                {
                    ComboCenter += Participant->GetActorLocation();
                }
            }
            ComboCenter /= Participants.Num();
            
            // 범위 내 모든 적에게 막대한 데미지
            TArray<FOverlapResult> OverlapResults;
            FCollisionQueryParams QueryParams;
            // TArray<AHSCharacterBase*>를 TArray<AActor*>로 변환
            TArray<AActor*> ActorsToIgnore;
            for (AHSCharacterBase* Participant : Participants)
            {
                if (Participant)
                {
                    ActorsToIgnore.Add(Cast<AActor>(Participant));
                }
            }
            QueryParams.AddIgnoredActors(ActorsToIgnore);
            
            World->OverlapMultiByChannel(
                OverlapResults,
                ComboCenter,
                FQuat::Identity,
                ECollisionChannel::ECC_Pawn,
                FCollisionShape::MakeSphere(2000.0f),
                QueryParams
            );
            
            float ComboDamage = 1000.0f * Participants.Num() * CalculateSynergyBonus(Participants);
            
            for (const FOverlapResult& Result : OverlapResults)
            {
                AHSCharacterBase* Enemy = Cast<AHSCharacterBase>(Result.GetActor());
                if (Enemy && Enemy->GetTeamID() != Participants[0]->GetTeamID())
                {
                    // 적에게 데미지 적용
                    if (Enemy->GetCombatComponent())
                    {
                        Enemy->GetCombatComponent()->TakeDamage(ComboDamage, EHSDamageType::Magical, Participants[0]);
                    }
                }
            }
            
            UE_LOG(LogTemp, Log, TEXT("궁극기 콤보 발동! 총 데미지: %f"), ComboDamage);
        }
    }
}

FActiveSharedAbility* UHSSharedAbilitySystem::GetPooledAbility()
{
    for (FActiveSharedAbility& Ability : AbilityPool)
    {
        if (!Ability.bIsActive)
        {
            return &Ability;
        }
    }
    
    // 풀이 부족하면 확장
    int32 NewIndex = AbilityPool.AddDefaulted();
    return &AbilityPool[NewIndex];
}

void UHSSharedAbilitySystem::ReturnAbilityToPool(FActiveSharedAbility* Ability)
{
    if (Ability)
    {
        Ability->AbilityID = NAME_None;
        Ability->ParticipatingPlayers.Empty();
        Ability->RemainingDuration = 0.0f;
        Ability->RemainingCooldown = 0.0f;
        Ability->bIsActive = false;
    }
}

void UHSSharedAbilitySystem::UpdateSpatialHash(const TArray<AHSCharacterBase*>& Players)
{
    SpatialHash.Empty();
    
    for (AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            FVector Location = Player->GetActorLocation();
            int32 CellX = FMath::FloorToInt(Location.X / SpatialHashCellSize);
            int32 CellY = FMath::FloorToInt(Location.Y / SpatialHashCellSize);
            int32 CellZ = FMath::FloorToInt(Location.Z / SpatialHashCellSize);
            
            int32 HashKey = CellX + (CellY * 73856093) + (CellZ * 19349663);
            
            TArray<AHSCharacterBase*>& CellPlayers = SpatialHash.FindOrAdd(HashKey);
            CellPlayers.Add(Player);
        }
    }
}

uint32 UHSSharedAbilitySystem::GetPlayerCombinationHash(const TArray<AHSCharacterBase*>& Players) const
{
    uint32 Hash = 0;
    
    for (const AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            Hash = HashCombine(Hash, GetTypeHash(Player->GetUniqueID()));
        }
    }
    
    return Hash;
}
