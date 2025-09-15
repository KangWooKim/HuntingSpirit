// HuntingSpirit Game - Enemy Spawner Implementation
// 여러 스폰 포인트를 관리하고 적 스폰을 총괄하는 매니저 클래스 구현

#include "HSEnemySpawner.h"
// Forward declaration - HSWaveManager.h는 헤더에서 이미 전방 선언됨
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

// 생성자 - 기본 설정 및 초기화
AHSEnemySpawner::AHSEnemySpawner()
{
	// 매 프레임 틱 활성화 (성능 모니터링 및 적응형 스폰을 위해)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 네트워킹 설정
	bReplicates = true;
	SetReplicateMovement(false); // 스폰 매니저는 이동하지 않음

	// 기본값 초기화
	CurrentState = EHSSpawnManagerState::Inactive;
	CurrentSpawnStrategy = EHSSpawnStrategy::Random;
	LastSpawnTime = 0.0f;
	CurrentWaveEnemyCount = 0;
	WaveEnemiesSpawned = 0;
	WaveSpawnInterval = 1.0f;
	WaveManager = nullptr;
	LastCleanupTime = 0.0f;
	SequentialSpawnIndex = 0;
	
	// 프레임레이트 히스토리 초기화
	FrameRateHistoryIndex = 0;
	for (int32 i = 0; i < 10; i++)
	{
		FrameRateHistory[i] = 60.0f; // 기본값
	}

	// 배열 메모리 예약 (성능 최적화)
	RegisteredSpawnPoints.Reserve(20);
	ManagedEnemies.Reserve(MaxManagedEnemies);
}

// 게임 시작 시 초기화
void AHSEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	InitializeSpawnManager();

	// 자동 스폰 포인트 검색
	if (bAutoFindSpawnPoints)
	{
		FindAndRegisterSpawnPoints();
	}

	// 자동 시작 설정
	if (bAutoStart)
	{
		// 약간의 딜레이 후 시작 (다른 액터들의 초기화 대기)
		FTimerHandle DelayTimer;
		GetWorldTimerManager().SetTimer(DelayTimer, this, &AHSEnemySpawner::StartSpawning, 1.0f, false);
	}

	// 통계 업데이트 타이머 시작
	GetWorldTimerManager().SetTimer(
		StatisticsUpdateTimer,
		this,
		&AHSEnemySpawner::UpdateStatistics,
		StatisticsUpdateInterval,
		true
	);
}

// 스폰 매니저 초기화
void AHSEnemySpawner::InitializeSpawnManager()
{
	// 통계 초기화
	SpawnStatistics.Reset();

	// 현재 스폰 전략을 기본값으로 설정
	CurrentSpawnStrategy = DefaultSpawnStrategy;

	// 타이머들 정리
	GetWorldTimerManager().ClearTimer(GlobalSpawnTimer);
	GetWorldTimerManager().ClearTimer(WaveSpawnTimer);

	LogSpawnManagerInfo(TEXT("스폰 매니저 초기화 완료"));
}

// 매 프레임 업데이트
void AHSEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 성능 메트릭 업데이트
	UpdatePerformanceMetrics();

	// 스폰 로직 업데이트
	if (CurrentState == EHSSpawnManagerState::Active)
	{
		UpdateSpawning(DeltaTime);
	}

	// 주기적으로 죽은 적들 정리
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastCleanupTime >= CleanupInterval)
	{
		CleanupDeadEnemies();
		LastCleanupTime = CurrentTime;
	}

	// 디버그 정보 표시
	if (bShowDebugInfo)
	{
		FVector Location = GetActorLocation();
		FString DebugText = FString::Printf(
			TEXT("Spawn Manager\nState: %s\nEnemies: %d\nSpawn Points: %d\nFrame Rate: %.1f"),
			*UEnum::GetValueAsString(CurrentState),
			GetTotalActiveEnemies(),
			RegisteredSpawnPoints.Num(),
			GetCurrentFrameRate()
		);
		
		DrawDebugString(GetWorld(), Location + FVector(0, 0, 200), DebugText, nullptr, FColor::Yellow, 0.0f, true);
	}
}

