// HuntingSpirit Game - Combat Component Implementation

#include "HSCombatComponent.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HuntingSpirit/Combat/HSHitReactionComponent.h"
#include "HuntingSpirit/Characters/Stats/HSStatsComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

// 생성자
UHSCombatComponent::UHSCombatComponent()
{
    // Tick 활성화
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 0.1초마다 상태 효과 처리

    // 네트워크 복제 활성화
    SetIsReplicatedByDefault(true);

    // 기본값 설정
    MaxHealth = 100.0f;
    CurrentHealth = MaxHealth;
    PhysicalArmor = 0.0f;
    MagicalArmor = 0.0f;
    bEnableHealthRegeneration = false;
    HealthRegenerationRate = 1.0f;
    HealthRegenerationDelay = 5.0f;
    bInvincible = false;
    InvincibilityDuration = 0.5f;

    // 오너 캐릭터 및 컴포넌트 캐시 초기화
    OwnerCharacter = nullptr;
    HitReactionComponent = nullptr;
}

// 컴포넌트 초기화
void UHSCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    // 오너 캐릭터 캐시
    OwnerCharacter = Cast<AHSCharacterBase>(GetOwner());
    
    // 히트 반응 컴포넌트 찾기 및 캐시
    if (OwnerCharacter)
    {
        HitReactionComponent = OwnerCharacter->FindComponentByClass<UHSHitReactionComponent>();
    }

    // 체력 초기화
    CurrentHealth = MaxHealth;
}

// 네트워크 복제 설정
void UHSCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 체력과 상태 효과만 복제
    DOREPLIFETIME(UHSCombatComponent, CurrentHealth);
    DOREPLIFETIME(UHSCombatComponent, ActiveStatusEffects);
}

// 매 프레임 호출
void UHSCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 상태 효과 처리 (서버에서만)
    if (GetOwner()->HasAuthority())
    {
        ProcessStatusEffects(DeltaTime);
    }
}

