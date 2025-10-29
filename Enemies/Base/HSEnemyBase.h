/**
 * @file HSEnemyBase.h
 * @brief 모든 적 캐릭터의 기본 클래스 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HuntingSpirit/Combat/Damage/HSDamageType.h"
#include "HSEnemyBase.generated.h"

class UHSCombatComponent;
class UHSHitReactionComponent;
class AHSAIControllerBase;
class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UPawnSensingComponent;

// 적 타입 열거형
UENUM(BlueprintType)
enum class EHSEnemyType : uint8
{
    None            UMETA(DisplayName = "None"),
    Melee           UMETA(DisplayName = "Melee"),           // 근접 공격
    Ranged          UMETA(DisplayName = "Ranged"),          // 원거리 공격
    Magic           UMETA(DisplayName = "Magic"),           // 마법 공격
    Support         UMETA(DisplayName = "Support"),         // 지원
    Boss            UMETA(DisplayName = "Boss"),            // 보스
    Elite           UMETA(DisplayName = "Elite")            // 정예
};

// 적 등급 열거형
UENUM(BlueprintType)
enum class EHSEnemyRank : uint8
{
    Minion          UMETA(DisplayName = "Minion"),          // 미니언
    Normal          UMETA(DisplayName = "Normal"),          // 일반
    Elite           UMETA(DisplayName = "Elite"),           // 정예
    Champion        UMETA(DisplayName = "Champion"),        // 챔피언
    Boss            UMETA(DisplayName = "Boss"),            // 보스
    WorldBoss       UMETA(DisplayName = "World Boss")      // 월드 보스
};

// 적 AI 상태 열거형
UENUM(BlueprintType)
enum class EHSEnemyAIState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),            // 대기
    Patrol          UMETA(DisplayName = "Patrol"),          // 순찰
    Investigating   UMETA(DisplayName = "Investigating"),   // 조사
    Chasing         UMETA(DisplayName = "Chasing"),         // 추격
    Attacking       UMETA(DisplayName = "Attacking"),       // 공격
    Retreating      UMETA(DisplayName = "Retreating"),      // 후퇴
    Stunned         UMETA(DisplayName = "Stunned"),         // 스턴
    Dead            UMETA(DisplayName = "Dead")             // 사망
};

/**
 * @brief 적의 타겟 변경 시 호출되는 델리게이트
 * @param OldTarget 이전 타겟
 * @param NewTarget 새로운 타겟
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyTargetChanged, AActor*, OldTarget, AActor*, NewTarget);

/**
 * @brief 적의 AI 상태 변경 시 호출되는 델리게이트
 * @param NewState 새로운 AI 상태
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyAIStateChanged, EHSEnemyAIState, NewState);

/**
 * @brief 적 사망 시 호출되는 델리게이트
 * @param DeadEnemy 사망한 적
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeath, AHSEnemyBase*, DeadEnemy);

/**
 * @brief 적이 데미지를 가할 때 호출되는 델리게이트
 * @param DamageAmount 가한 데미지 양
 * @param Target 데미지를 받은 대상
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDamageDealt, float, DamageAmount, AActor*, Target);

/**
 * @brief 모든 적 캐릭터의 기본 클래스
 * @details AI 상태 관리, 탐지 시스템, 전투 행동 등 적 캐릭터의 핵심 기능을 제공합니다.
 *          HSCharacterBase를 상속받아 적 전용 시스템을 구현합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API AHSEnemyBase : public AHSCharacterBase
{
    GENERATED_BODY()

public:
    /**
     * @brief 기본 생성자
     * @details 적 캐릭터의 기본 컴포넌트와 설정을 초기화합니다
     */
    AHSEnemyBase();

    /**
     * @brief 매 프레임마다 호출되는 틱 함수
     * @param DeltaTime 이전 프레임과의 시간 차이
     */
    virtual void Tick(float DeltaTime) override;

    /**
     * @brief AI 컨트롤러에 의해 빙의될 때 호출되는 함수
     * @param NewController 새로운 AI 컨트롤러
     */
    virtual void PossessedBy(AController* NewController) override;

    // 적 정보 함수들
    UFUNCTION(BlueprintPure, Category = "Enemy|Info")
    EHSEnemyType GetEnemyType() const { return EnemyType; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Info")
    EHSEnemyRank GetEnemyRank() const { return EnemyRank; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Info")
    FString GetEnemyName() const { return EnemyName; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Info")
    float GetDetectionRange() const { return DetectionRange; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Info")
    float GetAttackRange() const { return AttackRange; }

    // AI 상태 관련 함수들
    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    EHSEnemyAIState GetAIState() const { return CurrentAIState; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void SetAIState(EHSEnemyAIState NewState);

    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    AActor* GetCurrentTarget() const { return CurrentTarget; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void SetCurrentTarget(AActor* NewTarget);

    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    UBehaviorTree* GetBehaviorTree() const { return BehaviorTree; }

    // 전투 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    virtual void StartCombat(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    virtual void EndCombat();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    virtual bool IsInCombat() const { return bInCombat; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    virtual void PerformAttack();

    // 탐지 관련 함수들
    UFUNCTION(BlueprintPure, Category = "Enemy|Detection")
    bool CanSeeTarget(AActor* Target) const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Detection")
    float GetDistanceToTarget(AActor* Target) const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Detection")
    AActor* FindNearestPlayer() const;

    // 스탯 설정 함수들
    UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
    void SetEnemyStats(float InitialHealth, float Damage, float MoveSpeed);

    UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
    void ScaleStatsForRank();

    // 체력 관련 함수들 (부모 클래스에서 상속)
    virtual float GetHealth() const override { return CurrentHealth; }

    virtual float GetMaxHealth() const override { return MaxHealth; }

    virtual void SetHealth(float NewHealth) override;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Health")
    virtual void SetMaxHealth(float NewMaxHealth);

    virtual float GetHealthPercent() const override;

    virtual bool IsDead() const override { return bIsDead; }

    // 데미지 처리
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // 사망 처리
    UFUNCTION(BlueprintCallable, Category = "Enemy|Health")
    virtual void Die();

    // 공격력 관련
    UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
    float GetBaseDamage() const { return BaseDamage; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
    void SetBaseDamage(float NewDamage) { BaseDamage = NewDamage; }

    // 적 등급 설정
    UFUNCTION(BlueprintCallable, Category = "Enemy|Info")
    void SetEnemyRank(EHSEnemyRank NewRank) { EnemyRank = NewRank; }

    // 적 이름 설정
    UFUNCTION(BlueprintCallable, Category = "Enemy|Info")
    void SetEnemyName(const FString& NewName) { EnemyName = NewName; }

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Enemy Events")
    FOnEnemyTargetChanged OnEnemyTargetChanged;

    UPROPERTY(BlueprintAssignable, Category = "Enemy Events")
    FOnEnemyAIStateChanged OnEnemyAIStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Enemy Events")
    FOnEnemyDeath OnEnemyDeath;

    UPROPERTY(BlueprintAssignable, Category = "Enemy Events")
    FOnEnemyDamageDealt OnEnemyDamageDealt;

protected:
    /**
     * @brief 게임 시작 시 적 캐릭터를 초기화
     * @details AI 컨트롤러 설정, 컴포넌트 초기화, 스탯 설정 등을 수행합니다
     */
    virtual void BeginPlay() override;

    // 적 기본 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Info")
    EHSEnemyType EnemyType = EHSEnemyType::Melee;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Info")
    EHSEnemyRank EnemyRank = EHSEnemyRank::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Info")
    FString EnemyName = TEXT("Basic Enemy");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Info", meta = (MultiLine = "true"))
    FString EnemyDescription = TEXT("A basic enemy.");

    // 감지 컴포넌트 (적 전용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPawnSensingComponent* PawnSensingComponent;

    // AI 관련 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    class UBehaviorTree* BehaviorTree;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
    EHSEnemyAIState CurrentAIState = EHSEnemyAIState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
    AActor* CurrentTarget = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
    FVector LastKnownPlayerLocation = FVector::ZeroVector;

    // 탐지 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Detection", meta = (ClampMin = "0.0"))
    float DetectionRange = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Detection", meta = (ClampMin = "0.0"))
    float LoseTargetRange = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Detection", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float SightAngle = 90.0f;

    // 전투 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
    float AttackCooldown = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    FHSDamageInfo AttackDamageInfo;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
    bool bInCombat = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
    FTimerHandle AttackCooldownTimer;

    // 순찰 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Patrol")
    bool bShouldPatrol = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
    float PatrolRadius = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
    float PatrolWaitTime = 3.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol")
    FVector SpawnLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol")
    FVector PatrolTarget = FVector::ZeroVector;

    // 체력 관련 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Health")
    float CurrentHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Health")
    bool bIsDead = false;

    // 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
    float BaseDamage = 10.0f;

    // 스탯 스케일링
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
    float HealthScalePerRank = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
    float DamageScalePerRank = 1.3f;

    // AI 컨트롤러 캐시
    UPROPERTY()
    AHSAIControllerBase* AIController;

    // 블랙보드 및 비헤이비어 트리 컴포넌트 캐시
    UPROPERTY()
    UBlackboardComponent* BlackboardComponent;

    UPROPERTY()
    UBehaviorTreeComponent* BehaviorTreeComponent;

protected:
    // 타이머 함수들 - 하위 클래스에서 접근 가능하도록 protected로 변경
    UFUNCTION()
    void OnAttackCooldownExpired();

private:
    // 내부 함수들
    void InitializeEnemy();
    void SetupAIController();
    void SetupCombatComponent();
    void SetupSensingComponent();

    // 이벤트 처리 함수들
    UFUNCTION()
    void OnPawnSeen(APawn* SeenPawn);

    UFUNCTION()
    void OnPawnLost(APawn* LostPawn);

    UFUNCTION()
    void OnDamageReceived(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator);

    UFUNCTION()
    void OnDeath(AActor* DeadActor);

    // 유틸리티 함수들
    bool IsPlayerCharacter(AActor* Actor) const;
    FVector GetRandomPatrolPoint() const;
    void UpdateBlackboard();
};
