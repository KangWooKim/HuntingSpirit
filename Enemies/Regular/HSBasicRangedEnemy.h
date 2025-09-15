// HuntingSpirit Game - Basic Ranged Enemy Header
// 원거리 공격을 수행하는 기본 적 클래스

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HSBasicRangedEnemy.generated.h"

// 전방 선언
class AHSObjectPool;
class AHSMagicProjectile;

// 원거리 적 AI 상태
UENUM(BlueprintType)
enum class EHSRangedEnemyTactic : uint8
{
    KeepDistance    UMETA(DisplayName = "Keep Distance"),      // 거리 유지
    FindCover       UMETA(DisplayName = "Find Cover"),         // 엄폐물 찾기
    Strafe          UMETA(DisplayName = "Strafe"),             // 옆으로 이동하며 공격
    Retreat         UMETA(DisplayName = "Retreat"),            // 후퇴
    Aggressive      UMETA(DisplayName = "Aggressive")          // 공격적 접근
};

// 원거리 공격 타입
UENUM(BlueprintType)
enum class EHSRangedAttackType : uint8
{
    SingleShot      UMETA(DisplayName = "Single Shot"),        // 단발
    Burst           UMETA(DisplayName = "Burst"),               // 연발
    Spread          UMETA(DisplayName = "Spread"),              // 산탄
    Homing          UMETA(DisplayName = "Homing"),              // 유도
    Barrage         UMETA(DisplayName = "Barrage")              // 탄막
};

// 원거리 공격 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProjectileFired, AHSMagicProjectile*, Projectile, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRangedAttackStarted, EHSRangedAttackType, AttackType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRangedAttackCompleted);

UCLASS()
class HUNTINGSPIRIT_API AHSBasicRangedEnemy : public AHSEnemyBase
{
    GENERATED_BODY()

public:
    // 생성자
    AHSBasicRangedEnemy();

    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;

    // 전투 관련 오버라이드
    virtual void PerformAttack() override;
    virtual void StartCombat(AActor* Target) override;
    virtual void EndCombat() override;

    // 원거리 공격 함수들
    UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
    void FireProjectile(FVector TargetLocation);

    UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
    void FireProjectileAtActor(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
    void PerformBurstFire(int32 BurstCount);

    UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
    void PerformSpreadShot(int32 ProjectileCount, float SpreadAngleParam);

    UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
    void PerformBarrage();

    // 전술 관련 함수들
    UFUNCTION(BlueprintPure, Category = "AI|Tactics")
    EHSRangedEnemyTactic GetCurrentTactic() const { return CurrentTactic; }

    UFUNCTION(BlueprintCallable, Category = "AI|Tactics")
    void SetTactic(EHSRangedEnemyTactic NewTactic);

    UFUNCTION(BlueprintCallable, Category = "AI|Tactics")
    void EvaluateTacticalSituation();

    // 위치 관련 함수들
    UFUNCTION(BlueprintPure, Category = "AI|Positioning")
    bool IsAtOptimalRange() const;

    UFUNCTION(BlueprintPure, Category = "AI|Positioning")
    bool HasLineOfSight(AActor* Target) const;

    UFUNCTION(BlueprintCallable, Category = "AI|Positioning")
    FVector GetOptimalPosition(AActor* Target) const;

    UFUNCTION(BlueprintCallable, Category = "AI|Positioning")
    FVector GetStrafePosition(bool bMoveRight) const;

    // 발사체 풀 관련
    UFUNCTION(BlueprintCallable, Category = "Combat|ObjectPool")
    void SetProjectilePool(AHSObjectPool* Pool) { ProjectilePool = Pool; }

    UFUNCTION(BlueprintPure, Category = "Combat|ObjectPool")
    AHSObjectPool* GetProjectilePool() const { return ProjectilePool; }

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnProjectileFired OnProjectileFired;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnRangedAttackStarted OnRangedAttackStarted;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnRangedAttackCompleted OnRangedAttackCompleted;

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 원거리 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Ranged Attack")
    EHSRangedAttackType PrimaryAttackType = EHSRangedAttackType::SingleShot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Ranged Attack")
    TSubclassOf<AHSMagicProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Ranged Attack", meta = (ClampMin = "0.0"))
    float ProjectileSpeed = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Ranged Attack", meta = (ClampMin = "0.0"))
    float ProjectileDamage = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Ranged Attack")
    FVector ProjectileSpawnOffset = FVector(100.0f, 0.0f, 50.0f); // 발사 위치 오프셋

    // 연발 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Burst Attack", meta = (EditCondition = "PrimaryAttackType == EHSRangedAttackType::Burst"))
    int32 BurstShotCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Burst Attack", meta = (EditCondition = "PrimaryAttackType == EHSRangedAttackType::Burst", ClampMin = "0.01"))
    float BurstShotInterval = 0.2f;

    // 산탄 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Spread Attack", meta = (EditCondition = "PrimaryAttackType == EHSRangedAttackType::Spread"))
    int32 SpreadProjectileCount = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Spread Attack", meta = (EditCondition = "PrimaryAttackType == EHSRangedAttackType::Spread", ClampMin = "0.0", ClampMax = "90.0"))
    float SpreadAngle = 30.0f;

    // 탄막 공격 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Barrage Attack", meta = (EditCondition = "PrimaryAttackType == EHSRangedAttackType::Barrage"))
    int32 BarrageProjectileCount = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Barrage Attack", meta = (EditCondition = "PrimaryAttackType == EHSRangedAttackType::Barrage", ClampMin = "0.01"))
    float BarrageInterval = 0.1f;

    // 원거리 전투 거리 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Range", meta = (ClampMin = "0.0"))
    float OptimalAttackRange = 600.0f; // 최적 공격 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Range", meta = (ClampMin = "0.0"))
    float MinimumAttackRange = 300.0f; // 최소 공격 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Range", meta = (ClampMin = "0.0"))
    float MaximumAttackRange = 1000.0f; // 최대 공격 거리

    // 정확도 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Accuracy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BaseAccuracy = 0.8f; // 기본 정확도 (0~1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Accuracy", meta = (ClampMin = "0.0"))
    float AccuracyPenaltyPerMeter = 0.001f; // 거리당 정확도 감소

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Accuracy", meta = (ClampMin = "0.0"))
    float MovementAccuracyPenalty = 0.3f; // 이동 중 정확도 감소

    // 전술 설정
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Tactics")
    EHSRangedEnemyTactic CurrentTactic = EHSRangedEnemyTactic::KeepDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics")
    bool bUseDynamicTactics = true; // 상황에 따라 전술 변경

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ClampMin = "0.0"))
    float TacticsEvaluationInterval = 2.0f; // 전술 재평가 간격

    // 옆걸음 이동 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    bool bEnableStrafing = true; // 옆걸음 이동 활성화

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement", meta = (ClampMin = "0.0"))
    float StrafeSpeed = 300.0f; // 옆걸음 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement", meta = (ClampMin = "0.0"))
    float StrafeChangeInterval = 3.0f; // 옆걸음 방향 변경 간격

    // 오브젝트 풀링
    UPROPERTY()
    AHSObjectPool* ProjectilePool;

    // 공격 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
    bool bIsPerformingAttack = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
    int32 RemainingBurstShots = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
    int32 RemainingBarrageShots = 0;

    // 타이머 핸들
    FTimerHandle BurstFireTimerHandle;
    FTimerHandle BarrageTimerHandle;
    FTimerHandle TacticsEvaluationTimerHandle;
    FTimerHandle StrafeTimerHandle;

    // 현재 옆걸음 방향
    bool bStrafingRight = true;

private:
    // 내부 함수들
    void InitializeRangedEnemy();
    void SetupProjectilePool();
    
    // 발사체 생성 및 발사
    AHSMagicProjectile* CreateProjectile(FVector StartLocation, FVector Direction);
    void LaunchProjectile(AHSMagicProjectile* Projectile, FVector Direction);
    
    // 정확도 계산
    float CalculateAccuracy(AActor* Target) const;
    FVector ApplyAccuracySpread(FVector BaseDirection, float Accuracy) const;
    
    // 연발 공격 처리
    UFUNCTION()
    void FireBurstShot();
    
    // 탄막 공격 처리
    UFUNCTION()
    void FireBarrageShot();
    
    // 전술 평가
    UFUNCTION()
    void EvaluateTactics();
    
    // 옆걸음 이동
    UFUNCTION()
    void ChangeStrafeDirection();
    void UpdateStrafing(float DeltaTime);
    
    // 유틸리티 함수들
    FVector GetProjectileSpawnLocation() const;
    bool ShouldRetreat() const;
    bool ShouldSeekCover() const;
    float GetTargetDistance() const;
};
