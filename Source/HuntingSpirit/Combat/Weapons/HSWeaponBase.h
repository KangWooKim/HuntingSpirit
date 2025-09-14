// HuntingSpirit Game - Weapon Base Header
// 모든 무기의 기본 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HuntingSpirit/Combat/Damage/HSDamageType.h"
#include "HuntingSpirit/Optimization/ObjectPool/HSObjectPool.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HSWeaponBase.generated.h"

class AHSCharacterBase;
class UAnimMontage;
class USoundBase;
class UParticleSystem;

// 무기 타입 열거형
UENUM(BlueprintType)
enum class EHSWeaponType : uint8
{
    None            UMETA(DisplayName = "None"),
    Sword           UMETA(DisplayName = "Sword"),           // 검 (전사용)
    GreatSword      UMETA(DisplayName = "Great Sword"),    // 대검 (전사용)
    Dagger          UMETA(DisplayName = "Dagger"),         // 단검 (시프용)
    DualBlades      UMETA(DisplayName = "Dual Blades"),    // 쌍검 (시프용)
    Staff           UMETA(DisplayName = "Staff"),          // 지팡이 (마법사용)
    Wand            UMETA(DisplayName = "Wand"),           // 완드 (마법사용)
    Bow             UMETA(DisplayName = "Bow"),            // 활
    Crossbow        UMETA(DisplayName = "Crossbow")        // 석궁
};

// 무기 등급 열거형
UENUM(BlueprintType)
enum class EHSWeaponRarity : uint8
{
    Common          UMETA(DisplayName = "Common"),         // 일반
    Uncommon        UMETA(DisplayName = "Uncommon"),       // 고급
    Rare            UMETA(DisplayName = "Rare"),           // 희귀
    Epic            UMETA(DisplayName = "Epic"),           // 영웅
    Legendary       UMETA(DisplayName = "Legendary"),      // 전설
    Mythic          UMETA(DisplayName = "Mythic")          // 신화
};

// 무기 상태 열거형
UENUM(BlueprintType)
enum class EHSWeaponState : uint8
{
    Dropped         UMETA(DisplayName = "Dropped"),        // 드롭된 상태
    Equipped        UMETA(DisplayName = "Equipped"),       // 장착된 상태
    Stored          UMETA(DisplayName = "Stored"),         // 보관된 상태
    Broken          UMETA(DisplayName = "Broken")          // 파괴된 상태
};

// 무기 공격 패턴 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSWeaponAttackPattern
{
    GENERATED_BODY()

    // 공격 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern")
    FString AttackName = TEXT("Basic Attack");

    // 데미지 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern")
    FHSDamageInfo DamageInfo;

    // 공격 범위 (단위: 언리얼 유닛)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern", meta = (ClampMin = "0.0"))
    float AttackRange = 100.0f;

    // 공격 각도 (도 단위, 정면 기준)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern", meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float AttackAngle = 90.0f;

    // 애니메이션 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern")
    UAnimMontage* AttackMontage = nullptr;

    // 공격 쿨다운 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern", meta = (ClampMin = "0.0"))
    float Cooldown = 1.0f;

    // 스태미너 소모량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern", meta = (ClampMin = "0.0"))
    float StaminaCost = 10.0f;

    // 공격 효과음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern")
    USoundBase* AttackSound = nullptr;

    // 공격 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Pattern")
    UParticleSystem* AttackEffect = nullptr;

    // 생성자
    FHSWeaponAttackPattern()
    {
        AttackName = TEXT("Basic Attack");
        AttackRange = 100.0f;
        AttackAngle = 90.0f;
        AttackMontage = nullptr;
        Cooldown = 1.0f;
        StaminaCost = 10.0f;
        AttackSound = nullptr;
        AttackEffect = nullptr;
    }
};

// 무기 델리게이트 정의
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponEquipped, AHSWeaponBase*, Weapon, AHSCharacterBase*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponUnequipped, AHSWeaponBase*, Weapon, AHSCharacterBase*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponAttack, AHSWeaponBase*, Weapon, const FHSWeaponAttackPattern&, AttackPattern, TArray<AActor*>, HitTargets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponDurabilityChanged, float, CurrentDurability, float, MaxDurability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponBroken, AHSWeaponBase*, Weapon);

UCLASS()
class HUNTINGSPIRIT_API AHSWeaponBase : public AActor, public IHSPoolableObject
{
    GENERATED_BODY()
    
public:    
    // 생성자
    AHSWeaponBase();

    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;

    // 풀링 인터페이스 구현
    virtual void OnActivated_Implementation() override;
    virtual void OnDeactivated_Implementation() override;
    virtual void OnCreated_Implementation() override;

