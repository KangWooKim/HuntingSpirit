// HSBossAbilitySystem.h
// HuntingSpirit Game - 보스 능력 시스템
// 보스 몬스터의 특수 능력을 관리하고 실행하는 시스템

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSBossBase.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Templates/SharedPointer.h"
#include "Containers/Queue.h"
#include "Async/AsyncWork.h"
#include "Math/UnrealMathSSE.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundCue.h"
#include "HSBossAbilitySystem.generated.h"

// 전방 선언
class AHSBossBase;
class UWorld;
class AActor;
class APlayerController;

// 능력 우선순위 레벨
UENUM(BlueprintType)
enum class EHSAbilityPriority : uint8
{
    VeryLow     UMETA(DisplayName = "Very Low"),
    Low         UMETA(DisplayName = "Low"),
    Normal      UMETA(DisplayName = "Normal"),
    High        UMETA(DisplayName = "High"),
    VeryHigh    UMETA(DisplayName = "Very High"),
    Critical    UMETA(DisplayName = "Critical")
};

// 능력 타겟 타입
UENUM(BlueprintType)
enum class EHSAbilityTargetType : uint8
{
    None        UMETA(DisplayName = "No Target"),
    Self        UMETA(DisplayName = "Self"),
    SingleEnemy UMETA(DisplayName = "Single Enemy"),
    MultipleEnemies UMETA(DisplayName = "Multiple Enemies"),
    AreaOfEffect UMETA(DisplayName = "Area of Effect"),
    AllEnemies  UMETA(DisplayName = "All Enemies")
};

// 능력 상태
UENUM(BlueprintType)
enum class EHSAbilityState : uint8
{
    Ready       UMETA(DisplayName = "Ready"),
    Cooldown    UMETA(DisplayName = "On Cooldown"),
    Executing   UMETA(DisplayName = "Executing"),
    Interrupted UMETA(DisplayName = "Interrupted"),
    Disabled    UMETA(DisplayName = "Disabled")
};

// 능력 효과 타입
UENUM(BlueprintType)
enum class EHSAbilityEffectType : uint8
{
    Damage      UMETA(DisplayName = "Damage"),
    Heal        UMETA(DisplayName = "Heal"),
    Buff        UMETA(DisplayName = "Buff"),
    Debuff      UMETA(DisplayName = "Debuff"),
    Summon      UMETA(DisplayName = "Summon"),
    Environmental UMETA(DisplayName = "Environmental"),
    Special     UMETA(DisplayName = "Special")
};

// 보스 능력 데이터 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSBossAbility
{
    GENERATED_BODY()

    // 기본 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Info")
    FName AbilityID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Info")
    FText AbilityName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Info", meta = (MultiLine = "true"))
    FText Description;

    // 실행 조건
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution")
    EHSBossPhase RequiredPhase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution")
    float Cooldown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution")
    float CastTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution")
    float ManaCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution")
    EHSAbilityPriority Priority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execution")
    bool bCanBeInterrupted;

    // 타겟팅
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    EHSAbilityTargetType TargetType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float Range;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float AreaRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    int32 MaxTargets;

    // 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    EHSAbilityEffectType EffectType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float Damage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float Duration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float DamageOverTime;

    // 애니메이션 및 시각/청각 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    UAnimMontage* AnimationMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    UNiagaraSystem* VFXTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    USoundCue* SoundEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    FLinearColor EffectColor;

    // 연계 및 조건
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
    TArray<FName> RequiredAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
    TArray<FName> IncompatibleAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
    float HealthThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
    int32 MinPlayerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
    bool bOnlyInEnrageMode;

    // 내부 런타임 데이터 (복제되지 않음)
    float LastUsedTime;
    float RemainingCooldown;
    EHSAbilityState CurrentState;
    TArray<TObjectPtr<AActor>> CurrentTargets;
    int32 UsageCount;
    float TotalDamageDealt;

    FHSBossAbility()
    {
        AbilityID = NAME_None;
        AbilityName = FText::FromString(TEXT("Unknown Ability"));
        Description = FText::FromString(TEXT("No description available"));
        RequiredPhase = EHSBossPhase::Phase1;
        Cooldown = 10.0f;
        CastTime = 2.0f;
        ManaCost = 50.0f;
        Priority = EHSAbilityPriority::Normal;
        bCanBeInterrupted = true;
        TargetType = EHSAbilityTargetType::SingleEnemy;
        Range = 1000.0f;
        AreaRadius = 300.0f;
        MaxTargets = 1;
        EffectType = EHSAbilityEffectType::Damage;
        Damage = 100.0f;
        Duration = 0.0f;
        DamageOverTime = 0.0f;
        AnimationMontage = nullptr;
        VFXTemplate = nullptr;
        SoundEffect = nullptr;
        EffectColor = FLinearColor::Red;
        HealthThreshold = 0.0f;
        MinPlayerCount = 1;
        bOnlyInEnrageMode = false;
        
        // 런타임 초기화
        LastUsedTime = 0.0f;
        RemainingCooldown = 0.0f;
        CurrentState = EHSAbilityState::Ready;
        UsageCount = 0;
        TotalDamageDealt = 0.0f;
    }
};