// 데미지 적용 함수
FHSDamageResult UHSCombatComponent::ApplyDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    FHSDamageResult DamageResult;

    // 사망 상태이거나 무적 상태면 데미지 무효
    if (IsDead() || bInvincible)
    {
        return DamageResult;
    }

    // 네트워크 환경에서는 서버에서만 데미지 적용
    if (!GetOwner()->HasAuthority())
    {
        ServerApplyDamage(DamageInfo, DamageInstigator);
        return DamageResult;
    }

    // 최종 데미지 계산
    float FinalDamage = CalculateFinalDamage(DamageInfo, DamageInstigator);
    
    // 다음 공격 데미지 배율 적용
    FinalDamage *= NextAttackDamageMultiplier;
    NextAttackDamageMultiplier = 1.0f; // 사용 후 리셋
    
    // 추가 원소 데미지 적용
    for (const auto& ElementalDamage : AdditionalElementalDamage)
    {
        if (ElementalDamage.Key == DamageInfo.DamageType)
        {
            FinalDamage += ElementalDamage.Value;
        }
    }
    
    // 크리티컬 히트 판정
    bool bCritical = DamageInfo.IsCriticalHit();
    if (bCritical)
    {
        FinalDamage *= DamageInfo.CriticalMultiplier;
        DamageResult.bCriticalHit = true;
        
        // 크리티컬 히트 이벤트 발생
        OnCriticalHit.Broadcast(GetOwner(), FinalDamage);
    }

    // 방어력 적용 (bIgnoreArmor가 false일 때만)
    if (!DamageInfo.bIgnoreArmor)
    {
        float ArmorToUse = (DamageInfo.DamageType == EHSDamageType::Physical) ? PhysicalArmor : MagicalArmor;
        float ArmorReduction = CalculateArmorReduction(FinalDamage, ArmorToUse, DamageInfo.ArmorPenetration);
        FinalDamage -= ArmorReduction;
        DamageResult.MitigatedDamage = ArmorReduction;
    }

    // 데미지 타입별 저항력 적용
    float Resistance = DamageResistance.GetResistanceForDamageType(DamageInfo.DamageType);
    FinalDamage *= (1.0f - Resistance);

    // 최소 1의 데미지는 보장
    FinalDamage = FMath::Max(1.0f, FinalDamage);
    DamageResult.FinalDamage = FinalDamage;

    // 데미지 공유 처리
    float ActualDamage = FinalDamage;
    if (bDamageSharingEnabled && DamageSharingTeamMembers.Num() > 0)
    {
        float SharedDamage = FinalDamage * DamageShareRatio;
        ActualDamage = FinalDamage - SharedDamage;
        
        // 공유된 데미지를 팀원들에게 분배
        float DamagePerMember = SharedDamage / DamageSharingTeamMembers.Num();
        for (const auto& TeamMember : DamageSharingTeamMembers)
        {
            if (TeamMember.IsValid())
            {
                UHSCombatComponent* TeamMemberCombat = TeamMember->FindComponentByClass<UHSCombatComponent>();
                if (TeamMemberCombat && TeamMemberCombat->IsAlive())
                {
                    // 무한 루프 방지를 위해 데미지 공유를 임시로 비활성화
                    bool bWasSharing = TeamMemberCombat->bDamageSharingEnabled;
                    TeamMemberCombat->bDamageSharingEnabled = false;
                    
                    // 직접 체력 감소 (추가 효과 없이)
                    TeamMemberCombat->SetCurrentHealth(TeamMemberCombat->GetCurrentHealth() - DamagePerMember);
                    
                    TeamMemberCombat->bDamageSharingEnabled = bWasSharing;
                }
            }
        }
    }

    // 체력 감소
    float NewHealth = FMath::Max(0.0f, CurrentHealth - ActualDamage);
    SetCurrentHealth(NewHealth);

    // 상태 효과 적용
    for (const FHSStatusEffect& Effect : DamageInfo.StatusEffects)
    {
        if (DamageInfo.ShouldApplyStatusEffect(Effect))
        {
            if (ApplyStatusEffect(Effect, DamageInstigator))
            {
                DamageResult.AppliedStatusEffects.Add(Effect);
            }
        }
    }

    // 무적 상태 활성화
    if (InvincibilityDuration > 0.0f)
    {
        SetInvincible(true);
        GetWorld()->GetTimerManager().SetTimer(InvincibilityTimerHandle, this, &UHSCombatComponent::EndInvincibility, InvincibilityDuration, false);
    }

    // 체력 자동 회복 중단
    if (bEnableHealthRegeneration)
    {
        StopHealthRegeneration();
        GetWorld()->GetTimerManager().SetTimer(HealthRegenerationTimerHandle, this, &UHSCombatComponent::StartHealthRegeneration, HealthRegenerationDelay, false);
    }

    // 히트 반응 처리
    if (HitReactionComponent)
    {
        HitReactionComponent->ProcessHitReaction(DamageInfo, DamageInstigator);
    }

    // 사망 확인
    if (IsDead())
    {
        DamageResult.bTargetKilled = true;
        HandleDeath(DamageInstigator);
    }

    // 이벤트 발생
    OnDamageReceived.Broadcast(FinalDamage, DamageInfo, DamageInstigator);
    MulticastOnDamageReceived(FinalDamage, DamageInfo, DamageInstigator);

    // 공격자에게 데미지 전달 이벤트 발생
    if (DamageInstigator)
    {
        UHSCombatComponent* InstigatorCombat = DamageInstigator->FindComponentByClass<UHSCombatComponent>();
        if (InstigatorCombat)
        {
            InstigatorCombat->OnDamageDealt.Broadcast(FinalDamage, DamageInfo, GetOwner());
        }
    }

    return DamageResult;
}

// 최종 데미지 계산 함수
float UHSCombatComponent::CalculateFinalDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator) const
{
    float BaseDamage = DamageInfo.CalculateFinalDamage();

    // 스탯 기반 또는 비율 기반 데미지 계산
    switch (DamageInfo.CalculationMode)
    {
        case EHSDamageCalculationMode::Percentage:
        {
            // 최대 체력의 비율로 데미지 계산
            BaseDamage = MaxHealth * (DamageInfo.BaseDamage / 100.0f);
            break;
        }
        case EHSDamageCalculationMode::StatBased:
        {
            // 공격자의 스탯을 기반으로 데미지 계산 (예: 공격력 * 배수)
            float AttackPower = 0.0f;
            if (DamageInstigator)
            {
                if (const AHSCharacterBase* InstigatorCharacter = Cast<AHSCharacterBase>(DamageInstigator))
                {
                    if (const UHSStatsComponent* Stats = InstigatorCharacter->GetStatsComponent())
                    {
                        AttackPower = Stats->GetAttackPower();
                    }
                }
                if (FMath::IsNearlyZero(AttackPower))
                {
                    if (const UHSCombatComponent* InstigatorCombat = DamageInstigator->FindComponentByClass<UHSCombatComponent>())
                    {
                        AttackPower = InstigatorCombat->GetMaxHealth() * 0.05f;
                    }
                }
            }

            if (AttackPower > 0.0f)
            {
                const float Scaling = (DamageInfo.BaseDamage > 0.0f) ? DamageInfo.BaseDamage : 1.0f;
                BaseDamage = AttackPower * Scaling;
            }
            break;
        }
        default:
            // Fixed 및 RandomRange는 이미 CalculateFinalDamage에서 처리됨
            break;
    }

    return BaseDamage;
}