// 스폰 시작
void AHSEnemySpawner::StartSpawning()
{
	if (RegisteredSpawnPoints.Num() == 0)
	{
		LogSpawnManagerInfo(TEXT("등록된 스폰 포인트가 없어 스폰을 시작할 수 없습니다."));
		SetManagerState(EHSSpawnManagerState::Error);
		return;
	}

	SetManagerState(EHSSpawnManagerState::Active);
	ActivateAllSpawnPoints();

	LogSpawnManagerInfo(TEXT("스폰 시작됨"));
}

// 스폰 중지
void AHSEnemySpawner::StopSpawning()
{
	SetManagerState(EHSSpawnManagerState::Inactive);
	DeactivateAllSpawnPoints();

	// 모든 타이머 정리
	GetWorldTimerManager().ClearTimer(GlobalSpawnTimer);
	GetWorldTimerManager().ClearTimer(WaveSpawnTimer);

	LogSpawnManagerInfo(TEXT("스폰 중지됨"));
}

// 스폰 일시정지
void AHSEnemySpawner::PauseSpawning()
{
	if (CurrentState == EHSSpawnManagerState::Active)
	{
		SetManagerState(EHSSpawnManagerState::Paused);
		LogSpawnManagerInfo(TEXT("스폰 일시정지"));
	}
}

// 스폰 재개
void AHSEnemySpawner::ResumeSpawning()
{
	if (CurrentState == EHSSpawnManagerState::Paused)
	{
		SetManagerState(EHSSpawnManagerState::Active);
		LogSpawnManagerInfo(TEXT("스폰 재개"));
	}
}

// 스폰 매니저 리셋
void AHSEnemySpawner::ResetSpawner()
{
	// 모든 스폰된 적 제거
	for (AHSEnemyBase* Enemy : ManagedEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}
	ManagedEnemies.Empty();

	// 모든 스폰 포인트 리셋
	for (AHSSpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (IsValid(SpawnPoint))
		{
			SpawnPoint->KillAllSpawnedEnemies();
			SpawnPoint->DeactivateSpawnPoint();
		}
	}

	// 상태 및 통계 초기화
	SetManagerState(EHSSpawnManagerState::Inactive);
	SpawnStatistics.Reset();
	
	LogSpawnManagerInfo(TEXT("스폰 매니저 리셋 완료"));
}

// 스폰 포인트 등록
void AHSEnemySpawner::RegisterSpawnPoint(AHSSpawnPoint* SpawnPoint)
{
	if (!IsValid(SpawnPoint))
	{
		return;
	}

	// 중복 등록 방지
	if (RegisteredSpawnPoints.Contains(SpawnPoint))
	{
		return;
	}

	RegisteredSpawnPoints.Add(SpawnPoint);
	SpawnPoint->SetSpawnManager(this);

	LogSpawnManagerInfo(FString::Printf(TEXT("스폰 포인트 등록: %s (총 %d개)"), 
		*SpawnPoint->GetName(), RegisteredSpawnPoints.Num()));
}

// 스폰 포인트 등록 해제
void AHSEnemySpawner::UnregisterSpawnPoint(AHSSpawnPoint* SpawnPoint)
{
	if (IsValid(SpawnPoint))
	{
		RegisteredSpawnPoints.RemoveSwap(SpawnPoint);
		SpawnPoint->SetSpawnManager(nullptr);

		LogSpawnManagerInfo(FString::Printf(TEXT("스폰 포인트 등록 해제: %s"), *SpawnPoint->GetName()));
	}
}

// 모든 스폰 포인트 활성화
void AHSEnemySpawner::ActivateAllSpawnPoints()
{
	for (AHSSpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (IsValid(SpawnPoint))
		{
			SpawnPoint->ActivateSpawnPoint();
		}
	}
}

// 모든 스폰 포인트 비활성화
void AHSEnemySpawner::DeactivateAllSpawnPoints()
{
	for (AHSSpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (IsValid(SpawnPoint))
		{
			SpawnPoint->DeactivateSpawnPoint();
		}
	}
}

// 활성화된 스폰 포인트 목록 반환
TArray<AHSSpawnPoint*> AHSEnemySpawner::GetActiveSpawnPoints() const
{
	TArray<AHSSpawnPoint*> ActivePoints;
	ActivePoints.Reserve(RegisteredSpawnPoints.Num());

	for (AHSSpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (IsValid(SpawnPoint) && SpawnPoint->IsActive())
		{
			ActivePoints.Add(SpawnPoint);
		}
	}

	return ActivePoints;
}

