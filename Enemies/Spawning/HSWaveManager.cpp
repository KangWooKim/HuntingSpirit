// HuntingSpirit Game - Wave Manager Implementation
// 적 웨이브 시스템을 관리하는 클래스 구현

#include "HSWaveManager.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HuntingSpirit/Enemies/Regular/HSBasicMeleeEnemy.h"
#include "HuntingSpirit/Enemies/Regular/HSBasicRangedEnemy.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// 생성자 - 기본 설정 및 초기화
AHSWaveManager::AHSWaveManager()
{
	// 매 프레임 틱 활성화
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 네트워킹 설정
	bReplicates = true;
	SetReplicateMovement(false); // 웨이브 매니저는 이동하지 않음

	// 기본값 초기화
	CurrentWaveState = EHSWaveState::Inactive;
	CurrentWaveIndex = -1;
	CurrentWaveStartTime = 0.0f;
	WavePreparationStartTime = 0.0f;
	SpawnManager = nullptr;
	CurrentSpawnInfoIndex = 0;
	CurrentSpawnCount = 0;

	// 배열 메모리 예약 (성능 최적화)
	WaveDataArray.Reserve(20);
	CurrentWaveEnemies.Reserve(MaxEnemiesPerWave);
}

// 게임 시작 시 초기화
void AHSWaveManager::BeginPlay()
{
	Super::BeginPlay();

	InitializeWaveManager();

	// 자동 시작 설정
	if (bAutoStart)
	{
		// 약간의 딜레이 후 시작 (다른 시스템들의 초기화 대기)
		FTimerHandle DelayTimer;
		GetWorldTimerManager().SetTimer(DelayTimer, this, &AHSWaveManager::StartWaveSystem, 2.0f, false);
	}
}

// 웨이브 매니저 초기화
void AHSWaveManager::InitializeWaveManager()
{
	// 통계 초기화
	WaveStatistics.Reset();

	// 현재 웨이브 인덱스 초기화
	CurrentWaveIndex = -1;

	// 타이머들 정리
	GetWorldTimerManager().ClearTimer(WavePreparationTimer);
	GetWorldTimerManager().ClearTimer(WaveTimeoutTimer);
	GetWorldTimerManager().ClearTimer(EnemySpawnTimer);
	GetWorldTimerManager().ClearTimer(RestTimer);

	// 기본 웨이브 데이터 생성 (비어있는 경우)
	if (WaveDataArray.Num() == 0)
	{
		GenerateRandomWaves(5); // 기본 5개 웨이브 생성
	}

	LogWaveInfo(TEXT("웨이브 매니저 초기화 완료"));
}

// 매 프레임 업데이트
void AHSWaveManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 현재 웨이브 업데이트
	if (CurrentWaveState == EHSWaveState::InProgress)
	{
		UpdateCurrentWave(DeltaTime);
	}

	// 통계 업데이트
	UpdateWaveStatistics();

	// 디버그 정보 표시
	if (bShowDebugInfo)
	{
		DrawWaveDebugInfo();
	}
}

// 웨이브 시스템 시작
void AHSWaveManager::StartWaveSystem()
{
	if (WaveDataArray.Num() == 0)
	{
		LogWaveInfo(TEXT("웨이브 데이터가 없어 시스템을 시작할 수 없습니다."));
		return;
	}

	if (!SpawnManager)
	{
		// 스폰 매니저 자동 검색
		TArray<AActor*> FoundSpawnManagers;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSEnemySpawner::StaticClass(), FoundSpawnManagers);
		
		if (FoundSpawnManagers.Num() > 0)
		{
			SpawnManager = Cast<AHSEnemySpawner>(FoundSpawnManagers[0]);
			SpawnManager->SetWaveManager(this);
		}
		else
		{
			LogWaveInfo(TEXT("스폰 매니저를 찾을 수 없습니다."));
			return;
		}
	}

	// 웨이브 시스템 시작
	CurrentWaveIndex = -1;
	SetWaveState(EHSWaveState::Preparing);
	StartNextWave();

	LogWaveInfo(TEXT("웨이브 시스템 시작"));
}

