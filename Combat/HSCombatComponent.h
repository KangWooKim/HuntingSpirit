/**
 * @file HSCombatComponent.h
 * @brief 캐릭터의 전투 관련 기능을 담당하는 컴포넌트 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "HuntingSpirit/Combat/Damage/HSDamageType.h"
#include "HSCombatComponent.generated.h"

class AHSCharacterBase;
class UHSHitReactionComponent;

/**
 * @brief 데미지 수신 시 호출되는 델리게이트
 * @param DamageAmount 받은 데미지 양
 * @param DamageInfo 데미지 정보 구조체
 * @param DamageInstigator 데미지를 가한 액터
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamageReceived, float, DamageAmount, const FHSDamageInfo&, DamageInfo, AActor*, DamageInstigator);

/**
 * @brief 데미지 가함 시 호출되는 델리게이트
 * @param DamageAmount 가한 데미지 양
 * @param DamageInfo 데미지 정보 구조체
 * @param DamageTarget 데미지를 받은 액터
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamageDealt, float, DamageAmount, const FHSDamageInfo&, DamageInfo, AActor*, DamageTarget);

/**
 * @brief 전투 체력 변경 시 호출되는 델리게이트
 * @param NewHealth 새로운 체력 값
 * @param MaxHealth 최대 체력 값
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatHealthChanged, float, NewHealth, float, MaxHealth);

/**
 * @brief 캐릭터 사망 시 호출되는 델리게이트
 * @param DeadCharacter 사망한 캐릭터
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDied, AActor*, DeadCharacter);

/**
 * @brief 치명타 발생 시 호출되는 델리게이트
 * @param Target 치명타를 받은 대상
 * @param CriticalDamage 치명타 데미지 양
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCriticalHit, AActor*, Target, float, CriticalDamage);

/**
 * @brief 캐릭터의 전투 시스템을 관리하는 컴포넌트
 * @details 데미지 처리, 체력 관리, 상태 효과, 방어력 등 전투 관련 모든 기능을 제공
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * @brief 기본 생성자
     * @details 전투 컴포넌트의 기본 설정을 초기화
     */
    UHSCombatComponent();

    /**
     * @brief 매 프레임마다 호출되는 틱 함수
     * @param DeltaTime 이전 프레임과의 시간 차이
     * @param TickType 틱 유형
     * @param ThisTickFunction 틱 함수 구조체
     */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * @brief 네트워크 복제 속성을 설정
     * @param OutLifetimeProps 복제할 속성 목록
     */
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * @brief 데미지를 적용하고 결과를 반환
     * @param DamageInfo 데미지 정보 구조체
     * @param DamageInstigator 데미지를 가한 액터
     * @return 데미지 적용 결과
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    FHSDamageResult ApplyDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    /**
     * @brief 방어력을 고려한 최종 데미지를 계산
     * @param DamageInfo 데미지 정보 구조체
     * @param DamageInstigator 데미지를 가한 액터 (선택적)
     * @return 계산된 최종 데미지 값
     */
    UFUNCTION(BlueprintPure, Category = "Combat")
    float CalculateFinalDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator = nullptr) const;

    /**
     * @brief 힐링을 적용하여 체력을 회복
     * @param HealAmount 회복할 체력 양
     * @param HealInstigator 힐링을 적용한 액터 (선택적)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyHealing(float HealAmount, AActor* HealInstigator = nullptr);

    /**
     * @brief 상태 효과를 적용
     * @param StatusEffect 적용할 상태 효과 구조체
     * @param EffectInstigator 상태 효과를 적용한 액터 (선택적)
     * @return 상태 효과 적용 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool ApplyStatusEffect(const FHSStatusEffect& StatusEffect, AActor* EffectInstigator = nullptr);

    /**
     * @brief 특정 타입의 상태 효과를 제거
     * @param EffectType 제거할 상태 효과 타입
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RemoveStatusEffect(EHSStatusEffectType EffectType);

    /**
     * @brief 모든 상태 효과를 제거
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ClearAllStatusEffects();

    /**
     * @brief 현재 체력을 반환
     * @return 현재 체력 값
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    float GetCurrentHealth() const { return CurrentHealth; }

    /**
     * @brief 최대 체력을 반환
     * @return 최대 체력 값
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    float GetMaxHealth() const { return MaxHealth; }

    /**
     * @brief 체력 백분율을 반환
     * @return 체력 백분율 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    float GetHealthPercentage() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

    /**
     * @brief 최대 체력을 설정
     * @param NewMaxHealth 새로운 최대 체력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void SetMaxHealth(float NewMaxHealth);

    /**
     * @brief 현재 체력을 설정
     * @param NewHealth 새로운 현재 체력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void SetCurrentHealth(float NewHealth);

    /**
     * @brief 물리 방어력을 반환
     * @return 물리 방어력 값
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    float GetPhysicalArmor() const { return PhysicalArmor; }

    /**
     * @brief 마법 방어력을 반환
     * @return 마법 방어력 값
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    float GetMagicalArmor() const { return MagicalArmor; }

    /**
     * @brief 물리 방어력을 설정
     * @param NewArmor 새로운 물리 방어력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void SetPhysicalArmor(float NewArmor) { PhysicalArmor = FMath::Max(0.0f, NewArmor); }

    /**
     * @brief 마법 방어력을 설정
     * @param NewArmor 새로운 마법 방어력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void SetMagicalArmor(float NewArmor) { MagicalArmor = FMath::Max(0.0f, NewArmor); }

    /**
     * @brief 데미지 저항력을 반환
     * @return 데미지 저항력 구조체
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Resistance")
    FHSDamageResistance GetDamageResistance() const { return DamageResistance; }

    /**
     * @brief 데미지 저항력을 설정
     * @param NewResistance 새로운 데미지 저항력 구조체
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Resistance")
    void SetDamageResistance(const FHSDamageResistance& NewResistance) { DamageResistance = NewResistance; }

    /**
     * @brief 캐릭터가 살아있는지 확인
     * @return 생존 여부
     */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool IsAlive() const { return CurrentHealth > 0.0f; }

    /**
     * @brief 캐릭터가 죽었는지 확인
     * @return 사망 여부
     */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool IsDead() const { return CurrentHealth <= 0.0f; }

    /**
     * @brief 특정 상태 효과가 적용되어 있는지 확인
     * @param EffectType 확인할 상태 효과 타입
     * @return 상태 효과 적용 여부
     */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool HasStatusEffect(EHSStatusEffectType EffectType) const;

    /**
     * @brief 특정 상태 효과의 정보를 반환
     * @param EffectType 확인할 상태 효과 타입
     * @return 상태 효과 구조체
     */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    FHSStatusEffect GetStatusEffect(EHSStatusEffectType EffectType) const;
    
    /**
     * @brief 다음 공격에 대한 데미지 배율을 설정 (연계 공격용)
     * @param Multiplier 데미지 배율
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void SetNextAttackDamageMultiplier(float Multiplier);
    
    /**
     * @brief 원소 데미지를 추가 (시너지 효과용)
     * @param DamageType 원소 데미지 타입
     * @param Amount 추가할 데미지 양
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void AddElementalDamage(EHSDamageType DamageType, float Amount);
    
    /**
     * @brief 직접 데미지를 적용 (궁극기 콤보용)
     * @param DamageAmount 데미지 양
     * @param DamageType 데미지 타입
     * @param DamageInstigator 데미지를 가한 액터
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void TakeDamage(float DamageAmount, EHSDamageType DamageType, AActor* DamageInstigator);
    
    /**
     * @brief 데미지 공유를 활성화 (협동 방어용)
     * @param TeamMembers 데미지를 공유할 팀원들
     * @param ShareRatio 데미지 공유 비율
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void EnableDamageSharing(const TArray<class AHSCharacterBase*>& TeamMembers, float ShareRatio);
    
    /**
     * @brief 데미지 공유를 비활성화
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|SharedAbility")
    void DisableDamageSharing();

    /**
     * @brief 데미지 수신 시 호출되는 이벤트
     */
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnDamageReceived OnDamageReceived;

    /**
     * @brief 데미지 가함 시 호출되는 이벤트
     */
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnDamageDealt OnDamageDealt;

    /**
     * @brief 체력 변경 시 호출되는 이벤트
     */
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCombatHealthChanged OnHealthChanged;

    /**
     * @brief 캐릭터 사망 시 호출되는 이벤트
     */
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCharacterDied OnCharacterDied;

    /**
     * @brief 치명타 발생 시 호출되는 이벤트
     */
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCriticalHit OnCriticalHit;