// 가장 가까운 스폰 포인트 찾기
AHSSpawnPoint* AHSEnemySpawner::GetNearestSpawnPoint(const FVector& Location) const
{
	AHSSpawnPoint* NearestPoint = nullptr;
	float MinDistance = FLT_MAX;

	for (AHSSpawnPoint* SpawnPoint : RegisteredSpawnPoints)
	{
		if (IsSpawnPointValid(SpawnPoint))
		{
			float Distance = FVector::Dist(Location, SpawnPoint->GetActorLocation());
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				NearestPoint = SpawnPoint;
			}
		}
	}

	return NearestPoint;
}

// 월드에서 스폰 포인트 자동 검색 및 등록
void AHSEnemySpawner::FindAndRegisterSpawnPoints()
{
	TArray<AActor*> FoundSpawnPoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSSpawnPoint::StaticClass(), FoundSpawnPoints);

	int32 RegisteredCount = 0;
	for (AActor* Actor : FoundSpawnPoints)
	{
		AHSSpawnPoint* SpawnPoint = Cast<AHSSpawnPoint>(Actor);
		if (SpawnPoint && !RegisteredSpawnPoints.Contains(SpawnPoint))
		{
			RegisterSpawnPoint(SpawnPoint);
			RegisteredCount++;
		}
	}

	LogSpawnManagerInfo(FString::Printf(TEXT("자동 검색으로 %d개의 스폰 포인트를 등록했습니다."), RegisteredCount));
}

// 특정 스폰 포인트에서 적 스폰
AHSEnemyBase* AHSEnemySpawner::SpawnEnemyAtPoint(AHSSpawnPoint* SpawnPoint)
{
	if (!IsSpawnPointValid(SpawnPoint) || !CanSpawnMoreEnemies())
	{
		return nullptr;
	}

	// 적응형 스폰 조건 확인
	if (ShouldThrottleSpawning())
	{
		return nullptr;
	}

	AHSEnemyBase* SpawnedEnemy = SpawnPoint->SpawnEnemy();
	if (SpawnedEnemy)
	{
		RegisterManagedEnemy(SpawnedEnemy);
		LastSpawnTime = GetWorld()->GetTimeSeconds();
		
		return SpawnedEnemy;
	}

	return nullptr;
}

// 랜덤 스폰 포인트에서 적 스폰
AHSEnemyBase* AHSEnemySpawner::SpawnEnemyAtRandomPoint()
{
	AHSSpawnPoint* SelectedPoint = SelectSpawnPointByStrategy();
	return SpawnEnemyAtPoint(SelectedPoint);
}

// 적 그룹 스폰
TArray<AHSEnemyBase*> AHSEnemySpawner::SpawnEnemyGroup(int32 Count, float Radius)
{
	TArray<AHSEnemyBase*> SpawnedEnemies;
	
	if (Count <= 0 || !CanSpawnMoreEnemies())
	{
		return SpawnedEnemies;
	}

	// 중심 스폰 포인트 선택
	AHSSpawnPoint* CenterPoint = SelectSpawnPointByStrategy();
	if (!CenterPoint)
	{
		return SpawnedEnemies;
	}

	// 첫 번째 적을 중심점에 스폰
	AHSEnemyBase* FirstEnemy = SpawnEnemyAtPoint(CenterPoint);
	if (FirstEnemy)
	{
		SpawnedEnemies.Add(FirstEnemy);
	}

	// 나머지 적들을 주변에 스폰
	FVector CenterLocation = CenterPoint->GetActorLocation();
	for (int32 i = 1; i < Count; i++)
	{
		if (!CanSpawnMoreEnemies())
		{
			break;
		}

		// 반지름 내 랜덤 위치에서 가장 가까운 스폰 포인트 찾기
		float RandomAngle = FMath::RandRange(0.0f, 2.0f * PI);
		float RandomRadius = FMath::RandRange(Radius * 0.3f, Radius);
		
		FVector TargetLocation = CenterLocation + FVector(
			RandomRadius * FMath::Cos(RandomAngle),
			RandomRadius * FMath::Sin(RandomAngle),
			0.0f
		);

		AHSSpawnPoint* NearestPoint = GetNearestSpawnPoint(TargetLocation);
		if (NearestPoint && NearestPoint != CenterPoint)
		{
			AHSEnemyBase* SpawnedEnemy = SpawnEnemyAtPoint(NearestPoint);
			if (SpawnedEnemy)
			{
				SpawnedEnemies.Add(SpawnedEnemy);
			}
		}
	}

	LogSpawnManagerInfo(FString::Printf(TEXT("적 그룹 스폰 완료: %d/%d마리"), SpawnedEnemies.Num(), Count));
	return SpawnedEnemies;
}