// 웨이브 시스템 중지
void AHSWaveManager::StopWaveSystem()
{
	SetWaveState(EHSWaveState::Inactive);

	// 모든 타이머 정리
	GetWorldTimerManager().ClearAllTimersForObject(this);

	// 현재 웨이브 적들 정리
	CleanupCurrentWaveEnemies();

	LogWaveInfo(TEXT("웨이브 시스템 중지"));
}

// 웨이브 시스템 일시정지
void AHSWaveManager::PauseWaveSystem()
{
	if (CurrentWaveState == EHSWaveState::InProgress)
	{
		SetWaveState(EHSWaveState::Paused);
		
		// 타이머들 일시정지
		GetWorldTimerManager().PauseTimer(WaveTimeoutTimer);
		GetWorldTimerManager().PauseTimer(EnemySpawnTimer);

		LogWaveInfo(TEXT("웨이브 시스템 일시정지"));
	}
}

// 웨이브 시스템 재개
void AHSWaveManager::ResumeWaveSystem()
{
	if (CurrentWaveState == EHSWaveState::Paused)
	{
		SetWaveState(EHSWaveState::InProgress);
		
		// 타이머들 재개
		GetWorldTimerManager().UnPauseTimer(WaveTimeoutTimer);
		GetWorldTimerManager().UnPauseTimer(EnemySpawnTimer);

		LogWaveInfo(TEXT("웨이브 시스템 재개"));
	}
}

// 웨이브 시스템 리셋
void AHSWaveManager::ResetWaveSystem()
{
	StopWaveSystem();
	
	// 통계 초기화
	WaveStatistics.Reset();
	CurrentWaveIndex = -1;

	LogWaveInfo(TEXT("웨이브 시스템 리셋 완료"));
}

// 다음 웨이브 시작
void AHSWaveManager::StartNextWave()
{
	// 웨이브 인덱스 증가
	CurrentWaveIndex++;

	// 웨이브가 더 있는지 확인
	if (!HasMoreWaves())
	{
		if (bInfiniteMode)
		{
			// 무한 모드: 랜덤 웨이브 생성
			FHSWaveData NewWave = GenerateRandomWave(CurrentWaveIndex + 1, EHSWaveType::Standard);
			WaveDataArray.Add(NewWave);
		}
		else if (bLoopWaves && WaveDataArray.Num() > 0)
		{
			// 루프 모드: 처음부터 다시
			CurrentWaveIndex = 0;
		}
		else
		{
			// 모든 웨이브 완료
			OnAllWavesCompleted.Broadcast(WaveStatistics.CompletedWaves);
			SetWaveState(EHSWaveState::Completed);
			LogWaveInfo(TEXT("모든 웨이브 완료!"));
			return;
		}
	}

	// 웨이브 준비 시작
	StartWavePreparation();
}

// 현재 웨이브 완료
void AHSWaveManager::CompleteCurrentWave()
{
	if (CurrentWaveState == EHSWaveState::InProgress)
	{
		ProcessWaveCompletion();
	}
}

// 현재 웨이브 실패
void AHSWaveManager::FailCurrentWave()
{
	if (CurrentWaveState == EHSWaveState::InProgress)
	{
		ProcessWaveFailure();
	}
}

// 현재 웨이브 데이터 반환
FHSWaveData AHSWaveManager::GetCurrentWaveData() const
{
	if (CurrentWaveIndex >= 0 && CurrentWaveIndex < WaveDataArray.Num())
	{
		return WaveDataArray[CurrentWaveIndex];
	}
	return FHSWaveData(); // 기본값 반환
}