protected:
    /**
     * @brief 컴포넌트 초기화 작업을 수행
     */
    virtual void BeginPlay() override;

    /**
     * @brief 최대 체력
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (ClampMin = "0.0"))
    float MaxHealth = 100.0f;

    /**
     * @brief 현재 체력 (네트워크 복제됨)
     */
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Health")
    float CurrentHealth = 100.0f;

    /**
     * @brief 체력 자동 회복 활성화 여부
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
    bool bEnableHealthRegeneration = false;

    /**
     * @brief 체력 자동 회복 속도 (초당 회복량)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (EditCondition = "bEnableHealthRegeneration", ClampMin = "0.0"))
    float HealthRegenerationRate = 1.0f;

    /**
     * @brief 체력 회복 시작 지연 시간 (데미지 후)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (EditCondition = "bEnableHealthRegeneration", ClampMin = "0.0"))
    float HealthRegenerationDelay = 5.0f;

    /**
     * @brief 물리 방어력
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
    float PhysicalArmor = 0.0f;

    /**
     * @brief 마법 방어력
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
    float MagicalArmor = 0.0f;

    /**
     * @brief 데미지 타입별 저항력
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Resistance")
    FHSDamageResistance DamageResistance;

    /**
     * @brief 현재 적용된 상태 효과 목록 (네트워크 복제됨)
     */
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Status Effects")
    TArray<FHSStatusEffect> ActiveStatusEffects;

    /**
     * @brief 무적 상태 여부
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Invincibility")
    bool bInvincible = false;

    /**
     * @brief 피격 후 무적 지속 시간
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Invincibility")
    float InvincibilityDuration = 0.5f;

    /**
     * @brief 체력 회복 타이머 핸들
     */
    FTimerHandle HealthRegenerationTimerHandle;

    /**
     * @brief 무적 상태 타이머 핸들
     */
    FTimerHandle InvincibilityTimerHandle;

    /**
     * @brief 상태 효과별 타이머 핸들 맵
     */
    TMap<EHSStatusEffectType, FTimerHandle> StatusEffectTimerHandles;

    /**
     * @brief 오너 캐릭터 캐시
     */
    UPROPERTY()
    AHSCharacterBase* OwnerCharacter;

    /**
     * @brief 피격 반응 컴포넌트 캐시
     */
    UPROPERTY()
    UHSHitReactionComponent* HitReactionComponent;
    
    /**
     * @brief 다음 공격의 데미지 배율
     */
    float NextAttackDamageMultiplier = 1.0f;
    
    /**
     * @brief 추가 원소 데미지 맵
     */
    TMap<EHSDamageType, float> AdditionalElementalDamage;
    struct FElementalDamageStackInfo
    {
        EHSDamageType DamageType = EHSDamageType::None;
        float Amount = 0.0f;
    };
    TMap<int32, FElementalDamageStackInfo> ActiveElementalDamageStacks;
    int32 ElementalDamageStackIdCounter = 0;
    
    /**
     * @brief 데미지 공유 활성화 여부
     */
    bool bDamageSharingEnabled = false;

    /**
     * @brief 데미지를 공유할 팀원들
     */
    TArray<TWeakObjectPtr<AHSCharacterBase>> DamageSharingTeamMembers;

    /**
     * @brief 데미지 공유 비율
     */
    float DamageShareRatio = 0.0f;