// 웨이브 스폰 시작
void AHSEnemySpawner::SpawnEnemyWave(int32 EnemyCount, float SpawnInterval)
{
	if (EnemyCount <= 0)
	{
		return;
	}

	// 웨이브 설정
	CurrentWaveEnemyCount = EnemyCount;
	WaveEnemiesSpawned = 0;
	WaveSpawnInterval = SpawnInterval;

	// 웨이브 스폰 타이머 시작
	GetWorldTimerManager().SetTimer(
		WaveSpawnTimer,
		this,
		&AHSEnemySpawner::OnWaveSpawnTimerComplete,
		WaveSpawnInterval,
		true
	);

	SetManagerState(EHSSpawnManagerState::WaveTransition);
	LogSpawnManagerInfo(FString::Printf(TEXT("웨이브 스폰 시작: %d마리, 간격 %.1f초"), EnemyCount, SpawnInterval));
}

// 총 활성화된 적 수 반환
int32 AHSEnemySpawner::GetTotalActiveEnemies() const
{
	int32 ActiveCount = 0;
	for (AHSEnemyBase* Enemy : ManagedEnemies)
	{
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			ActiveCount++;
		}
	}
	return ActiveCount;
}

// 모든 스폰된 적 목록 반환
TArray<AHSEnemyBase*> AHSEnemySpawner::GetAllSpawnedEnemies() const
{
	TArray<AHSEnemyBase*> ValidEnemies;
	ValidEnemies.Reserve(ManagedEnemies.Num());

	for (AHSEnemyBase* Enemy : ManagedEnemies)
	{
		if (IsValid(Enemy))
		{
			ValidEnemies.Add(Enemy);
		}
	}

	return ValidEnemies;
}

// 더 많은 적을 스폰할 수 있는지 확인
bool AHSEnemySpawner::CanSpawnMoreEnemies() const
{
	// 기본 조건 확인
	if (CurrentState != EHSSpawnManagerState::Active && CurrentState != EHSSpawnManagerState::WaveTransition)
	{
		return false;
	}

	// 최대 적 수 제한
	int32 CurrentActiveEnemies = GetTotalActiveEnemies();
	if (CurrentActiveEnemies >= AdaptiveSettings.MaxConcurrentEnemies)
	{
		return false;
	}

	// 동시 스폰 제한
	if (CurrentActiveEnemies >= MaxConcurrentSpawns)
	{
		return false;
	}

	// 글로벌 쿨다운 확인
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastSpawnTime < GlobalSpawnCooldown)
	{
		return false;
	}

	return true;
}

// 스폰 전략 설정
void AHSEnemySpawner::SetSpawnStrategy(EHSSpawnStrategy NewStrategy)
{
	CurrentSpawnStrategy = NewStrategy;
	LogSpawnManagerInfo(FString::Printf(TEXT("스폰 전략 변경: %s"), *UEnum::GetValueAsString(NewStrategy)));
}

// 전략에 따른 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectSpawnPointByStrategy()
{
	switch (CurrentSpawnStrategy)
	{
		case EHSSpawnStrategy::Random:
			return SelectRandomSpawnPoint();
		case EHSSpawnStrategy::PlayerBased:
			return SelectPlayerBasedSpawnPoint();
		case EHSSpawnStrategy::Sequential:
			return SelectSequentialSpawnPoint();
		case EHSSpawnStrategy::Weighted:
			return SelectWeightedSpawnPoint();
		case EHSSpawnStrategy::Distance:
			return SelectDistanceBasedSpawnPoint();
		case EHSSpawnStrategy::Pressure:
			return SelectPressureBasedSpawnPoint();
		default:
			return SelectRandomSpawnPoint();
	}
}