// 현재 웨이브 진행률 계산
float AHSWaveManager::GetCurrentWaveProgress() const
{
	if (CurrentWaveState != EHSWaveState::InProgress)
	{
		return 0.0f;
	}

	FHSWaveData CurrentWave = GetCurrentWaveData();
	
	// 적 처치 기반 진행률
	if (CurrentWave.bRequireAllEnemiesKilled)
	{
		if (WaveStatistics.CurrentWaveSpawned > 0)
		{
			return (float)WaveStatistics.CurrentWaveKills / WaveStatistics.CurrentWaveSpawned;
		}
	}
	
	// 시간 기반 진행률
	if (CurrentWave.TimeLimit > 0.0f)
	{
		float ElapsedTime = GetWorld()->GetTimeSeconds() - CurrentWaveStartTime;
		return FMath::Clamp(ElapsedTime / CurrentWave.TimeLimit, 0.0f, 1.0f);
	}

	return 0.0f;
}

// 남은 시간 계산
float AHSWaveManager::GetRemainingTime() const
{
	if (CurrentWaveState != EHSWaveState::InProgress)
	{
		return 0.0f;
	}

	FHSWaveData CurrentWave = GetCurrentWaveData();
	if (CurrentWave.TimeLimit > 0.0f)
	{
		float ElapsedTime = GetWorld()->GetTimeSeconds() - CurrentWaveStartTime;
		return FMath::Max(0.0f, CurrentWave.TimeLimit - ElapsedTime);
	}

	return 0.0f; // 시간 제한 없음
}

// 더 많은 웨이브가 있는지 확인
bool AHSWaveManager::HasMoreWaves() const
{
	return (CurrentWaveIndex + 1) < WaveDataArray.Num() || bInfiniteMode || bLoopWaves;
}

// 웨이브 데이터 설정
void AHSWaveManager::SetWaveData(const TArray<FHSWaveData>& NewWaveData)
{
	WaveDataArray = NewWaveData;
	LogWaveInfo(FString::Printf(TEXT("웨이브 데이터 설정 완료: %d개 웨이브"), WaveDataArray.Num()));
}

// 웨이브 데이터 추가
void AHSWaveManager::AddWaveData(const FHSWaveData& WaveData)
{
	WaveDataArray.Add(WaveData);
}

// 웨이브 데이터 클리어
void AHSWaveManager::ClearWaveData()
{
	WaveDataArray.Empty();
}

// 현재 웨이브 설정
void AHSWaveManager::SetCurrentWave(int32 WaveNumber)
{
	if (WaveNumber >= 1 && WaveNumber <= WaveDataArray.Num())
	{
		CurrentWaveIndex = WaveNumber - 1;
		LogWaveInfo(FString::Printf(TEXT("현재 웨이브를 %d로 설정"), WaveNumber));
	}
}

// 스폰 매니저 설정
void AHSWaveManager::SetSpawnManager(AHSEnemySpawner* InSpawnManager)
{
	SpawnManager = InSpawnManager;
	
	if (SpawnManager)
	{
		// 이벤트 바인딩
		SpawnManager->OnEnemySpawnedFromManager.AddDynamic(this, &AHSWaveManager::OnEnemySpawned);
		SpawnManager->OnEnemyDiedFromManager.AddDynamic(this, &AHSWaveManager::OnEnemyKilled);
		SpawnManager->OnAllEnemiesCleared.AddDynamic(this, &AHSWaveManager::OnAllEnemiesCleared);
	}
}

// 적이 스폰될 때 호출
void AHSWaveManager::OnEnemySpawned(AHSEnemyBase* SpawnedEnemy, AHSSpawnPoint* SpawnPoint)
{
	if (IsValid(SpawnedEnemy) && CurrentWaveState == EHSWaveState::InProgress)
	{
		RegisterWaveEnemy(SpawnedEnemy);
		WaveStatistics.CurrentWaveSpawned++;
	}
}