// 힐링 적용 함수
void UHSCombatComponent::ApplyHealing(float HealAmount, AActor* HealInstigator)
{
    if (IsDead() || HealAmount <= 0.0f)
    {
        return;
    }

    float NewHealth = FMath::Min(MaxHealth, CurrentHealth + HealAmount);
    SetCurrentHealth(NewHealth);
}

// 상태 효과 적용 함수
bool UHSCombatComponent::ApplyStatusEffect(const FHSStatusEffect& StatusEffect, AActor* EffectInstigator)
{
    if (StatusEffect.EffectType == EHSStatusEffectType::None || StatusEffect.Duration <= 0.0f)
    {
        return false;
    }

    // 기존 상태 효과 확인
    FHSStatusEffect* ExistingEffect = ActiveStatusEffects.FindByPredicate([&StatusEffect](const FHSStatusEffect& Effect)
    {
        return Effect.EffectType == StatusEffect.EffectType;
    });

    if (ExistingEffect)
    {
        if (StatusEffect.bStackable && ExistingEffect->MaxStacks > 1)
        {
            // 스택 가능한 효과인 경우 강도 증가 (최대 스택 수 제한)
            float NewIntensity = FMath::Min(ExistingEffect->Intensity + StatusEffect.Intensity, 
                                          StatusEffect.Intensity * StatusEffect.MaxStacks);
            ExistingEffect->Intensity = NewIntensity;
            ExistingEffect->Duration = StatusEffect.Duration; // 지속시간 갱신
        }
        else
        {
            // 스택 불가능한 효과인 경우 새로운 효과로 교체
            *ExistingEffect = StatusEffect;
        }
    }
    else
    {
        // 새로운 상태 효과 추가
        ActiveStatusEffects.Add(StatusEffect);
    }

    // 타이머 설정 (기존 타이머가 있으면 갱신)
    FTimerHandle& TimerHandle = StatusEffectTimerHandles.FindOrAdd(StatusEffect.EffectType);
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
    
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindUFunction(this, FName("OnStatusEffectExpired"), (uint8)StatusEffect.EffectType);
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, StatusEffect.Duration, false);

    return true;
}

// 상태 효과 제거 함수
void UHSCombatComponent::RemoveStatusEffect(EHSStatusEffectType EffectType)
{
    ActiveStatusEffects.RemoveAll([EffectType](const FHSStatusEffect& Effect)
    {
        return Effect.EffectType == EffectType;
    });

    // 타이머 정리
    if (FTimerHandle* TimerHandle = StatusEffectTimerHandles.Find(EffectType))
    {
        GetWorld()->GetTimerManager().ClearTimer(*TimerHandle);
        StatusEffectTimerHandles.Remove(EffectType);
    }
}

// 모든 상태 효과 제거
void UHSCombatComponent::ClearAllStatusEffects()
{
    ActiveStatusEffects.Empty();

    // 모든 타이머 정리
    for (auto& TimerPair : StatusEffectTimerHandles)
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerPair.Value);
    }
    StatusEffectTimerHandles.Empty();
}

// 최대 체력 설정
void UHSCombatComponent::SetMaxHealth(float NewMaxHealth)
{
    float OldMaxHealth = MaxHealth;
    MaxHealth = FMath::Max(0.0f, NewMaxHealth);
    
    // 비율을 유지하면서 현재 체력 조정
    if (OldMaxHealth > 0.0f)
    {
        float HealthRatio = CurrentHealth / OldMaxHealth;
        SetCurrentHealth(MaxHealth * HealthRatio);
    }
    else
    {
        SetCurrentHealth(MaxHealth);
    }
}

// 현재 체력 설정
void UHSCombatComponent::SetCurrentHealth(float NewHealth)
{
    float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
    
    if (FMath::Abs(OldHealth - CurrentHealth) > KINDA_SMALL_NUMBER)
    {
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    }
}

