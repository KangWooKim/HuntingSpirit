// HuntingSpirit Game - Enemy Spawn Point Header
// 적이 스폰될 위치와 설정을 관리하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HSSpawnPoint.generated.h"

class AHSEnemyBase;
class AHSEnemySpawner;

// 스폰 포인트 상태 열거형
UENUM(BlueprintType)
enum class EHSSpawnPointState : uint8
{
	Inactive		UMETA(DisplayName = "Inactive"),		// 비활성화
	Active			UMETA(DisplayName = "Active"),			// 활성화
	Spawning		UMETA(DisplayName = "Spawning"),		// 스폰 중
	Occupied		UMETA(DisplayName = "Occupied"),		// 점유됨
	Cooldown		UMETA(DisplayName = "Cooldown"),		// 쿨다운
	Disabled		UMETA(DisplayName = "Disabled")			// 비활성화됨
};

// 스폰 설정 구조체
USTRUCT(BlueprintType)
struct FHSSpawnSettings
{
	GENERATED_BODY()

	// 스폰할 적 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
	TSubclassOf<AHSEnemyBase> EnemyClass;

	// 스폰 확률 (0.0 ~ 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;

	// 동시 스폰 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings", meta = (ClampMin = "1", ClampMax = "20"))
	int32 SpawnCount = 1;

	// 스폰 딜레이 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings", meta = (ClampMin = "0.0"))
	float SpawnDelay = 0.0f;

	// 스폰 쿨다운 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings", meta = (ClampMin = "0.0"))
	float SpawnCooldown = 5.0f;

	// 스폰 반지름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings", meta = (ClampMin = "0.0"))
	float SpawnRadius = 100.0f;

	// 최대 스폰 개수 제한
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings", meta = (ClampMin = "1"))
	int32 MaxSpawnedEnemies = 5;

	// 기본 생성자
	FHSSpawnSettings()
	{
		EnemyClass = nullptr;
		SpawnChance = 1.0f;
		SpawnCount = 1;
		SpawnDelay = 0.0f;
		SpawnCooldown = 5.0f;
		SpawnRadius = 100.0f;
		MaxSpawnedEnemies = 5;
	}
};

// 스폰 포인트 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemySpawned, AHSEnemyBase*, SpawnedEnemy, AHSSpawnPoint*, SpawnPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDied, AHSEnemyBase*, DeadEnemy, AHSSpawnPoint*, SpawnPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpawnPointStateChanged, EHSSpawnPointState, NewState);

UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API AHSSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	// 생성자
	AHSSpawnPoint();

	// 매 프레임 호출
	virtual void Tick(float DeltaTime) override;

	// 스폰 관련 함수들
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void ActivateSpawnPoint();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void DeactivateSpawnPoint();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	bool CanSpawnEnemy() const;

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	AHSEnemyBase* SpawnEnemy();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void SpawnEnemyWithDelay();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void ForceSpawn();

	// 상태 관련 함수들
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	EHSSpawnPointState GetSpawnPointState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void SetSpawnPointState(EHSSpawnPointState NewState);

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool IsActive() const { return CurrentState == EHSSpawnPointState::Active; }

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool IsOccupied() const { return CurrentState == EHSSpawnPointState::Occupied; }

	// 스폰된 적 관리
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	int32 GetSpawnedEnemyCount() const { return SpawnedEnemies.Num(); }

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	TArray<AHSEnemyBase*> GetSpawnedEnemies() const { return SpawnedEnemies; }

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void ClearDeadEnemies();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void KillAllSpawnedEnemies();

	// 설정 관련 함수들
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void SetSpawnSettings(const FHSSpawnSettings& NewSettings);

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	FHSSpawnSettings GetSpawnSettings() const { return SpawnSettings; }

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void SetEnemyClass(TSubclassOf<AHSEnemyBase> NewEnemyClass);

	// 스폰 위치 관련
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	FVector GetRandomSpawnLocation() const;

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool IsLocationClear(const FVector& Location) const;

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void SetSpawnRadius(float NewRadius);

	// 스폰 매니저 연결
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void SetSpawnManager(AHSEnemySpawner* InSpawnManager);

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	AHSEnemySpawner* GetSpawnManager() const { return SpawnManager; }

	// 에디터 전용 함수들
#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Spawn Point|Debug")
	void TestSpawn();

	UFUNCTION(CallInEditor, Category = "Spawn Point|Debug")
	void ClearAllSpawned();

	UFUNCTION(CallInEditor, Category = "Spawn Point|Debug")
	void ShowSpawnRadius();
#endif

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Spawn Point Events")
	FOnEnemySpawned OnEnemySpawned;

	UPROPERTY(BlueprintAssignable, Category = "Spawn Point Events")
	FOnEnemyDied OnEnemyDied;

	UPROPERTY(BlueprintAssignable, Category = "Spawn Point Events")
	FOnSpawnPointStateChanged OnSpawnPointStateChanged;

protected:
	// 게임 시작 시 호출
	virtual void BeginPlay() override;

	// 컴포넌트들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SpawnPointMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SpawnRadiusComponent;

	// 스폰 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
	FHSSpawnSettings SpawnSettings;

	// 스폰 포인트 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bAutoActivate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bRespawnEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bShowDebugInfo = false;

	// 런타임 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point|Runtime")
	EHSSpawnPointState CurrentState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point|Runtime")
	TArray<AHSEnemyBase*> SpawnedEnemies;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point|Runtime")
	float LastSpawnTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point|Runtime")
	int32 TotalSpawnedCount;

	// 타이머 핸들
	UPROPERTY()
	FTimerHandle SpawnDelayTimer;

	UPROPERTY()
	FTimerHandle SpawnCooldownTimer;

	// 스폰 매니저 참조
	UPROPERTY()
	AHSEnemySpawner* SpawnManager;

private:
	// 내부 함수들
	void InitializeSpawnPoint();
	void SetupComponents();
	void UpdateSpawnRadius();

	// 스폰 관련 내부 함수들
	bool ValidateSpawnConditions() const;
	FVector FindValidSpawnLocation() const;
	void OnSpawnDelayComplete();
	void OnSpawnCooldownComplete();

	// 이벤트 처리 함수들
	UFUNCTION()
	void OnSpawnedEnemyDeath(AHSEnemyBase* DeadEnemy);

	// 유틸리티 함수들
	void RegisterSpawnedEnemy(AHSEnemyBase* Enemy);
	void UnregisterSpawnedEnemy(AHSEnemyBase* Enemy);
	void CheckAndUpdateOccupiedState();

	// 디버그 함수들
	void DrawDebugInfo() const;
	void LogSpawnInfo(const FString& Message) const;

	// 최적화 관련
	static const int32 MaxTraceDistance = 1000;
	inline static const float MinSpawnDistance = 50.0f;
	inline static const float MaxSpawnAttempts = 10;
};
