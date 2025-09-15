// HuntingSpirit Game - Enemy Spawner Header
// 여러 스폰 포인트를 관리하고 적 스폰을 총괄하는 매니저 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HuntingSpirit/Enemies/Spawning/HSSpawnPoint.h"
#include "HSEnemySpawner.generated.h"

class AHSSpawnPoint;
class AHSWaveManager;
class AHSPlayerCharacter;

// 스폰 매니저 상태 열거형
UENUM(BlueprintType)
enum class EHSSpawnManagerState : uint8
{
	Inactive		UMETA(DisplayName = "Inactive"),		// 비활성화
	Active			UMETA(DisplayName = "Active"),			// 활성화
	Paused			UMETA(DisplayName = "Paused"),			// 일시정지
	WaveTransition	UMETA(DisplayName = "Wave Transition"),	// 웨이브 전환
	Completed		UMETA(DisplayName = "Completed"),		// 완료
	Error			UMETA(DisplayName = "Error")			// 오류
};

// 스폰 전략 열거형
UENUM(BlueprintType)
enum class EHSSpawnStrategy : uint8
{
	Random			UMETA(DisplayName = "Random"),			// 랜덤 스폰
	PlayerBased		UMETA(DisplayName = "Player Based"),	// 플레이어 기반
	Sequential		UMETA(DisplayName = "Sequential"),		// 순차적
	Weighted		UMETA(DisplayName = "Weighted"),		// 가중치 기반
	Distance		UMETA(DisplayName = "Distance"),		// 거리 기반
	Pressure		UMETA(DisplayName = "Pressure")			// 압박 기반
};

// 스폰 통계 구조체
USTRUCT(BlueprintType)
struct FHSSpawnStatistics
{
	GENERATED_BODY()

	// 총 스폰된 적 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
	int32 TotalSpawned = 0;

	// 현재 살아있는 적 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
	int32 CurrentAlive = 0;

	// 죽은 적 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
	int32 TotalKilled = 0;

	// 활성화된 스폰 포인트 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
	int32 ActiveSpawnPoints = 0;

	// 평균 스폰 간격
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
	float AverageSpawnInterval = 0.0f;

	// 스폰 효율성 (성공률)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
	float SpawnEfficiency = 0.0f;

	// 기본 생성자
	FHSSpawnStatistics()
	{
		Reset();
	}

	// 통계 초기화
	void Reset()
	{
		TotalSpawned = 0;
		CurrentAlive = 0;
		TotalKilled = 0;
		ActiveSpawnPoints = 0;
		AverageSpawnInterval = 0.0f;
		SpawnEfficiency = 0.0f;
	}
};

// 적응형 스폰 설정 구조체
USTRUCT(BlueprintType)
struct FHSAdaptiveSpawnSettings
{
	GENERATED_BODY()

	// 플레이어 거리 기반 스폰 조절
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn")
	bool bUseDistanceBasedSpawn = true;

	// 최소 플레이어 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn", meta = (ClampMin = "0.0"))
	float MinPlayerDistance = 500.0f;

	// 최대 플레이어 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn", meta = (ClampMin = "0.0"))
	float MaxPlayerDistance = 2000.0f;

	// 플레이어 수에 따른 스폰 스케일링
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn")
	bool bScaleWithPlayerCount = true;

	// 플레이어당 스폰 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn", meta = (ClampMin = "0.1"))
	float SpawnMultiplierPerPlayer = 1.0f;

	// 성능 기반 스폰 조절
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn")
	bool bUsePerformanceBasedSpawn = true;

	// 최대 동시 스폰 적 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn", meta = (ClampMin = "1"))
	int32 MaxConcurrentEnemies = 50;

	// 프레임레이트 임계값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Spawn", meta = (ClampMin = "10.0"))
	float FrameRateThreshold = 30.0f;

	// 기본 생성자
	FHSAdaptiveSpawnSettings()
	{
		bUseDistanceBasedSpawn = true;
		MinPlayerDistance = 500.0f;
		MaxPlayerDistance = 2000.0f;
		bScaleWithPlayerCount = true;
		SpawnMultiplierPerPlayer = 1.0f;
		bUsePerformanceBasedSpawn = true;
		MaxConcurrentEnemies = 50;
		FrameRateThreshold = 30.0f;
	}
};