private:
    /**
     * @brief 방어력을 통한 데미지 감소를 계산
     * @param Damage 원본 데미지
     * @param Armor 방어력
     * @param ArmorPenetration 방어력 관통력 (선택적)
     * @return 감소된 데미지 값
     */
    float CalculateArmorReduction(float Damage, float Armor, float ArmorPenetration = 0.0f) const;

    /**
     * @brief 매 프레임마다 상태 효과들을 처리
     * @param DeltaTime 이전 프레임과의 시간 차이
     */
    void ProcessStatusEffects(float DeltaTime);

    /**
     * @brief 상태 효과로 인한 데미지를 적용
     * @param Effect 적용할 상태 효과
     */
    void ApplyStatusEffectDamage(const FHSStatusEffect& Effect);

    /**
     * @brief 상태 효과 만료 시 호출되는 함수
     * @param EffectType 만료된 상태 효과 타입
     */
    void OnStatusEffectExpired(EHSStatusEffectType EffectType);

    /**
     * @brief 체력 자동 회복을 시작
     */
    UFUNCTION()
    void StartHealthRegeneration();

    /**
     * @brief 체력 자동 회복을 중지
     */
    UFUNCTION()
    void StopHealthRegeneration();

    /**
     * @brief 무적 상태를 설정
     * @param bNewInvincible 새로운 무적 상태
     */
    UFUNCTION()
    void SetInvincible(bool bNewInvincible);

    /**
     * @brief 무적 상태를 종료
     */
    UFUNCTION()
    void EndInvincibility();

    /**
     * @brief 사망 처리를 수행
     * @param KillerActor 킬러 액터
     */
    void HandleDeath(AActor* KillerActor);

    /**
     * @brief 서버에서 데미지를 적용하는 RPC 함수
     * @param DamageInfo 데미지 정보
     * @param DamageInstigator 데미지를 가한 액터
     */
    UFUNCTION(Server, Reliable)
    void ServerApplyDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    /**
     * @brief 모든 클라이언트에 데미지 수신을 알리는 RPC 함수
     * @param DamageAmount 데미지 양
     * @param DamageInfo 데미지 정보
     * @param DamageInstigator 데미지를 가한 액터
     */
    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnDamageReceived(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    /**
     * @brief 모든 클라이언트에 캐릭터 사망을 알리는 RPC 함수
     * @param DeadCharacter 사망한 캐릭터
     */
    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnCharacterDied(AActor* DeadCharacter);

    UFUNCTION()
    void HandleElementalDamageExpired(int32 StackId);
};
