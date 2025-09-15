// HuntingSpirit Game - Wave Manager Header
// 적 웨이브 시스템을 관리하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HuntingSpirit/Enemies/Spawning/HSEnemySpawner.h"
#include "HSWaveManager.generated.h"

class AHSEnemySpawner;
class AHSEnemyBase;

// 웨이브 상태 열거형
UENUM(BlueprintType)
enum class EHSWaveState : uint8
{
	Inactive		UMETA(DisplayName = "Inactive"),		// 비활성화
	Preparing		UMETA(DisplayName = "Preparing"),		// 준비 중
	InProgress		UMETA(DisplayName = "In Progress"),		// 진행 중
	Completed		UMETA(DisplayName = "Completed"),		// 완료
	Failed			UMETA(DisplayName = "Failed"),			// 실패
	Paused			UMETA(DisplayName = "Paused")			// 일시정지
};

// 웨이브 타입 열거형
UENUM(BlueprintType)
enum class EHSWaveType : uint8
{
	Standard		UMETA(DisplayName = "Standard"),		// 표준 웨이브
	Elite			UMETA(DisplayName = "Elite"),			// 정예 웨이브
	Boss			UMETA(DisplayName = "Boss"),			// 보스 웨이브
	Swarm			UMETA(DisplayName = "Swarm"),			// 대규모 웨이브
	Mixed			UMETA(DisplayName = "Mixed"),			// 혼합 웨이브
	Survival		UMETA(DisplayName = "Survival"),		// 생존 웨이브
	Timed			UMETA(DisplayName = "Timed")			// 시간 제한 웨이브
};

// 적 스폰 정보 구조체
USTRUCT(BlueprintType)
struct FHSEnemySpawnInfo
{
	GENERATED_BODY()

	// 스폰할 적 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn")
	TSubclassOf<AHSEnemyBase> EnemyClass;

	// 스폰할 적 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn", meta = (ClampMin = "1"))
	int32 Count = 1;

	// 스폰 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn", meta = (ClampMin = "0.0"))
	float SpawnDelay = 0.0f;

	// 스폰 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn", meta = (ClampMin = "0.1"))
	float SpawnInterval = 1.0f;

	// 스폰 확률
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;

	// 그룹 스폰 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn")
	bool bSpawnAsGroup = false;

	// 그룹 스폰 반지름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawn", meta = (ClampMin = "50.0"))
	float GroupRadius = 200.0f;

	// 기본 생성자
	FHSEnemySpawnInfo()
	{
		EnemyClass = nullptr;
		Count = 1;
		SpawnDelay = 0.0f;
		SpawnInterval = 1.0f;
		SpawnChance = 1.0f;
		bSpawnAsGroup = false;
		GroupRadius = 200.0f;
	}
};

// 웨이브 데이터 구조체
USTRUCT(BlueprintType)
struct FHSWaveData
{
	GENERATED_BODY()

	// 웨이브 번호
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data")
	int32 WaveNumber = 1;

	// 웨이브 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data")
	FString WaveName = TEXT("Wave 1");

	// 웨이브 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data")
	EHSWaveType WaveType = EHSWaveType::Standard;

	// 웨이브 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data", meta = (MultiLine = "true"))
	FString WaveDescription = TEXT("Standard wave with basic enemies.");

	// 스폰할 적들 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data")
	TArray<FHSEnemySpawnInfo> EnemySpawns;

	// 웨이브 준비 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data", meta = (ClampMin = "0.0"))
	float PrepareTime = 5.0f;

	// 웨이브 시간 제한 (0이면 무제한)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data", meta = (ClampMin = "0.0"))
	float TimeLimit = 0.0f;

	// 다음 웨이브까지의 휴식 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data", meta = (ClampMin = "0.0"))
	float RestTime = 10.0f;

	// 웨이브 완료 조건 (모든 적 제거)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data")
	bool bRequireAllEnemiesKilled = true;

	// 플레이어 수에 따른 스케일링
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data")
	bool bScaleWithPlayerCount = true;

	// 플레이어당 스케일링 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Data", meta = (ClampMin = "0.1"))
	float PlayerScaleMultiplier = 0.5f;

	// 기본 생성자
	FHSWaveData()
	{
		WaveNumber = 1;
		WaveName = TEXT("Wave 1");
		WaveType = EHSWaveType::Standard;
		WaveDescription = TEXT("Standard wave with basic enemies.");
		PrepareTime = 5.0f;
		TimeLimit = 0.0f;
		RestTime = 10.0f;
		bRequireAllEnemiesKilled = true;
		bScaleWithPlayerCount = true;
		PlayerScaleMultiplier = 0.5f;
	}
};