// 스폰 매니저 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemySpawnedFromManager, AHSEnemyBase*, SpawnedEnemy, AHSSpawnPoint*, SpawnPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDiedFromManager, AHSEnemyBase*, DeadEnemy, AHSSpawnPoint*, SpawnPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpawnManagerStateChanged, EHSSpawnManagerState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllEnemiesCleared, AHSEnemySpawner*, SpawnManager);

UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API AHSEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	// 생성자
	AHSEnemySpawner();

	// 매 프레임 호출
	virtual void Tick(float DeltaTime) override;

	// 스폰 매니저 제어
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void StopSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void PauseSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void ResumeSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void ResetSpawner();

	// 스폰 포인트 관리
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawn Points")
	void RegisterSpawnPoint(AHSSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawn Points")
	void UnregisterSpawnPoint(AHSSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawn Points")
	void ActivateAllSpawnPoints();

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawn Points")
	void DeactivateAllSpawnPoints();

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Spawn Points")
	TArray<AHSSpawnPoint*> GetActiveSpawnPoints() const;

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Spawn Points")
	AHSSpawnPoint* GetNearestSpawnPoint(const FVector& Location) const;

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawn Points")
	void FindAndRegisterSpawnPoints();

	// 적 스폰 제어
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawning")
	AHSEnemyBase* SpawnEnemyAtPoint(AHSSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawning")
	AHSEnemyBase* SpawnEnemyAtRandomPoint();

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawning")
	TArray<AHSEnemyBase*> SpawnEnemyGroup(int32 Count, float Radius = 300.0f);

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Spawning")
	void SpawnEnemyWave(int32 EnemyCount, float SpawnInterval = 1.0f);

	// 상태 및 정보
	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Info")
	EHSSpawnManagerState GetSpawnManagerState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Info")
	FHSSpawnStatistics GetSpawnStatistics() const { return SpawnStatistics; }

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Info")
	int32 GetTotalActiveEnemies() const;

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Info")
	TArray<AHSEnemyBase*> GetAllSpawnedEnemies() const;

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Info")
	bool CanSpawnMoreEnemies() const;

	// 스폰 전략
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Strategy")
	void SetSpawnStrategy(EHSSpawnStrategy NewStrategy);

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Strategy")
	EHSSpawnStrategy GetSpawnStrategy() const { return CurrentSpawnStrategy; }

	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Strategy")
	AHSSpawnPoint* SelectSpawnPointByStrategy();

	// 적응형 스폰
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager|Adaptive")
	void UpdateAdaptiveSpawnSettings();

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Adaptive")
	float CalculateSpawnMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Spawn Manager|Adaptive")
	bool ShouldThrottleSpawning() const;

	// 스폰 이벤트 처리 (스폰 포인트에서 호출)
	UFUNCTION()
	void OnEnemySpawnedFromPoint(AHSEnemyBase* SpawnedEnemy, AHSSpawnPoint* SpawnPoint);

	UFUNCTION()
	void OnEnemyDiedFromPoint(AHSEnemyBase* DeadEnemy, AHSSpawnPoint* SpawnPoint);

	// 적 죽음 이벤트 처리 (델리게이트 바인딩용)
	UFUNCTION()
	void OnEnemyDied(AHSEnemyBase* DeadEnemy);

	// 에디터 전용 함수들
#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Spawn Manager|Debug")
	void TestSpawnWave();

	UFUNCTION(CallInEditor, Category = "Spawn Manager|Debug")
	void ClearAllEnemies();

	UFUNCTION(CallInEditor, Category = "Spawn Manager|Debug")
	void ShowSpawnManagerInfo();

	UFUNCTION(CallInEditor, Category = "Spawn Manager|Debug")
	void ValidateSpawnPoints();
#endif

	// 웨이브 매니저 연결
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void SetWaveManager(AHSWaveManager* InWaveManager);

	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	AHSWaveManager* GetWaveManager() const { return WaveManager; }

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Spawn Manager Events")
	FOnEnemySpawnedFromManager OnEnemySpawnedFromManager;

	UPROPERTY(BlueprintAssignable, Category = "Spawn Manager Events")
	FOnEnemyDiedFromManager OnEnemyDiedFromManager;

	UPROPERTY(BlueprintAssignable, Category = "Spawn Manager Events")
	FOnSpawnManagerStateChanged OnSpawnManagerStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Spawn Manager Events")
	FOnAllEnemiesCleared OnAllEnemiesCleared;

protected:
	// 게임 시작 시 호출
	virtual void BeginPlay() override;

	// 스폰 매니저 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Settings")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Settings")
	bool bAutoFindSpawnPoints = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Settings")
	EHSSpawnStrategy DefaultSpawnStrategy = EHSSpawnStrategy::Random;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Settings")
	float GlobalSpawnCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Settings")
	int32 MaxConcurrentSpawns = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Settings")
	bool bShowDebugInfo = false;

	// 적응형 스폰 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Manager|Adaptive")
	FHSAdaptiveSpawnSettings AdaptiveSettings;

	// 런타임 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Manager|Runtime")
	EHSSpawnManagerState CurrentState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Manager|Runtime")
	EHSSpawnStrategy CurrentSpawnStrategy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Manager|Runtime")
	TArray<AHSSpawnPoint*> RegisteredSpawnPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Manager|Runtime")
	TArray<AHSEnemyBase*> ManagedEnemies;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Manager|Runtime")
	FHSSpawnStatistics SpawnStatistics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Manager|Runtime")
	float LastSpawnTime;

	// 타이머 핸들들
	UPROPERTY()
	FTimerHandle GlobalSpawnTimer;

	UPROPERTY()
	FTimerHandle WaveSpawnTimer;

	UPROPERTY()
	FTimerHandle StatisticsUpdateTimer;

	// 웨이브 매니저 참조
	UPROPERTY()
	AHSWaveManager* WaveManager;

	// 웨이브 스폰 관련
	UPROPERTY()
	int32 CurrentWaveEnemyCount;

	UPROPERTY()
	int32 WaveEnemiesSpawned;

	UPROPERTY()
	float WaveSpawnInterval;

private:
	// 내부 함수들
	void InitializeSpawnManager();
	void SetManagerState(EHSSpawnManagerState NewState);
	
	// 스폰 로직
	void UpdateSpawning(float DeltaTime);
	void ProcessSpawnQueue();
	AHSSpawnPoint* SelectOptimalSpawnPoint() const;
	
	// 스폰 전략 구현
	AHSSpawnPoint* SelectRandomSpawnPoint() const;
	AHSSpawnPoint* SelectPlayerBasedSpawnPoint() const;
	AHSSpawnPoint* SelectSequentialSpawnPoint() const;
	AHSSpawnPoint* SelectWeightedSpawnPoint() const;
	AHSSpawnPoint* SelectDistanceBasedSpawnPoint() const;
	AHSSpawnPoint* SelectPressureBasedSpawnPoint() const;

	// 적 관리
	void RegisterManagedEnemy(AHSEnemyBase* Enemy);
	void UnregisterManagedEnemy(AHSEnemyBase* Enemy);
	void CleanupDeadEnemies();

	// 플레이어 관련
	TArray<AHSPlayerCharacter*> GetNearbyPlayers(const FVector& Location, float Radius) const;
	FVector GetAveragePlayerLocation() const;
	int32 GetActivePlayerCount() const;

	// 통계 및 성능
	void UpdateStatistics();
	void UpdatePerformanceMetrics();
	float GetCurrentFrameRate() const;

	// 유틸리티
	bool IsSpawnPointValid(AHSSpawnPoint* SpawnPoint) const;
	float CalculateSpawnPointWeight(AHSSpawnPoint* SpawnPoint) const;
	void LogSpawnManagerInfo(const FString& Message) const;

	// 웨이브 스폰 관련
	void OnWaveSpawnTimerComplete();
	void ProcessWaveSpawn();

	// 최적화 관련 상수
	static const int32 MaxManagedEnemies = 200;
	inline static const float StatisticsUpdateInterval = 2.0f;
	inline static const float CleanupInterval = 5.0f;
	
	// 성능 모니터링
	float FrameRateHistory[10];
	int32 FrameRateHistoryIndex;
	float LastCleanupTime;
	int32 SequentialSpawnIndex;
};