// 능력 실행 컨텍스트
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSAbilityExecutionContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AHSBossBase> Caster;

    UPROPERTY(BlueprintReadWrite)
    TArray<TObjectPtr<AActor>> Targets;

    UPROPERTY(BlueprintReadWrite)
    FVector TargetLocation;

    UPROPERTY(BlueprintReadWrite)
    float DamageMultiplier;

    UPROPERTY(BlueprintReadWrite)
    float CooldownReduction;

    UPROPERTY(BlueprintReadWrite)
    bool bIgnoreRange;

    FHSAbilityExecutionContext()
    {
        DamageMultiplier = 1.0f;
        CooldownReduction = 0.0f;
        bIgnoreRange = false;
        TargetLocation = FVector::ZeroVector;
    }
};

// 성능 추적을 위한 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSAbilityPerformanceData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName AbilityID;

    UPROPERTY(BlueprintReadOnly)
    int32 ExecutionCount;

    UPROPERTY(BlueprintReadOnly)
    float TotalExecutionTime;

    UPROPERTY(BlueprintReadOnly)
    float AverageExecutionTime;

    UPROPERTY(BlueprintReadOnly)
    float MaxExecutionTime;

    UPROPERTY(BlueprintReadOnly)
    float TotalDamageOutput;

    FHSAbilityPerformanceData()
    {
        AbilityID = NAME_None;
        ExecutionCount = 0;
        TotalExecutionTime = 0.0f;
        AverageExecutionTime = 0.0f;
        MaxExecutionTime = 0.0f;
        TotalDamageOutput = 0.0f;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityExecutedDelegate, const FHSBossAbility&, Ability, const FHSAbilityExecutionContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityStateChangedDelegate, FName, AbilityID, EHSAbilityState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityCooldownExpiredDelegate, FName, AbilityID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAbilityInterruptedDelegate, FName, AbilityID, AActor*, Interrupter, float, InterruptTime);