// 웨이브 통계 구조체
USTRUCT(BlueprintType)
struct FHSWaveStatistics
{
	GENERATED_BODY()

	// 현재 웨이브 번호
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	int32 CurrentWave = 0;

	// 완료된 웨이브 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	int32 CompletedWaves = 0;

	// 총 처치한 적 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	int32 TotalEnemiesKilled = 0;

	// 현재 웨이브에서 처치한 적 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	int32 CurrentWaveKills = 0;

	// 현재 웨이브 스폰된 적 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	int32 CurrentWaveSpawned = 0;

	// 현재 웨이브 진행 시간
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	float CurrentWaveTime = 0.0f;

	// 평균 웨이브 완료 시간
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	float AverageWaveTime = 0.0f;

	// 최고 달성 웨이브
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Statistics")
	int32 HighestWave = 0;

	// 기본 생성자
	FHSWaveStatistics()
	{
		Reset();
	}

	// 통계 초기화
	void Reset()
	{
		CurrentWave = 0;
		CompletedWaves = 0;
		TotalEnemiesKilled = 0;
		CurrentWaveKills = 0;
		CurrentWaveSpawned = 0;
		CurrentWaveTime = 0.0f;
		AverageWaveTime = 0.0f;
		HighestWave = 0;
	}
};

// 웨이브 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWaveCompleted, int32, WaveNumber, float, CompletionTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveFailed, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWavePreparation, int32, NextWaveNumber, float, PrepareTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllWavesCompleted, int32, TotalWavesCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStateChanged, EHSWaveState, NewState);

UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API AHSWaveManager : public AActor
{
	GENERATED_BODY()

public:
	// 생성자
	AHSWaveManager();

	// 매 프레임 호출
	virtual void Tick(float DeltaTime) override;

	// 웨이브 제어
	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void StartWaveSystem();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void StopWaveSystem();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void PauseWaveSystem();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void ResumeWaveSystem();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void ResetWaveSystem();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void StartNextWave();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void CompleteCurrentWave();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void FailCurrentWave();

	// 웨이브 정보
	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	EHSWaveState GetWaveState() const { return CurrentWaveState; }

	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	int32 GetCurrentWaveNumber() const { return CurrentWaveIndex + 1; }

	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	FHSWaveData GetCurrentWaveData() const;

	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	FHSWaveStatistics GetWaveStatistics() const { return WaveStatistics; }

	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	float GetCurrentWaveProgress() const;

	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Wave Manager|Info")
	bool HasMoreWaves() const;

	// 웨이브 설정
	UFUNCTION(BlueprintCallable, Category = "Wave Manager|Configuration")
	void SetWaveData(const TArray<FHSWaveData>& NewWaveData);

	UFUNCTION(BlueprintCallable, Category = "Wave Manager|Configuration")
	void AddWaveData(const FHSWaveData& WaveData);

	UFUNCTION(BlueprintCallable, Category = "Wave Manager|Configuration")
	void ClearWaveData();

	UFUNCTION(BlueprintCallable, Category = "Wave Manager|Configuration")
	void SetCurrentWave(int32 WaveNumber);

	// 스폰 매니저 연결
	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void SetSpawnManager(AHSEnemySpawner* InSpawnManager);

	UFUNCTION(BlueprintPure, Category = "Wave Manager")
	AHSEnemySpawner* GetSpawnManager() const { return SpawnManager; }

	// 적 관리
	UFUNCTION()
	void OnEnemySpawned(AHSEnemyBase* SpawnedEnemy, AHSSpawnPoint* SpawnPoint);

	UFUNCTION()
	void OnEnemyKilled(AHSEnemyBase* KilledEnemy, AHSSpawnPoint* SpawnPoint);

	UFUNCTION()
	void OnAllEnemiesCleared(AHSEnemySpawner* SpawnManagerRef);

	// 생성 및 로딩
	UFUNCTION(BlueprintCallable, Category = "Wave Manager|Generation")
	void GenerateRandomWaves(int32 WaveCount = 10);

	UFUNCTION(BlueprintCallable, Category = "Wave Manager|Generation")
	FHSWaveData GenerateRandomWave(int32 WaveNumber, EHSWaveType WaveType = EHSWaveType::Standard);

	// 에디터 전용 함수들
#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Wave Manager|Debug")
	void TestCurrentWave();

	UFUNCTION(CallInEditor, Category = "Wave Manager|Debug")
	void SkipToNextWave();

	UFUNCTION(CallInEditor, Category = "Wave Manager|Debug")
	void ShowWaveInfo();

	UFUNCTION(CallInEditor, Category = "Wave Manager|Debug")
	void GenerateTestWaves();
#endif

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Wave Events")
	FOnWaveStarted OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Wave Events")
	FOnWaveCompleted OnWaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Wave Events")
	FOnWaveFailed OnWaveFailed;

	UPROPERTY(BlueprintAssignable, Category = "Wave Events")
	FOnWavePreparation OnWavePreparation;

	UPROPERTY(BlueprintAssignable, Category = "Wave Events")
	FOnAllWavesCompleted OnAllWavesCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Wave Events")
	FOnWaveStateChanged OnWaveStateChanged;

protected:
	// 게임 시작 시 호출
	virtual void BeginPlay() override;

	// 웨이브 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Configuration")
	TArray<FHSWaveData> WaveDataArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Configuration")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Configuration")
	bool bLoopWaves = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Configuration")
	bool bInfiniteMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Configuration")
	float GlobalDifficultyMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Configuration")
	bool bShowDebugInfo = false;

	// 런타임 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Runtime")
	EHSWaveState CurrentWaveState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Runtime")
	int32 CurrentWaveIndex;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Runtime")
	FHSWaveStatistics WaveStatistics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Runtime")
	float CurrentWaveStartTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Runtime")
	float WavePreparationStartTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave Runtime")
	TArray<AHSEnemyBase*> CurrentWaveEnemies;

	// 타이머 핸들들
	UPROPERTY()
	FTimerHandle WavePreparationTimer;

	UPROPERTY()
	FTimerHandle WaveTimeoutTimer;

	UPROPERTY()
	FTimerHandle EnemySpawnTimer;

	UPROPERTY()
	FTimerHandle RestTimer;

	// 스폰 매니저 참조
	UPROPERTY()
	AHSEnemySpawner* SpawnManager;

	// 현재 웨이브 스폰 관련
	UPROPERTY()
	int32 CurrentSpawnInfoIndex;

	UPROPERTY()
	int32 CurrentSpawnCount;