// 적응형 스폰 설정 업데이트
void AHSEnemySpawner::UpdateAdaptiveSpawnSettings()
{
	// 플레이어 수에 따른 조절
	if (AdaptiveSettings.bScaleWithPlayerCount)
	{
		int32 PlayerCount = GetActivePlayerCount();
		float SpawnMultiplier = CalculateSpawnMultiplier();
		
		// 최대 동시 적 수 조절
		AdaptiveSettings.MaxConcurrentEnemies = FMath::CeilToInt(
			AdaptiveSettings.MaxConcurrentEnemies * SpawnMultiplier
		);
	}

	// 성능 기반 조절
	if (AdaptiveSettings.bUsePerformanceBasedSpawn)
	{
		float CurrentFPS = GetCurrentFrameRate();
		if (CurrentFPS < AdaptiveSettings.FrameRateThreshold)
		{
			// 프레임레이트가 낮으면 스폰 수 줄이기
			AdaptiveSettings.MaxConcurrentEnemies = FMath::Max(
				AdaptiveSettings.MaxConcurrentEnemies - 5,
				10 // 최소값
			);
		}
	}
}

// 스폰 배율 계산
float AHSEnemySpawner::CalculateSpawnMultiplier() const
{
	float Multiplier = 1.0f;

	// 플레이어 수 기반 배율
	if (AdaptiveSettings.bScaleWithPlayerCount)
	{
		int32 PlayerCount = GetActivePlayerCount();
		Multiplier *= (1.0f + (PlayerCount - 1) * AdaptiveSettings.SpawnMultiplierPerPlayer);
	}

	return FMath::Clamp(Multiplier, 0.1f, 5.0f); // 최소/최대 제한
}

// 스폰 제한 여부 확인
bool AHSEnemySpawner::ShouldThrottleSpawning() const
{
	// 성능 기반 제한
	if (AdaptiveSettings.bUsePerformanceBasedSpawn)
	{
		float CurrentFPS = GetCurrentFrameRate();
		if (CurrentFPS < AdaptiveSettings.FrameRateThreshold)
		{
			return true;
		}
	}

	// 거리 기반 제한
	if (AdaptiveSettings.bUseDistanceBasedSpawn)
	{
		// 플레이어들과의 거리 확인 로직은 각 스폰 포인트에서 처리
	}

	return false;
}

// 스폰 포인트에서 적이 스폰될 때 호출
void AHSEnemySpawner::OnEnemySpawnedFromPoint(AHSEnemyBase* SpawnedEnemy, AHSSpawnPoint* SpawnPoint)
{
	if (IsValid(SpawnedEnemy))
	{
		// 중복 등록 방지
		if (!ManagedEnemies.Contains(SpawnedEnemy))
		{
			RegisterManagedEnemy(SpawnedEnemy);
		}

		OnEnemySpawnedFromManager.Broadcast(SpawnedEnemy, SpawnPoint);
	}
}

// 스폰 포인트에서 적이 죽을 때 호출
void AHSEnemySpawner::OnEnemyDiedFromPoint(AHSEnemyBase* DeadEnemy, AHSSpawnPoint* SpawnPoint)
{
	UnregisterManagedEnemy(DeadEnemy);
	OnEnemyDiedFromManager.Broadcast(DeadEnemy, SpawnPoint);

	// 모든 적이 죽었는지 확인
	if (GetTotalActiveEnemies() == 0)
	{
		OnAllEnemiesCleared.Broadcast(this);
	}
}

// 적 죽음 이벤트 처리 (델리게이트 바인딩용)
void AHSEnemySpawner::OnEnemyDied(AHSEnemyBase* DeadEnemy)
{
	// SpawnPoint 정보 없이 처리
	OnEnemyDiedFromPoint(DeadEnemy, nullptr);
}

// 웨이브 매니저 설정
void AHSEnemySpawner::SetWaveManager(AHSWaveManager* InWaveManager)
{
	WaveManager = InWaveManager;
}

// 매니저 상태 설정
void AHSEnemySpawner::SetManagerState(EHSSpawnManagerState NewState)
{
	if (CurrentState != NewState)
	{
		CurrentState = NewState;
		OnSpawnManagerStateChanged.Broadcast(NewState);
	}
}