// 적이 죽을 때 호출
void AHSWaveManager::OnEnemyKilled(AHSEnemyBase* KilledEnemy, AHSSpawnPoint* SpawnPoint)
{
	if (IsValid(KilledEnemy) && CurrentWaveState == EHSWaveState::InProgress)
	{
		UnregisterWaveEnemy(KilledEnemy);
		WaveStatistics.CurrentWaveKills++;
		WaveStatistics.TotalEnemiesKilled++;

		// 웨이브 완료 조건 확인
		if (IsCurrentWaveComplete())
		{
			CompleteCurrentWave();
		}
	}
}

// 모든 적이 제거될 때 호출
void AHSWaveManager::OnAllEnemiesCleared(AHSEnemySpawner* SpawnManagerRef)
{
	if (CurrentWaveState == EHSWaveState::InProgress)
	{
		FHSWaveData CurrentWave = GetCurrentWaveData();
		if (CurrentWave.bRequireAllEnemiesKilled)
		{
			CompleteCurrentWave();
		}
	}
}

// 랜덤 웨이브 생성
void AHSWaveManager::GenerateRandomWaves(int32 WaveCount)
{
	WaveDataArray.Empty();
	WaveDataArray.Reserve(WaveCount);

	for (int32 i = 0; i < WaveCount; i++)
	{
		EHSWaveType WaveType = EHSWaveType::Standard;
		
		// 웨이브 타입 결정 (난이도에 따라)
		if (i % 5 == 4) // 매 5번째 웨이브는 엘리트
		{
			WaveType = EHSWaveType::Elite;
		}
		else if (i % 10 == 9) // 매 10번째 웨이브는 보스
		{
			WaveType = EHSWaveType::Boss;
		}

		FHSWaveData NewWave = GenerateRandomWave(i + 1, WaveType);
		WaveDataArray.Add(NewWave);
	}

	LogWaveInfo(FString::Printf(TEXT("랜덤 웨이브 %d개 생성 완료"), WaveCount));
}

// 단일 랜덤 웨이브 생성
FHSWaveData AHSWaveManager::GenerateRandomWave(int32 WaveNumber, EHSWaveType WaveType)
{
	FHSWaveData NewWave;
	NewWave.WaveNumber = WaveNumber;
	NewWave.WaveName = FString::Printf(TEXT("Wave %d"), WaveNumber);
	NewWave.WaveType = WaveType;

	// 웨이브 타입에 따른 설정
	switch (WaveType)
	{
		case EHSWaveType::Standard:
			NewWave.WaveDescription = TEXT("Standard wave with basic enemies.");
			NewWave.PrepareTime = 5.0f;
			NewWave.RestTime = 10.0f;
			break;

		case EHSWaveType::Elite:
			NewWave.WaveDescription = TEXT("Elite wave with stronger enemies.");
			NewWave.PrepareTime = 8.0f;
			NewWave.RestTime = 15.0f;
			break;

		case EHSWaveType::Boss:
			NewWave.WaveDescription = TEXT("Boss wave with powerful enemies.");
			NewWave.PrepareTime = 10.0f;
			NewWave.RestTime = 20.0f;
			break;

		case EHSWaveType::Swarm:
			NewWave.WaveDescription = TEXT("Swarm wave with many enemies.");
			NewWave.PrepareTime = 5.0f;
			NewWave.RestTime = 10.0f;
			break;

		default:
			break;
	}

	// 적 스폰 정보 생성
	TArray<TSubclassOf<AHSEnemyBase>> AvailableEnemies = GetAvailableEnemyClasses();
	if (AvailableEnemies.Num() > 0)
	{
		int32 EnemyTypeCount = FMath::RandRange(1, FMath::Min(3, AvailableEnemies.Num()));
		
		for (int32 i = 0; i < EnemyTypeCount; i++)
		{
			TSubclassOf<AHSEnemyBase> RandomEnemyClass = AvailableEnemies[FMath::RandRange(0, AvailableEnemies.Num() - 1)];
			FHSEnemySpawnInfo SpawnInfo = CreateRandomSpawnInfo(RandomEnemyClass, WaveNumber);
			NewWave.EnemySpawns.Add(SpawnInfo);
		}
	}

	return NewWave;
}