private:
	// 내부 함수들
	void InitializeWaveManager();
	void SetWaveState(EHSWaveState NewState);

	// 웨이브 진행 로직
	void StartWavePreparation();
	void StartCurrentWave();
	void UpdateCurrentWave(float DeltaTime);
	void ProcessWaveCompletion();
	void ProcessWaveFailure();

	// 적 스폰 로직
	void StartWaveSpawning();
	void ProcessEnemySpawning();
	void SpawnCurrentWaveEnemies();
	void SpawnEnemyFromInfo(const FHSEnemySpawnInfo& SpawnInfo);

	// 웨이브 관리
	bool IsCurrentWaveComplete() const;
	bool IsCurrentWaveFailed() const;
	void PrepareNextWave();
	void AdvanceToNextWave();

	// 타이머 콜백
	UFUNCTION()
	void OnWavePreparationComplete();

	UFUNCTION()
	void OnWaveTimeout();

	UFUNCTION()
	void OnEnemySpawnTimer();

	UFUNCTION()
	void OnRestTimerComplete();

	// 통계 관리
	void UpdateWaveStatistics();
	void RecordWaveCompletion();

	// 적 관리
	void RegisterWaveEnemy(AHSEnemyBase* Enemy);
	void UnregisterWaveEnemy(AHSEnemyBase* Enemy);
	void CleanupCurrentWaveEnemies();

	// 스케일링 및 난이도
	int32 CalculateScaledEnemyCount(int32 BaseCount) const;
	float CalculateDifficultyMultiplier() const;
	int32 GetActivePlayerCount() const;

	// 유틸리티 함수들
	void LogWaveInfo(const FString& Message) const;
	void DrawWaveDebugInfo() const;

	// 웨이브 생성 헬퍼
	TArray<TSubclassOf<AHSEnemyBase>> GetAvailableEnemyClasses() const;
	FHSEnemySpawnInfo CreateRandomSpawnInfo(TSubclassOf<AHSEnemyBase> EnemyClass, int32 WaveNumber) const;

	// 상수
	inline static const float DefaultWaveTimeout = 300.0f; // 5분
	static const int32 MaxEnemiesPerWave = 50;
	inline static const float MinSpawnInterval = 0.5f;
	inline static const float MaxSpawnInterval = 5.0f;
};