// 스폰 로직 업데이트
void AHSEnemySpawner::UpdateSpawning(float DeltaTime)
{
	// 웨이브 스폰 중이 아니고, 자동 스폰이 필요한 경우
	if (CurrentState == EHSSpawnManagerState::Active && CanSpawnMoreEnemies())
	{
		// 주기적으로 적 스폰
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastSpawnTime >= GlobalSpawnCooldown)
		{
			SpawnEnemyAtRandomPoint();
		}
	}
}

// 랜덤 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectRandomSpawnPoint() const
{
	TArray<AHSSpawnPoint*> ValidPoints = GetActiveSpawnPoints();
	if (ValidPoints.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, ValidPoints.Num() - 1);
		return ValidPoints[RandomIndex];
	}
	return nullptr;
}

// 플레이어 기반 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectPlayerBasedSpawnPoint() const
{
	FVector PlayerCenter = GetAveragePlayerLocation();
	if (PlayerCenter != FVector::ZeroVector)
	{
		// 플레이어들로부터 적당한 거리의 스폰 포인트 찾기
		TArray<AHSSpawnPoint*> ValidPoints = GetActiveSpawnPoints();
		TArray<AHSSpawnPoint*> SuitablePoints;

		for (AHSSpawnPoint* Point : ValidPoints)
		{
			float Distance = FVector::Dist(Point->GetActorLocation(), PlayerCenter);
			if (Distance >= AdaptiveSettings.MinPlayerDistance && Distance <= AdaptiveSettings.MaxPlayerDistance)
			{
				SuitablePoints.Add(Point);
			}
		}

		if (SuitablePoints.Num() > 0)
		{
			int32 RandomIndex = FMath::RandRange(0, SuitablePoints.Num() - 1);
			return SuitablePoints[RandomIndex];
		}
	}

	return SelectRandomSpawnPoint(); // 폴백
}

// 순차적 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectSequentialSpawnPoint() const
{
	TArray<AHSSpawnPoint*> ValidPoints = GetActiveSpawnPoints();
	if (ValidPoints.Num() > 0)
	{
		AHSSpawnPoint* SelectedPoint = ValidPoints[SequentialSpawnIndex % ValidPoints.Num()];
		const_cast<AHSEnemySpawner*>(this)->SequentialSpawnIndex++; // mutable 대신 const_cast 사용
		return SelectedPoint;
	}
	return nullptr;
}

// 가중치 기반 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectWeightedSpawnPoint() const
{
	TArray<AHSSpawnPoint*> ValidPoints = GetActiveSpawnPoints();
	TArray<float> Weights;
	Weights.Reserve(ValidPoints.Num());

	// 각 스폰 포인트의 가중치 계산
	for (AHSSpawnPoint* Point : ValidPoints)
	{
		float Weight = CalculateSpawnPointWeight(Point);
		Weights.Add(Weight);
	}

	// 가중치 기반 선택
	if (Weights.Num() > 0)
	{
		float TotalWeight = 0.0f;
		for (float Weight : Weights)
		{
			TotalWeight += Weight;
		}

		if (TotalWeight > 0.0f)
		{
			float RandomValue = FMath::RandRange(0.0f, TotalWeight);
			float CurrentWeight = 0.0f;

			for (int32 i = 0; i < ValidPoints.Num(); i++)
			{
				CurrentWeight += Weights[i];
				if (RandomValue <= CurrentWeight)
				{
					return ValidPoints[i];
				}
			}
		}
	}

	return SelectRandomSpawnPoint(); // 폴백
}

// 거리 기반 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectDistanceBasedSpawnPoint() const
{
	// 플레이어로부터 가장 먼 스폰 포인트 선택
	FVector PlayerCenter = GetAveragePlayerLocation();
	if (PlayerCenter == FVector::ZeroVector)
	{
		return SelectRandomSpawnPoint();
	}

	TArray<AHSSpawnPoint*> ValidPoints = GetActiveSpawnPoints();
	AHSSpawnPoint* FarthestPoint = nullptr;
	float MaxDistance = 0.0f;

	for (AHSSpawnPoint* Point : ValidPoints)
	{
		float Distance = FVector::Dist(Point->GetActorLocation(), PlayerCenter);
		if (Distance > MaxDistance && Distance >= AdaptiveSettings.MinPlayerDistance)
		{
			MaxDistance = Distance;
			FarthestPoint = Point;
		}
	}

	return FarthestPoint ? FarthestPoint : SelectRandomSpawnPoint();
}