// 웨이브 상태 설정
void AHSWaveManager::SetWaveState(EHSWaveState NewState)
{
	if (CurrentWaveState != NewState)
	{
		CurrentWaveState = NewState;
		OnWaveStateChanged.Broadcast(NewState);
	}
}

// 웨이브 준비 시작
void AHSWaveManager::StartWavePreparation()
{
	SetWaveState(EHSWaveState::Preparing);
	
	FHSWaveData CurrentWave = GetCurrentWaveData();
	WavePreparationStartTime = GetWorld()->GetTimeSeconds();

	// 준비 시간 타이머 설정
	if (CurrentWave.PrepareTime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			WavePreparationTimer,
			this,
			&AHSWaveManager::OnWavePreparationComplete,
			CurrentWave.PrepareTime,
			false
		);

		OnWavePreparation.Broadcast(GetCurrentWaveNumber(), CurrentWave.PrepareTime);
		LogWaveInfo(FString::Printf(TEXT("웨이브 %d 준비 시작 (%.1f초)"), GetCurrentWaveNumber(), CurrentWave.PrepareTime));
	}
	else
	{
		// 즉시 시작
		OnWavePreparationComplete();
	}
}

// 현재 웨이브 시작
void AHSWaveManager::StartCurrentWave()
{
	SetWaveState(EHSWaveState::InProgress);
	
	FHSWaveData CurrentWave = GetCurrentWaveData();
	CurrentWaveStartTime = GetWorld()->GetTimeSeconds();

	// 웨이브 통계 초기화
	WaveStatistics.CurrentWave = GetCurrentWaveNumber();
	WaveStatistics.CurrentWaveKills = 0;
	WaveStatistics.CurrentWaveSpawned = 0;
	WaveStatistics.CurrentWaveTime = 0.0f;

	// 시간 제한 타이머 설정
	if (CurrentWave.TimeLimit > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			WaveTimeoutTimer,
			this,
			&AHSWaveManager::OnWaveTimeout,
			CurrentWave.TimeLimit,
			false
		);
	}

	// 적 스폰 시작
	StartWaveSpawning();

	OnWaveStarted.Broadcast(GetCurrentWaveNumber());
	LogWaveInfo(FString::Printf(TEXT("웨이브 %d 시작: %s"), GetCurrentWaveNumber(), *CurrentWave.WaveName));
}

// 현재 웨이브 업데이트
void AHSWaveManager::UpdateCurrentWave(float DeltaTime)
{
	// 웨이브 시간 업데이트
	WaveStatistics.CurrentWaveTime = GetWorld()->GetTimeSeconds() - CurrentWaveStartTime;

	// 웨이브 실패 조건 확인
	if (IsCurrentWaveFailed())
	{
		ProcessWaveFailure();
		return;
	}

	// 적 스폰 처리
	ProcessEnemySpawning();
}

// 웨이브 완료 처리
void AHSWaveManager::ProcessWaveCompletion()
{
	SetWaveState(EHSWaveState::Completed);

	// 타이머들 정리
	GetWorldTimerManager().ClearTimer(WaveTimeoutTimer);
	GetWorldTimerManager().ClearTimer(EnemySpawnTimer);

	// 통계 기록
	RecordWaveCompletion();

	// 이벤트 발송
	OnWaveCompleted.Broadcast(GetCurrentWaveNumber(), WaveStatistics.CurrentWaveTime);

	LogWaveInfo(FString::Printf(TEXT("웨이브 %d 완료! (시간: %.1f초, 처치: %d마리)"), 
		GetCurrentWaveNumber(), WaveStatistics.CurrentWaveTime, WaveStatistics.CurrentWaveKills));

	// 휴식 시간 시작
	FHSWaveData CurrentWave = GetCurrentWaveData();
	if (CurrentWave.RestTime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			RestTimer,
			this,
			&AHSWaveManager::OnRestTimerComplete,
			CurrentWave.RestTime,
			false
		);
	}
	else
	{
		OnRestTimerComplete();
	}
}