    // 무기 장착/해제 함수
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual bool EquipWeapon(AHSCharacterBase* Character);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual bool UnequipWeapon();

    // 공격 수행 함수
    UFUNCTION(BlueprintCallable, Category = "Weapon|Attack")
    virtual bool PerformAttack(int32 AttackPatternIndex = 0);

    // 공격 패턴 관련 함수
    UFUNCTION(BlueprintPure, Category = "Weapon|Attack")
    int32 GetAttackPatternCount() const { return AttackPatterns.Num(); }

    UFUNCTION(BlueprintPure, Category = "Weapon|Attack")
    FHSWeaponAttackPattern GetAttackPattern(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Weapon|Attack")
    void AddAttackPattern(const FHSWeaponAttackPattern& Pattern);

    // 무기 정보 함수들
    UFUNCTION(BlueprintPure, Category = "Weapon|Info")
    EHSWeaponType GetWeaponType() const { return WeaponType; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Info")
    EHSWeaponRarity GetWeaponRarity() const { return WeaponRarity; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Info")
    EHSWeaponState GetWeaponState() const { return WeaponState; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Info")
    FString GetWeaponName() const { return WeaponName; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Info")
    FString GetWeaponDescription() const { return WeaponDescription; }

    // 내구도 관련 함수
    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    float GetCurrentDurability() const { return CurrentDurability; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    float GetMaxDurability() const { return MaxDurability; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    float GetDurabilityPercentage() const { return MaxDurability > 0.0f ? CurrentDurability / MaxDurability : 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
    void SetDurability(float NewDurability);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
    void RepairWeapon(float RepairAmount = -1.0f);

    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    bool IsBroken() const { return WeaponState == EHSWeaponState::Broken; }

    // 무기 소유자 함수
    UFUNCTION(BlueprintPure, Category = "Weapon")
    AHSCharacterBase* GetOwningCharacter() const { return OwningCharacter; }

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Weapon Events")
    FOnWeaponEquipped OnWeaponEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Weapon Events")
    FOnWeaponUnequipped OnWeaponUnequipped;

    UPROPERTY(BlueprintAssignable, Category = "Weapon Events")
    FOnWeaponAttack OnWeaponAttack;

    UPROPERTY(BlueprintAssignable, Category = "Weapon Events")
    FOnWeaponDurabilityChanged OnWeaponDurabilityChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon Events")
    FOnWeaponBroken OnWeaponBroken;

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 무기 메시 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* WeaponMesh;

    // 상호작용 범위 (획득용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionSphere;

    // 무기 기본 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Info")
    EHSWeaponType WeaponType = EHSWeaponType::Sword;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Info")
    EHSWeaponRarity WeaponRarity = EHSWeaponRarity::Common;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Info")
    EHSWeaponState WeaponState = EHSWeaponState::Dropped;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Info")
    FString WeaponName = TEXT("Basic Weapon");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Info", meta = (MultiLine = "true"))
    FString WeaponDescription = TEXT("A basic weapon.");

    // 공격 패턴 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Attack Patterns")
    TArray<FHSWeaponAttackPattern> AttackPatterns;

    // 내구도 시스템
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Durability", meta = (ClampMin = "0.0"))
    float MaxDurability = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Durability")
    float CurrentDurability = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Durability")
    bool bHasDurability = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Durability", meta = (EditCondition = "bHasDurability", ClampMin = "0.0"))
    float DurabilityLossPerAttack = 1.0f;

    // 무기 소유자
    UPROPERTY()
    AHSCharacterBase* OwningCharacter;

private:
    // 공격 쿨다운 타이머 핸들들
    TArray<FTimerHandle> AttackCooldownTimers;

    // 내부 함수들
    void InitializeWeapon();
    void SetWeaponState(EHSWeaponState NewState);
    void ReduceDurability(float Amount);
    
    // 공격 실행 함수들
    TArray<AActor*> PerformRangeAttack(const FHSWeaponAttackPattern& Pattern);
    void ApplyDamageToTargets(const TArray<AActor*>& Targets, const FHSWeaponAttackPattern& Pattern);
    
    // 공격 가능 여부 확인
    bool CanPerformAttack(int32 PatternIndex) const;
    
    // 공격 쿨다운 관리
    void StartAttackCooldown(int32 PatternIndex);
    void OnAttackCooldownExpired(int32 PatternIndex);

    // 상호작용 함수들
    UFUNCTION()
    void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
    
    // 유틸리티 함수들
    bool IsValidTarget(AActor* Target) const;
    FName GetWeaponSocketName() const;
};
