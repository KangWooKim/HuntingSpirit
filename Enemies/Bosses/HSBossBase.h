// HuntingSpirit Game - Boss Base Header
// 모든 보스 몬스터의 기본 클래스
// 페이즈 시스템, 특수 공격 패턴, 협동 메커니즘 등 보스 전용 기능 구현

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "Engine/DataTable.h"
#include "HuntingSpirit/Items/HSItemBase.h" // AHSItemBase 완전한 타입 정의를 위해 추가
#include "HSBossBase.generated.h"

// 전방 선언
class UHSBossPhaseComponent;
class UHSBossAbilityComponent;
class UWidgetComponent;
class UHSBossHealthBarWidget;
class UBoxComponent;
class UParticleSystemComponent;
class USoundCue;

// 보스 페이즈 상태
UENUM(BlueprintType)
enum class EHSBossPhase : uint8
{
    Phase1      UMETA(DisplayName = "Phase 1"),
    Phase2      UMETA(DisplayName = "Phase 2"),
    Phase3      UMETA(DisplayName = "Phase 3"),
    Enraged     UMETA(DisplayName = "Enraged"),
    Final       UMETA(DisplayName = "Final Stand")
};

// 보스 공격 패턴 타입
UENUM(BlueprintType)
enum class EHSBossPatternType : uint8
{
    Melee       UMETA(DisplayName = "Melee Pattern"),
    Ranged      UMETA(DisplayName = "Ranged Pattern"),
    Area        UMETA(DisplayName = "Area Pattern"),
    Special     UMETA(DisplayName = "Special Pattern"),
    Ultimate    UMETA(DisplayName = "Ultimate Pattern")
};

// 보스 공격 패턴 구조체
USTRUCT(BlueprintType)
struct FHSBossAttackPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PatternName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHSBossPatternType PatternType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Damage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Cooldown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Range;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinimumPhase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresMultiplePlayers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class UAnimMontage* AnimationMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class UParticleSystem* VFXTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundCue* SoundEffect;

    FHSBossAttackPattern()
    {
        PatternName = TEXT("Default");
        PatternType = EHSBossPatternType::Melee;
        Damage = 100.0f;
        Cooldown = 5.0f;
        Range = 500.0f;
        ActivationTime = 2.0f;
        MinimumPhase = 1;
        bRequiresMultiplePlayers = false;
        AnimationMontage = nullptr;
        VFXTemplate = nullptr;
        SoundEffect = nullptr;
    }
};

// 보스 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseChanged, EHSBossPhase, OldPhase, EHSBossPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPatternStart, const FHSBossAttackPattern&, Pattern);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPatternEnd, const FHSBossAttackPattern&, Pattern);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossEnraged, float, EnrageDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossHealthChanged, float, CurrentHealth, float, MaxHealth);

UCLASS()
class HUNTINGSPIRIT_API AHSBossBase : public AHSEnemyBase
{
    GENERATED_BODY()

public:
    // 생성자
    AHSBossBase();

    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;

    // 페이즈 관련 함수
    UFUNCTION(BlueprintPure, Category = "Boss|Phase")
    EHSBossPhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
    virtual void SetBossPhase(EHSBossPhase NewPhase);

    UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
    virtual void CheckPhaseTransition();

    // 패턴 관련 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
    virtual void ExecuteAttackPattern(const FHSBossAttackPattern& Pattern);

    UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
    virtual FHSBossAttackPattern SelectNextPattern();

    UFUNCTION(BlueprintPure, Category = "Boss|Pattern")
    bool IsPatternActive() const { return bIsExecutingPattern; }

    UFUNCTION(BlueprintPure, Category = "Boss|Pattern")
    const FHSBossAttackPattern& GetCurrentPattern() const { return CurrentPattern; }

    UFUNCTION(BlueprintPure, Category = "Boss|Pattern")
    const TArray<FHSBossAttackPattern>& GetAttackPatterns() const { return AttackPatterns; }

    // 협동 메커니즘 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Coop")
    virtual void OnMultiplePlayersDetected(const TArray<AActor*>& Players);

    UFUNCTION(BlueprintCallable, Category = "Boss|Coop")
    virtual void TriggerCoopMechanic();

    UFUNCTION(BlueprintPure, Category = "Boss|Coop")
    int32 GetActivePlayerCount() const;

    // 특수 능력 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Ability")
    virtual void ActivateSpecialAbility(FName AbilityName);

    UFUNCTION(BlueprintCallable, Category = "Boss|Ability")
    virtual void DeactivateSpecialAbility(FName AbilityName);

    // 환경 상호작용 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Environment")
    virtual void TriggerEnvironmentalHazard();

    UFUNCTION(BlueprintCallable, Category = "Boss|Environment")
    virtual void DestroyEnvironmentalObject(AActor* Object);

    // 분노 모드 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Enrage")
    virtual void EnterEnrageMode(float Duration = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "Boss|Enrage")
    virtual void ExitEnrageMode();

    UFUNCTION(BlueprintPure, Category = "Boss|Enrage")
    bool IsEnraged() const { return bIsEnraged; }

    // 체력 관련 오버라이드
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void SetHealth(float NewHealth) override;
    
    // 커스텀 데미지 처리
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    virtual void TakeDamageCustom(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);
    
    // 현재 체력 반환 (일관성을 위한 래퍼 함수)
    // 부모 클래스의 GetHealth()를 래핑하는 함수 - UFUNCTION 제거하여 중복 선언 방지
    float GetCurrentHealth() const { return GetHealth(); }
    
    // 최대 체력 반환 (일관성을 위한 래퍼 함수)
    // 부모 클래스에서 이미 UFUNCTION으로 선언되어 있으므로 UFUNCTION 제거
    virtual float GetMaxHealth() const override { return MaxHealth; }

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Boss Events")
    FOnBossPhaseChanged OnBossPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Boss Events")
    FOnBossPatternStart OnBossPatternStart;