// 웨이브 실패 처리
void AHSWaveManager::ProcessWaveFailure()
{
	SetWaveState(EHSWaveState::Failed);

	// 타이머들 정리
	GetWorldTimerManager().ClearAllTimersForObject(this);

	// 현재 웨이브 적들 정리
	CleanupCurrentWaveEnemies();

	OnWaveFailed.Broadcast(GetCurrentWaveNumber());
	LogWaveInfo(FString::Printf(TEXT("웨이브 %d 실패!"), GetCurrentWaveNumber()));
}

// 웨이브 스폰 시작
void AHSWaveManager::StartWaveSpawning()
{
	CurrentSpawnInfoIndex = 0;
	CurrentSpawnCount = 0;

	if (SpawnManager)
	{
		SpawnManager->StartSpawning();
	}

	// 스폰 타이머 시작
	GetWorldTimerManager().SetTimer(
		EnemySpawnTimer,
		this,
		&AHSWaveManager::OnEnemySpawnTimer,
		0.5f, // 0.5초마다 스폰 확인
		true
	);
}

// 적 스폰 처리
void AHSWaveManager::ProcessEnemySpawning()
{
	// 스폰 매니저를 통해 적 스폰
	if (SpawnManager && SpawnManager->CanSpawnMoreEnemies())
	{
		SpawnCurrentWaveEnemies();
	}
}

// 현재 웨이브 적들 스폰
void AHSWaveManager::SpawnCurrentWaveEnemies()
{
	FHSWaveData CurrentWave = GetCurrentWaveData();
	
	// 모든 스폰 정보를 처리했는지 확인
	if (CurrentSpawnInfoIndex >= CurrentWave.EnemySpawns.Num())
	{
		return;
	}

	// 현재 스폰 정보 가져오기
	const FHSEnemySpawnInfo& SpawnInfo = CurrentWave.EnemySpawns[CurrentSpawnInfoIndex];
	
	// 스폰 확률 체크
	if (FMath::RandRange(0.0f, 1.0f) <= SpawnInfo.SpawnChance)
	{
		SpawnEnemyFromInfo(SpawnInfo);
	}

	// 다음 스폰 정보로 이동
	CurrentSpawnCount++;
	if (CurrentSpawnCount >= SpawnInfo.Count)
	{
		CurrentSpawnInfoIndex++;
		CurrentSpawnCount = 0;
	}
}

// 스폰 정보로부터 적 스폰
void AHSWaveManager::SpawnEnemyFromInfo(const FHSEnemySpawnInfo& SpawnInfo)
{
	if (!SpawnManager || !SpawnInfo.EnemyClass)
	{
		return;
	}

	if (SpawnInfo.bSpawnAsGroup)
	{
		// 그룹 스폰
		SpawnManager->SpawnEnemyGroup(SpawnInfo.Count, SpawnInfo.GroupRadius);
	}
	else
	{
		// 개별 스폰
		SpawnManager->SpawnEnemyAtRandomPoint();
	}
}

// 현재 웨이브 완료 여부 확인
bool AHSWaveManager::IsCurrentWaveComplete() const
{
	if (CurrentWaveState != EHSWaveState::InProgress)
	{
		return false;
	}

	FHSWaveData CurrentWave = GetCurrentWaveData();

	// 모든 적 제거 조건
	if (CurrentWave.bRequireAllEnemiesKilled)
	{
		return CurrentWaveEnemies.Num() == 0 && WaveStatistics.CurrentWaveSpawned > 0;
	}

	// 시간 기반 완료 (시간이 다 됨)
	if (CurrentWave.TimeLimit > 0.0f)
	{
		float ElapsedTime = GetWorld()->GetTimeSeconds() - CurrentWaveStartTime;
		return ElapsedTime >= CurrentWave.TimeLimit;
	}

	return false;
}