// 압박 기반 스폰 포인트 선택
AHSSpawnPoint* AHSEnemySpawner::SelectPressureBasedSpawnPoint() const
{
	// 가장 적이 적은 스폰 포인트 선택
	TArray<AHSSpawnPoint*> ValidPoints = GetActiveSpawnPoints();
	AHSSpawnPoint* LeastBusyPoint = nullptr;
	int32 MinEnemyCount = INT_MAX;

	for (AHSSpawnPoint* Point : ValidPoints)
	{
		int32 EnemyCount = Point->GetSpawnedEnemyCount();
		if (EnemyCount < MinEnemyCount)
		{
			MinEnemyCount = EnemyCount;
			LeastBusyPoint = Point;
		}
	}

	return LeastBusyPoint ? LeastBusyPoint : SelectRandomSpawnPoint();
}

// 관리되는 적 등록
void AHSEnemySpawner::RegisterManagedEnemy(AHSEnemyBase* Enemy)
{
	if (IsValid(Enemy) && !ManagedEnemies.Contains(Enemy))
	{
		ManagedEnemies.Add(Enemy);
		
		// 적의 죽음 이벤트에 바인딩
		Enemy->OnEnemyDeath.AddDynamic(this, &AHSEnemySpawner::OnEnemyDied);
	}
}

// 관리되는 적 등록 해제
void AHSEnemySpawner::UnregisterManagedEnemy(AHSEnemyBase* Enemy)
{
	if (IsValid(Enemy))
	{
		ManagedEnemies.RemoveSwap(Enemy);
		
		// 이벤트 바인딩 해제
		Enemy->OnEnemyDeath.RemoveDynamic(this, &AHSEnemySpawner::OnEnemyDied);
	}
}

// 죽은 적들 정리
void AHSEnemySpawner::CleanupDeadEnemies()
{
	// 역순으로 순회하며 죽은 적들 제거
	for (int32 i = ManagedEnemies.Num() - 1; i >= 0; i--)
	{
		if (!IsValid(ManagedEnemies[i]) || ManagedEnemies[i]->IsDead())
		{
			ManagedEnemies.RemoveAtSwap(i);
		}
	}
}

// 근처 플레이어들 찾기
TArray<AHSPlayerCharacter*> AHSEnemySpawner::GetNearbyPlayers(const FVector& Location, float Radius) const
{
	TArray<AHSPlayerCharacter*> NearbyPlayers;
	
	TArray<AActor*> FoundPlayers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSPlayerCharacter::StaticClass(), FoundPlayers);

	for (AActor* Actor : FoundPlayers)
	{
		AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(Actor);
		if (Player && FVector::Dist(Player->GetActorLocation(), Location) <= Radius)
		{
			NearbyPlayers.Add(Player);
		}
	}

	return NearbyPlayers;
}

// 플레이어들의 평균 위치 계산
FVector AHSEnemySpawner::GetAveragePlayerLocation() const
{
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSPlayerCharacter::StaticClass(), Players);

	if (Players.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FVector Sum = FVector::ZeroVector;
	for (AActor* Player : Players)
	{
		Sum += Player->GetActorLocation();
	}

	return Sum / Players.Num();
}

// 활성화된 플레이어 수 반환
int32 AHSEnemySpawner::GetActivePlayerCount() const
{
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSPlayerCharacter::StaticClass(), Players);

	int32 ActiveCount = 0;
	for (AActor* Actor : Players)
	{
		AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(Actor);
		if (Player && !Player->IsDead())
		{
			ActiveCount++;
		}
	}

	return ActiveCount;
}

// 통계 업데이트
void AHSEnemySpawner::UpdateStatistics()
{
	SpawnStatistics.CurrentAlive = GetTotalActiveEnemies();
	SpawnStatistics.ActiveSpawnPoints = GetActiveSpawnPoints().Num();
	
	// 효율성 계산
	if (SpawnStatistics.TotalSpawned > 0)
	{
		SpawnStatistics.SpawnEfficiency = (float)SpawnStatistics.TotalKilled / SpawnStatistics.TotalSpawned;
	}
}