// 상태 효과 보유 여부 확인
bool UHSCombatComponent::HasStatusEffect(EHSStatusEffectType EffectType) const
{
    return ActiveStatusEffects.ContainsByPredicate([EffectType](const FHSStatusEffect& Effect)
    {
        return Effect.EffectType == EffectType;
    });
}

// 상태 효과 정보 가져오기
FHSStatusEffect UHSCombatComponent::GetStatusEffect(EHSStatusEffectType EffectType) const
{
    const FHSStatusEffect* FoundEffect = ActiveStatusEffects.FindByPredicate([EffectType](const FHSStatusEffect& Effect)
    {
        return Effect.EffectType == EffectType;
    });

    return FoundEffect ? *FoundEffect : FHSStatusEffect();
}

// 방어력 계산 함수
float UHSCombatComponent::CalculateArmorReduction(float Damage, float Armor, float ArmorPenetration) const
{
    // 방어력 관통 적용
    float EffectiveArmor = Armor * (1.0f - ArmorPenetration);
    
    // 방어력 공식: 감소율 = Armor / (Armor + 100)
    // 100 방어력 = 50% 감소, 200 방어력 = 66.7% 감소
    float ReductionRatio = EffectiveArmor / (EffectiveArmor + 100.0f);
    
    return Damage * ReductionRatio;
}

// 상태 효과 처리
void UHSCombatComponent::ProcessStatusEffects(float DeltaTime)
{
    for (const FHSStatusEffect& Effect : ActiveStatusEffects)
    {
        switch (Effect.EffectType)
        {
            case EHSStatusEffectType::PoisonDot:
            case EHSStatusEffectType::Burn:
                // DoT 데미지 적용 (1초마다)
                ApplyStatusEffectDamage(Effect);
                break;
                
            case EHSStatusEffectType::Slow:
                // 이동속도 감소는 다른 컴포넌트에서 처리
                break;
                
            default:
                break;
        }
    }
}

// 상태 효과 데미지 적용
void UHSCombatComponent::ApplyStatusEffectDamage(const FHSStatusEffect& Effect)
{
    if (Effect.Intensity > 0.0f)
    {
        FHSDamageInfo DotDamageInfo;
        DotDamageInfo.BaseDamage = Effect.Intensity;
        DotDamageInfo.DamageType = (Effect.EffectType == EHSStatusEffectType::Burn) ? EHSDamageType::Fire : EHSDamageType::Poison;
        DotDamageInfo.CalculationMode = EHSDamageCalculationMode::Fixed;
        
        // 자기 자신에게 데미지 적용 (무적 상태 무시)
        bool bWasInvincible = bInvincible;
        bInvincible = false;
        ApplyDamage(DotDamageInfo, nullptr);
        bInvincible = bWasInvincible;
    }
}

// 상태 효과 만료 처리
void UHSCombatComponent::OnStatusEffectExpired(EHSStatusEffectType EffectType)
{
    RemoveStatusEffect(EffectType);
}

// 체력 자동 회복 시작
void UHSCombatComponent::StartHealthRegeneration()
{
    if (bEnableHealthRegeneration && IsAlive() && CurrentHealth < MaxHealth)
    {
        ApplyHealing(HealthRegenerationRate * 0.1f); // 0.1초마다 회복
        
        GetWorld()->GetTimerManager().SetTimer(HealthRegenerationTimerHandle, this, 
            &UHSCombatComponent::StartHealthRegeneration, 0.1f, false);
    }
}

// 체력 자동 회복 중단
void UHSCombatComponent::StopHealthRegeneration()
{
    GetWorld()->GetTimerManager().ClearTimer(HealthRegenerationTimerHandle);
}

// 무적 상태 설정
void UHSCombatComponent::SetInvincible(bool bNewInvincible)
{
    bInvincible = bNewInvincible;
}

// 무적 상태 종료
void UHSCombatComponent::EndInvincibility()
{
    SetInvincible(false);
}

// 사망 처리
void UHSCombatComponent::HandleDeath(AActor* KillerActor)
{
    // 모든 상태 효과 제거
    ClearAllStatusEffects();
    
    // 체력 회복 중단
    StopHealthRegeneration();
    
    // 캐릭터 상태 변경
    if (OwnerCharacter)
    {
        OwnerCharacter->SetCharacterState(ECharacterState::Dead);
    }
    
    // 사망 이벤트 발생
    OnCharacterDied.Broadcast(GetOwner());
    MulticastOnCharacterDied(GetOwner());
}