// 현재 웨이브 실패 여부 확인
bool AHSWaveManager::IsCurrentWaveFailed() const
{
	// 플레이어가 모두 죽었는지 확인
	int32 AlivePlayerCount = 0;
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSPlayerCharacter::StaticClass(), Players);

	for (AActor* Actor : Players)
	{
		AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(Actor);
		if (Player && !Player->IsDead())
		{
			AlivePlayerCount++;
		}
	}

	return AlivePlayerCount == 0;
}

// 웨이브 완료 기록
void AHSWaveManager::RecordWaveCompletion()
{
	WaveStatistics.CompletedWaves++;
	WaveStatistics.HighestWave = FMath::Max(WaveStatistics.HighestWave, GetCurrentWaveNumber());

	// 평균 웨이브 시간 계산
	if (WaveStatistics.CompletedWaves > 0)
	{
		WaveStatistics.AverageWaveTime = 
			(WaveStatistics.AverageWaveTime * (WaveStatistics.CompletedWaves - 1) + WaveStatistics.CurrentWaveTime) 
			/ WaveStatistics.CompletedWaves;
	}
}

// 웨이브 적 등록
void AHSWaveManager::RegisterWaveEnemy(AHSEnemyBase* Enemy)
{
	if (IsValid(Enemy) && !CurrentWaveEnemies.Contains(Enemy))
	{
		CurrentWaveEnemies.Add(Enemy);
	}
}

// 웨이브 적 등록 해제
void AHSWaveManager::UnregisterWaveEnemy(AHSEnemyBase* Enemy)
{
	CurrentWaveEnemies.RemoveSwap(Enemy);
}

// 현재 웨이브 적들 정리
void AHSWaveManager::CleanupCurrentWaveEnemies()
{
	for (AHSEnemyBase* Enemy : CurrentWaveEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}
	CurrentWaveEnemies.Empty();
}

// 스케일된 적 수 계산
int32 AHSWaveManager::CalculateScaledEnemyCount(int32 BaseCount) const
{
	float Multiplier = CalculateDifficultyMultiplier();
	
	FHSWaveData CurrentWave = GetCurrentWaveData();
	if (CurrentWave.bScaleWithPlayerCount)
	{
		int32 PlayerCount = GetActivePlayerCount();
		Multiplier *= (1.0f + (PlayerCount - 1) * CurrentWave.PlayerScaleMultiplier);
	}

	return FMath::CeilToInt(BaseCount * Multiplier);
}

// 난이도 배율 계산
float AHSWaveManager::CalculateDifficultyMultiplier() const
{
	float Multiplier = GlobalDifficultyMultiplier;
	
	// 웨이브 진행에 따른 난이도 증가
	Multiplier *= (1.0f + CurrentWaveIndex * 0.1f);

	return FMath::Clamp(Multiplier, 0.5f, 3.0f);
}

// 활성 플레이어 수 반환
int32 AHSWaveManager::GetActivePlayerCount() const
{
	int32 Count = 0;
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSPlayerCharacter::StaticClass(), Players);

	for (AActor* Actor : Players)
	{
		AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(Actor);
		if (Player && !Player->IsDead())
		{
			Count++;
		}
	}

	return FMath::Max(1, Count); // 최소 1
}

// 통계 업데이트
void AHSWaveManager::UpdateWaveStatistics()
{
	if (CurrentWaveState == EHSWaveState::InProgress)
	{
		WaveStatistics.CurrentWaveTime = GetWorld()->GetTimeSeconds() - CurrentWaveStartTime;
	}
}