// 성능 메트릭 업데이트
void AHSEnemySpawner::UpdatePerformanceMetrics()
{
	// 프레임레이트 히스토리 업데이트
	float CurrentFPS = GetCurrentFrameRate();
	FrameRateHistory[FrameRateHistoryIndex] = CurrentFPS;
	FrameRateHistoryIndex = (FrameRateHistoryIndex + 1) % 10;
}

// 현재 프레임레이트 반환
float AHSEnemySpawner::GetCurrentFrameRate() const
{
	if (GetWorld())
	{
		return 1.0f / GetWorld()->GetDeltaSeconds();
	}
	return 60.0f; // 기본값
}

// 스폰 포인트 유효성 검사
bool AHSEnemySpawner::IsSpawnPointValid(AHSSpawnPoint* SpawnPoint) const
{
	return IsValid(SpawnPoint) && SpawnPoint->IsActive() && SpawnPoint->CanSpawnEnemy();
}

// 스폰 포인트 가중치 계산
float AHSEnemySpawner::CalculateSpawnPointWeight(AHSSpawnPoint* SpawnPoint) const
{
	if (!IsValid(SpawnPoint))
	{
		return 0.0f;
	}

	float Weight = 1.0f;

	// 스폰된 적 수에 따른 가중치 감소
	int32 SpawnedCount = SpawnPoint->GetSpawnedEnemyCount();
	Weight *= FMath::Max(0.1f, 1.0f - (SpawnedCount * 0.2f));

	// 플레이어 거리에 따른 가중치 조절
	FVector PlayerCenter = GetAveragePlayerLocation();
	if (PlayerCenter != FVector::ZeroVector)
	{
		float Distance = FVector::Dist(SpawnPoint->GetActorLocation(), PlayerCenter);
		if (Distance < AdaptiveSettings.MinPlayerDistance)
		{
			Weight *= 0.1f; // 너무 가까우면 가중치 크게 감소
		}
		else if (Distance > AdaptiveSettings.MaxPlayerDistance)
		{
			Weight *= 0.5f; // 너무 멀어도 가중치 감소
		}
	}

	return Weight;
}

// 웨이브 스폰 타이머 완료
void AHSEnemySpawner::OnWaveSpawnTimerComplete()
{
	if (WaveEnemiesSpawned < CurrentWaveEnemyCount)
	{
		AHSEnemyBase* SpawnedEnemy = SpawnEnemyAtRandomPoint();
		if (SpawnedEnemy)
		{
			WaveEnemiesSpawned++;
		}

		// 웨이브 완료 확인
		if (WaveEnemiesSpawned >= CurrentWaveEnemyCount)
		{
			GetWorldTimerManager().ClearTimer(WaveSpawnTimer);
			SetManagerState(EHSSpawnManagerState::Active);
			
			LogSpawnManagerInfo(FString::Printf(TEXT("웨이브 스폰 완료: %d마리"), WaveEnemiesSpawned));
		}
	}
}

// 로그 출력
void AHSEnemySpawner::LogSpawnManagerInfo(const FString& Message) const
{
	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] %s"), *Message);
}

#if WITH_EDITOR
// 에디터 전용: 테스트 웨이브 스폰
void AHSEnemySpawner::TestSpawnWave()
{
	SpawnEnemyWave(5, 2.0f);
}

// 에디터 전용: 모든 적 제거
void AHSEnemySpawner::ClearAllEnemies()
{
	for (AHSEnemyBase* Enemy : ManagedEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}
	ManagedEnemies.Empty();
}

// 에디터 전용: 스폰 매니저 정보 표시
void AHSEnemySpawner::ShowSpawnManagerInfo()
{
	bShowDebugInfo = !bShowDebugInfo;
}

// 에디터 전용: 스폰 포인트 검증
void AHSEnemySpawner::ValidateSpawnPoints()
{
	int32 ValidCount = 0;
	for (AHSSpawnPoint* Point : RegisteredSpawnPoints)
	{
		if (IsSpawnPointValid(Point))
		{
			ValidCount++;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("스폰 포인트 검증 결과: %d/%d 유효"), ValidCount, RegisteredSpawnPoints.Num());
}
#endif