// 네트워크 RPC 함수들
void UHSCombatComponent::ServerApplyDamage_Implementation(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    ApplyDamage(DamageInfo, DamageInstigator);
}

void UHSCombatComponent::MulticastOnDamageReceived_Implementation(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    // 클라이언트에서 시각/청각 효과 처리
    // 예: 데미지 텍스트 표시, 피격 효과음 재생 등
}

void UHSCombatComponent::MulticastOnCharacterDied_Implementation(AActor* DeadCharacter)
{
    // 클라이언트에서 사망 연출 처리
    // 예: 사망 애니메이션, 사망 효과음 재생 등
}

// 공유 능력 시스템 관련 메서드 구현

void UHSCombatComponent::SetNextAttackDamageMultiplier(float Multiplier)
{
    NextAttackDamageMultiplier = FMath::Max(0.1f, Multiplier); // 최소 0.1배
    
    // 일정 시간 후 리셋 (10초)
    FTimerHandle ResetTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(ResetTimerHandle, [this]()
    {
        NextAttackDamageMultiplier = 1.0f;
    }, 10.0f, false);
}

void UHSCombatComponent::AddElementalDamage(EHSDamageType DamageType, float Amount)
{
    if (DamageType == EHSDamageType::Physical || DamageType == EHSDamageType::None || Amount <= 0.0f)
    {
        return; // 물리 데미지나 None 타입은 추가하지 않음
    }

    if (!GetWorld())
    {
        return;
    }

    float& CurrentAmount = AdditionalElementalDamage.FindOrAdd(DamageType);
    CurrentAmount += Amount;

    const int32 StackId = ++ElementalDamageStackIdCounter;
    FElementalDamageStackInfo& StackInfo = ActiveElementalDamageStacks.FindOrAdd(StackId);
    StackInfo.DamageType = DamageType;
    StackInfo.Amount = Amount;

    FTimerDelegate TimerDelegate;
    TimerDelegate.BindUFunction(this, FName("HandleElementalDamageExpired"), StackId);
    FTimerHandle ExpirationHandle;
    GetWorld()->GetTimerManager().SetTimer(ExpirationHandle, TimerDelegate, 5.0f, false);
}

void UHSCombatComponent::TakeDamage(float DamageAmount, EHSDamageType DamageType, AActor* DamageInstigator)
{
    // 직접 데미지 적용 (일반 ApplyDamage와 다르게 방어력 무시)
    if (IsDead() || DamageAmount <= 0.0f)
    {
        return;
    }
    
    // 데미지 정보 생성
    FHSDamageInfo DirectDamageInfo;
    DirectDamageInfo.BaseDamage = DamageAmount;
    DirectDamageInfo.DamageType = DamageType;
    DirectDamageInfo.CalculationMode = EHSDamageCalculationMode::Fixed;
    DirectDamageInfo.bIgnoreArmor = true; // 방어력 무시
    
    // 데미지 적용
    ApplyDamage(DirectDamageInfo, DamageInstigator);
}

void UHSCombatComponent::EnableDamageSharing(const TArray<AHSCharacterBase*>& TeamMembers, float ShareRatio)
{
    bDamageSharingEnabled = true;
    DamageShareRatio = FMath::Clamp(ShareRatio, 0.0f, 0.9f); // 최대 90%까지만 공유
    
    // 팀원 목록 업데이트
    DamageSharingTeamMembers.Empty();
    for (AHSCharacterBase* Member : TeamMembers)
    {
        if (Member && Member != GetOwner())
        {
            DamageSharingTeamMembers.Add(Member);
        }
    }
}

void UHSCombatComponent::DisableDamageSharing()
{
    bDamageSharingEnabled = false;
    DamageShareRatio = 0.0f;
    DamageSharingTeamMembers.Empty();
}

void UHSCombatComponent::HandleElementalDamageExpired(int32 StackId)
{
    if (FElementalDamageStackInfo* StackInfo = ActiveElementalDamageStacks.Find(StackId))
    {
        if (float* CurrentAmount = AdditionalElementalDamage.Find(StackInfo->DamageType))
        {
            *CurrentAmount = FMath::Max(0.0f, *CurrentAmount - StackInfo->Amount);
            if (*CurrentAmount <= KINDA_SMALL_NUMBER)
            {
                AdditionalElementalDamage.Remove(StackInfo->DamageType);
            }
        }
        ActiveElementalDamageStacks.Remove(StackId);
    }
}