    UPROPERTY(BlueprintAssignable, Category = "Boss Events")
    FOnBossPatternEnd OnBossPatternEnd;

    UPROPERTY(BlueprintAssignable, Category = "Boss Events")
    FOnBossEnraged OnBossEnraged;

    UPROPERTY(BlueprintAssignable, Category = "Boss Events")
    FOnBossHealthChanged OnBossHealthChanged;

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 사망 처리 오버라이드
    virtual void Die() override;

    // 보스 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Info")
    FString BossTitle = TEXT("Unknown Boss");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Info", meta = (MultiLine = "true"))
    FString BossLore = TEXT("A powerful boss that threatens the world.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Info")
    bool bIsWorldBoss = false;

    // 페이즈 시스템
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
    EHSBossPhase CurrentPhase = EHSBossPhase::Phase1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phase")
    TMap<EHSBossPhase, float> PhaseHealthThresholds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phase")
    TMap<EHSBossPhase, float> PhaseDamageMultipliers;

    // 패턴 시스템
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
    TArray<FHSBossAttackPattern> AttackPatterns;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
    FHSBossAttackPattern CurrentPattern;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
    bool bIsExecutingPattern = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
    FTimerHandle PatternExecutionTimer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
    FTimerHandle PatternCooldownTimer;

    // 협동 메커니즘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Coop")
    int32 MinPlayersForCoopMechanic = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Coop")
    float CoopDamageReduction = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Coop")
    TArray<AActor*> EngagedPlayers;

    // 특수 능력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Ability")
    TMap<FName, bool> SpecialAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Ability")
    float AbilityCooldownMultiplier = 1.0f;

    // 환경 상호작용
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Environment")
    TArray<TSubclassOf<AActor>> EnvironmentalHazardClasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Environment")
    float EnvironmentalDamage = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Environment")
    float HazardSpawnRadius = 1000.0f;

    // 분노 모드
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Enrage")
    bool bIsEnraged = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Enrage")
    float EnrageDamageMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Enrage")
    float EnrageSpeedMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Enrage")
    float EnrageHealthThreshold = 0.2f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Enrage")
    FTimerHandle EnrageTimer;

    // UI 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    UWidgetComponent* BossHealthBarComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    UWidgetComponent* BossNameplateComponent;

    // 효과 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Effects")
    UParticleSystemComponent* PhaseTransitionEffect;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Effects")
    UParticleSystemComponent* EnrageEffect;

    // 충돌 영역 (대형 보스용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Collision")
    UBoxComponent* ExtendedHitbox;

    // 보스 전용 스탯
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
    float BaseMaxHealth = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
    float DamageResistance = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
    float CrowdControlResistance = 0.5f;

    // 보스 전용 AI 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
    float AggroRange = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
    bool bCanLoseAggro = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
    float ThreatMultiplier = 2.0f;

    // 보상 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Rewards")
    TArray<TSubclassOf<AHSItemBase>> GuaranteedDrops;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Rewards")
    float ExperienceReward = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Rewards")
    float CurrencyReward = 500.0f;

protected:
    // 내부 함수들
    virtual void InitializeBoss();
    virtual void SetupBossComponents();
    virtual void UpdateBossUI();
    
    // 패턴 실행 함수들
    UFUNCTION()
    virtual void OnPatternActivationComplete();
    
    UFUNCTION()
    virtual void OnPatternCooldownComplete();
    
    virtual void ExecuteMeleePattern(const FHSBossAttackPattern& Pattern);
    virtual void ExecuteRangedPattern(const FHSBossAttackPattern& Pattern);
    virtual void ExecuteAreaPattern(const FHSBossAttackPattern& Pattern);
    virtual void ExecuteSpecialPattern(const FHSBossAttackPattern& Pattern);
    virtual void ExecuteUltimatePattern(const FHSBossAttackPattern& Pattern);
    
    // 페이즈 전환 함수
    virtual void OnPhaseTransition(EHSBossPhase OldPhase, EHSBossPhase NewPhase);
    virtual void PlayPhaseTransitionEffects();
    
    // 협동 메커니즘 내부 함수
    virtual void UpdateEngagedPlayers();
    virtual float CalculateCoopDamageReduction() const;
    
    // 환경 상호작용 내부 함수
    virtual void SpawnEnvironmentalHazard(const FVector& Location);
    virtual TArray<FVector> GetHazardSpawnLocations() const;
    
    // 분노 모드 내부 함수
    UFUNCTION()
    virtual void OnEnrageExpired();
    
    virtual void ApplyEnrageEffects();
    virtual void RemoveEnrageEffects();
    
    // 보상 처리
    virtual void DistributeRewards();
    virtual void GrantExperienceToPlayers();
    virtual void SpawnLoot();
    
    // 헬퍼 함수들
    virtual TArray<AActor*> FindNearbyPlayers() const;
    virtual void SpawnMinions();

private:
    // 오브젝트 풀링을 위한 캐시
    UPROPERTY()
    TArray<AActor*> CachedEnvironmentalHazards;
    
    // 패턴 선택 가중치
    TMap<FName, float> PatternWeights;
    
    // 플레이어 위협 수준 추적
    TMap<AActor*, float> PlayerThreatLevels;
    
    // 내부 헬퍼 함수들
    void InitializePhaseThresholds();
    void InitializeAttackPatterns();
    void ValidatePatternRequirements(const FHSBossAttackPattern& Pattern);
    bool CanExecutePattern(const FHSBossAttackPattern& Pattern) const;
    void UpdatePatternWeights();
    float CalculateThreatLevel(AActor* Player) const;
    void BroadcastBossEvents();
};
