// HuntingSpirit Game - Combat Component Header
// 캐릭터의 전투 관련 기능을 담당하는 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "HuntingSpirit/Combat/Damage/HSDamageType.h"
#include "HSCombatComponent.generated.h"

class AHSCharacterBase;
class UHSHitReactionComponent;

// 전투 이벤트 델리게이트 정의
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamageReceived, float, DamageAmount, const FHSDamageInfo&, DamageInfo, AActor*, DamageInstigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamageDealt, float, DamageAmount, const FHSDamageInfo&, DamageInfo, AActor*, DamageTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDied, AActor*, DeadCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCriticalHit, AActor*, Target, float, CriticalDamage);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 생성자
    UHSCombatComponent();

    // 매 프레임 호출
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 네트워크 복제 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 데미지 적용 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    FHSDamageResult ApplyDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    // 데미지 계산 함수 (방어력 고려)
    UFUNCTION(BlueprintPure, Category = "Combat")
    float CalculateFinalDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator = nullptr) const;

    // 힐링 적용 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyHealing(float HealAmount, AActor* HealInstigator = nullptr);

    // 상태 효과 적용 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool ApplyStatusEffect(const FHSStatusEffect& StatusEffect, AActor* EffectInstigator = nullptr);

    // 상태 효과 제거 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RemoveStatusEffect(EHSStatusEffectType EffectType);

    // 모든 상태 효과 제거
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ClearAllStatusEffects();

    // 체력 관련 함수
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    float GetHealthPercentage() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void SetMaxHealth(float NewMaxHealth);

    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void SetCurrentHealth(float NewHealth);

    // 방어력 관련 함수
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    float GetPhysicalArmor() const { return PhysicalArmor; }

    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    float GetMagicalArmor() const { return MagicalArmor; }

    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void SetPhysicalArmor(float NewArmor) { PhysicalArmor = FMath::Max(0.0f, NewArmor); }

    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void SetMagicalArmor(float NewArmor) { MagicalArmor = FMath::Max(0.0f, NewArmor); }

    // 저항력 관련 함수
    UFUNCTION(BlueprintPure, Category = "Combat|Resistance")
    FHSDamageResistance GetDamageResistance() const { return DamageResistance; }

    UFUNCTION(BlueprintCallable, Category = "Combat|Resistance")
    void SetDamageResistance(const FHSDamageResistance& NewResistance) { DamageResistance = NewResistance; }

    // 전투 상태 확인 함수
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool IsAlive() const { return CurrentHealth > 0.0f; }

    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool IsDead() const { return CurrentHealth <= 0.0f; }

    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool HasStatusEffect(EHSStatusEffectType EffectType) const;

    UFUNCTION(BlueprintPure, Category = "Combat|State")
    FHSStatusEffect GetStatusEffect(EHSStatusEffectType EffectType) const;
    
    // 공유 능력 시스템을 위한 추가 메서드들
    
    // 다음 공격에 대한 데미지 배율 설정 (연계 공격용)
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void SetNextAttackDamageMultiplier(float Multiplier);
    
    // 원소 데미지 추가 (시너지 효과용)
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void AddElementalDamage(EHSDamageType DamageType, float Amount);
    
    // 직접 데미지 적용 (궁극기 콤보용)
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void TakeDamage(float DamageAmount, EHSDamageType DamageType, AActor* DamageInstigator);
    
    // 데미지 공유 활성화 (협동 방어용)
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void EnableDamageSharing(const TArray<class AHSCharacterBase*>& TeamMembers, float ShareRatio);
    
    // 데미지 공유 비활성화
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void DisableDamageSharing();

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnDamageReceived OnDamageReceived;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnDamageDealt OnDamageDealt;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCombatHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCharacterDied OnCharacterDied;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCriticalHit OnCriticalHit;

protected:
    // 컴포넌트 초기화
    virtual void BeginPlay() override;

    // 체력 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (ClampMin = "0.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Health")
    float CurrentHealth = 100.0f;

    // 체력 자동 회복 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
    bool bEnableHealthRegeneration = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (EditCondition = "bEnableHealthRegeneration", ClampMin = "0.0"))
    float HealthRegenerationRate = 1.0f; // 초당 회복량

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (EditCondition = "bEnableHealthRegeneration", ClampMin = "0.0"))
    float HealthRegenerationDelay = 5.0f; // 데미지를 받은 후 회복 시작까지 지연시간

    // 방어력 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
    float PhysicalArmor = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
    float MagicalArmor = 0.0f;

    // 데미지 저항력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Resistance")
    FHSDamageResistance DamageResistance;

    // 현재 적용된 상태 효과들
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Status Effects")
    TArray<FHSStatusEffect> ActiveStatusEffects;

    // 무적 상태 관련
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Invincibility")
    bool bInvincible = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Invincibility")
    float InvincibilityDuration = 0.5f; // 피격 후 무적 시간

    // 타이머 핸들들
    FTimerHandle HealthRegenerationTimerHandle;
    FTimerHandle InvincibilityTimerHandle;
    TMap<EHSStatusEffectType, FTimerHandle> StatusEffectTimerHandles;

    // 오너 캐릭터 캐시
    UPROPERTY()
    AHSCharacterBase* OwnerCharacter;

    // 피격 반응 컴포넌트 캐시
    UPROPERTY()
    UHSHitReactionComponent* HitReactionComponent;
    
    // 공유 능력 시스템 관련 변수들
    
    // 다음 공격 데미지 배율
    float NextAttackDamageMultiplier = 1.0f;
    
    // 추가 원소 데미지
    TMap<EHSDamageType, float> AdditionalElementalDamage;
    
    // 데미지 공유 설정
    bool bDamageSharingEnabled = false;
    TArray<TWeakObjectPtr<AHSCharacterBase>> DamageSharingTeamMembers;
    float DamageShareRatio = 0.0f;

private:
    // 방어력 계산 함수
    float CalculateArmorReduction(float Damage, float Armor, float ArmorPenetration = 0.0f) const;

    // 상태 효과 처리 함수들
    void ProcessStatusEffects(float DeltaTime);
    void ApplyStatusEffectDamage(const FHSStatusEffect& Effect);
    void OnStatusEffectExpired(EHSStatusEffectType EffectType);

    // 체력 회복 관련 함수들
    UFUNCTION()
    void StartHealthRegeneration();

    UFUNCTION()
    void StopHealthRegeneration();

    // 무적 상태 관련 함수들
    UFUNCTION()
    void SetInvincible(bool bNewInvincible);

    UFUNCTION()
    void EndInvincibility();

    // 사망 처리 함수
    void HandleDeath(AActor* KillerActor);

    // 네트워크 RPC 함수들
    UFUNCTION(Server, Reliable)
    void ServerApplyDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnDamageReceived(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnCharacterDied(AActor* DeadCharacter);
};