// 사용 가능한 적 클래스 목록 반환
TArray<TSubclassOf<AHSEnemyBase>> AHSWaveManager::GetAvailableEnemyClasses() const
{
	TArray<TSubclassOf<AHSEnemyBase>> EnemyClasses;
	
	// 기본 적 클래스들 추가
	EnemyClasses.Add(AHSBasicMeleeEnemy::StaticClass());
	EnemyClasses.Add(AHSBasicRangedEnemy::StaticClass());

	return EnemyClasses;
}

// 랜덤 스폰 정보 생성
FHSEnemySpawnInfo AHSWaveManager::CreateRandomSpawnInfo(TSubclassOf<AHSEnemyBase> EnemyClass, int32 WaveNumber) const
{
	FHSEnemySpawnInfo SpawnInfo;
	SpawnInfo.EnemyClass = EnemyClass;
	SpawnInfo.Count = FMath::RandRange(1, FMath::Min(5, WaveNumber));
	SpawnInfo.SpawnDelay = FMath::RandRange(0.0f, 3.0f);
	SpawnInfo.SpawnInterval = FMath::RandRange(MinSpawnInterval, MaxSpawnInterval);
	SpawnInfo.SpawnChance = FMath::RandRange(0.7f, 1.0f);
	SpawnInfo.bSpawnAsGroup = FMath::RandBool();
	SpawnInfo.GroupRadius = FMath::RandRange(100.0f, 400.0f);

	return SpawnInfo;
}

// 타이머 콜백들
void AHSWaveManager::OnWavePreparationComplete()
{
	StartCurrentWave();
}

void AHSWaveManager::OnWaveTimeout()
{
	// 시간 초과로 웨이브 실패
	ProcessWaveFailure();
}

void AHSWaveManager::OnEnemySpawnTimer()
{
	ProcessEnemySpawning();
}

void AHSWaveManager::OnRestTimerComplete()
{
	StartNextWave();
}

// 디버그 정보 그리기
void AHSWaveManager::DrawWaveDebugInfo() const
{
	if (!GetWorld())
		return;

	FVector Location = GetActorLocation() + FVector(0, 0, 300);
	
	FString DebugText = FString::Printf(
		TEXT("Wave Manager\nState: %s\nWave: %d/%d\nEnemies: %d/%d\nTime: %.1f\nProgress: %.1f%%"),
		*UEnum::GetValueAsString(CurrentWaveState),
		GetCurrentWaveNumber(),
		WaveDataArray.Num(),
		WaveStatistics.CurrentWaveKills,
		WaveStatistics.CurrentWaveSpawned,
		WaveStatistics.CurrentWaveTime,
		GetCurrentWaveProgress() * 100.0f
	);
	
	DrawDebugString(GetWorld(), Location, DebugText, nullptr, FColor::Cyan, 0.0f, true);
}

// 로그 출력
void AHSWaveManager::LogWaveInfo(const FString& Message) const
{
	UE_LOG(LogTemp, Warning, TEXT("[WaveManager] %s"), *Message);
}

#if WITH_EDITOR
// 에디터 전용: 현재 웨이브 테스트
void AHSWaveManager::TestCurrentWave()
{
	if (CurrentWaveIndex >= 0 && CurrentWaveIndex < WaveDataArray.Num())
	{
		StartCurrentWave();
	}
}

// 에디터 전용: 다음 웨이브로 스킵
void AHSWaveManager::SkipToNextWave()
{
	CompleteCurrentWave();
}

// 에디터 전용: 웨이브 정보 표시
void AHSWaveManager::ShowWaveInfo()
{
	bShowDebugInfo = !bShowDebugInfo;
}

// 에디터 전용: 테스트 웨이브 생성
void AHSWaveManager::GenerateTestWaves()
{
	GenerateRandomWaves(3);
}
#endif