/**
 * HSBossAbilitySystem
 * 보스 몬스터의 특수 능력을 관리하는 고성능 컴포넌트
 * 
 * 주요 기능:
 * - 능력 관리 및 실행
 * - 쿨다운 및 상태 관리
 * - 타겟팅 시스템
 * - 연계 능력 지원
 * - 성능 최적화 (메모리 풀링, 캐싱, SIMD)
 * - 네트워킹 지원
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSBossAbilitySystem : public UActorComponent
{
    GENERATED_BODY()

public:
    // 생성자
    UHSBossAbilitySystem();

    // UActorComponent 오버라이드
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 능력 관리
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Management")
    bool AddAbility(const FHSBossAbility& NewAbility);

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Management")
    bool RemoveAbility(FName AbilityID);

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Management")
    void ClearAllAbilities();

    UFUNCTION(BlueprintPure, Category = "Boss Ability|Management")
    bool HasAbility(FName AbilityID) const;

    UFUNCTION(BlueprintPure, Category = "Boss Ability|Management")
    FHSBossAbility GetAbility(FName AbilityID) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Management")
    bool ModifyAbility(FName AbilityID, const FHSBossAbility& ModifiedAbility);

    // 능력 검색 및 필터링 (최적화된 캐싱 시스템)
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Query")
    TArray<FHSBossAbility> GetAvailableAbilities(EHSBossPhase CurrentPhase) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Query")
    TArray<FHSBossAbility> GetAbilitiesByPriority(EHSAbilityPriority MinPriority) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Query")
    TArray<FHSBossAbility> GetAbilitiesByTargetType(EHSAbilityTargetType TargetType) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Query")
    FHSBossAbility GetBestAbilityForSituation(const FHSAbilityExecutionContext& Context) const;

    // 능력 실행
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Execution")
    bool ExecuteAbility(FName AbilityID, const FHSAbilityExecutionContext& Context);

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Execution")
    bool InterruptAbility(FName AbilityID, AActor* Interrupter = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Execution")
    void InterruptAllAbilities();

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Execution")
    bool QueueAbility(FName AbilityID, const FHSAbilityExecutionContext& Context, float DelayTime = 0.0f);

    // 상태 확인
    UFUNCTION(BlueprintPure, Category = "Boss Ability|State")
    bool CanUseAbility(FName AbilityID, EHSBossPhase CurrentPhase) const;

    UFUNCTION(BlueprintPure, Category = "Boss Ability|State")
    EHSAbilityState GetAbilityState(FName AbilityID) const;

    UFUNCTION(BlueprintPure, Category = "Boss Ability|State")
    float GetAbilityCooldown(FName AbilityID) const;

    UFUNCTION(BlueprintPure, Category = "Boss Ability|State")
    float GetAbilityRemainingCooldown(FName AbilityID) const;

    UFUNCTION(BlueprintPure, Category = "Boss Ability|State")
    bool IsExecutingAnyAbility() const;

    UFUNCTION(BlueprintPure, Category = "Boss Ability|State")
    TArray<FName> GetExecutingAbilities() const;

    // 쿨다운 관리
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Cooldown")
    void ResetAbilityCooldown(FName AbilityID);

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Cooldown")
    void ResetAllCooldowns();

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Cooldown")
    void ModifyCooldown(FName AbilityID, float CooldownReduction);

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Cooldown")
    void SetGlobalCooldownMultiplier(float Multiplier);

    // 타겟팅 시스템
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Targeting")
    TArray<AActor*> FindTargetsForAbility(const FHSBossAbility& Ability, const FVector& TargetLocation) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Targeting")
    bool IsValidTarget(const FHSBossAbility& Ability, AActor* Target) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Targeting")
    FVector GetOptimalTargetLocation(const FHSBossAbility& Ability, const TArray<AActor*>& PotentialTargets) const;

    // 성능 분석
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Performance")
    FHSAbilityPerformanceData GetAbilityPerformanceData(FName AbilityID) const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Performance")
    TArray<FHSAbilityPerformanceData> GetAllPerformanceData() const;

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Performance")
    void ResetPerformanceData();

    // 디버깅 및 개발자 도구
    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Debug", CallInEditor)
    void DebugPrintAllAbilities();

    UFUNCTION(BlueprintCallable, Category = "Boss Ability|Debug")
    void DrawDebugInformation();

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Boss Ability Events")
    FOnAbilityExecutedDelegate OnAbilityExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Boss Ability Events")
    FOnAbilityStateChangedDelegate OnAbilityStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Boss Ability Events")
    FOnAbilityCooldownExpiredDelegate OnAbilityCooldownExpired;

    UPROPERTY(BlueprintAssignable, Category = "Boss Ability Events")
    FOnAbilityInterruptedDelegate OnAbilityInterrupted;

protected:
    // 설정 가능한 매개변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    TArray<FHSBossAbility> DefaultAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    int32 MaxConcurrentAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    float GlobalCooldownMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    bool bEnablePerformanceTracking;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    bool bUseAdvancedTargeting;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    float TargetingUpdateFrequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Ability Settings")
    bool bDebugMode;

private:
    // 핵심 데이터 구조 (최적화됨)
    UPROPERTY()
    TMap<FName, FHSBossAbility> AbilitiesMap;

    // 캐싱된 데이터 (성능 최적화)
    mutable TArray<FHSBossAbility> CachedAvailableAbilities;
    mutable EHSBossPhase LastCachedPhase;
    mutable float LastCacheTime;
    static constexpr float CACHE_VALIDITY_TIME = 0.1f; // 100ms

    // 실행 중인 능력 추적
    TSet<FName> ExecutingAbilities;
    TQueue<TPair<FName, FHSAbilityExecutionContext>> QueuedAbilities;

    // 성능 추적
    TMap<FName, FHSAbilityPerformanceData> PerformanceDataMap;

    // 타이머 관리 (메모리 풀링)
    TMap<FName, FTimerHandle> CooldownTimers;
    TMap<FName, FTimerHandle> ExecutionTimers;

    // 오브젝트 풀링을 위한 캐시
    TArray<UNiagaraComponent*> VFXPool;
    TArray<UAudioComponent*> AudioPool;

    // 보스 참조 (메모리 안전성 확보)
    TObjectPtr<AHSBossBase> OwnerBoss;

    // 내부 헬퍼 함수들
    void InitializeAbilitySystem();
    void LoadDefaultAbilities();
    bool ValidateAbility(const FHSBossAbility& Ability) const;
    void UpdateCooldowns(float DeltaTime);
    void ProcessQueuedAbilities();
    
    // 능력 실행 내부 함수들
    bool InternalExecuteAbility(const FHSBossAbility& Ability, const FHSAbilityExecutionContext& Context);
    void PlayAbilityEffects(const FHSBossAbility& Ability, const FHSAbilityExecutionContext& Context);
    void ApplyAbilityEffects(const FHSBossAbility& Ability, const FHSAbilityExecutionContext& Context);
    
    // 타겟팅 내부 함수들
    TArray<AActor*> FindSingleTarget(const FHSBossAbility& Ability, const FVector& TargetLocation) const;
    TArray<AActor*> FindMultipleTargets(const FHSBossAbility& Ability, const FVector& TargetLocation) const;
    TArray<AActor*> FindAreaTargets(const FHSBossAbility& Ability, const FVector& TargetLocation) const;
    
    // 성능 최적화 함수들
    void OptimizeAbilityCache();
    void UpdatePerformanceData(FName AbilityID, float ExecutionTime, float DamageDealt);
    
    // 리소스 풀 관리
    UNiagaraComponent* GetPooledVFXComponent();
    void ReturnVFXComponentToPool(UNiagaraComponent* Component);
    UAudioComponent* GetPooledAudioComponent();
    void ReturnAudioComponentToPool(UAudioComponent* Component);
    
    // 메모리 관리 및 정리
    void CleanupExpiredReferences();
    void OptimizeMemoryUsage();
    
    // 콜백 함수들
    UFUNCTION()
    void OnCooldownExpired(FName AbilityID);
    
    UFUNCTION()
    void OnAbilityExecutionComplete(FName AbilityID);
    
    // 자가검증 함수들
    bool ValidateExecutionContext(const FHSAbilityExecutionContext& Context) const;
    bool ValidateTargets(const TArray<AActor*>& Targets) const;
    bool CheckGameStateConsistency() const;
    void LogErrorWithContext(const FString& ErrorMessage, FName AbilityID = NAME_None) const;
};
