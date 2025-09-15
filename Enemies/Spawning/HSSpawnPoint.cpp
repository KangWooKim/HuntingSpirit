// HuntingSpirit Game - Enemy Spawn Point Implementation
// 적이 스폰될 위치와 설정을 관리하는 클래스 구현

#include "HSSpawnPoint.h"
#include "HuntingSpirit/Enemies/Base/HSEnemyBase.h"
#include "HuntingSpirit/Enemies/Spawning/HSEnemySpawner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// 생성자 - 컴포넌트 초기화 및 기본 설정
AHSSpawnPoint::AHSSpawnPoint()
{
	// 매 프레임 틱 비활성화 (필요시에만 활성화)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 네트워킹 설정
	bReplicates = true;
	SetReplicateMovement(false); // 스폰포인트는 이동하지 않음

	// 컴포넌트 설정
	SetupComponents();

	// 기본값 초기화
	CurrentState = EHSSpawnPointState::Inactive;
	LastSpawnTime = 0.0f;
	TotalSpawnedCount = 0;
	SpawnManager = nullptr;
}

// 컴포넌트 설정 함수
void AHSSpawnPoint::SetupComponents()
{
	// 루트 컴포넌트 설정
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	RootComponent = RootSceneComponent;

	// 스폰 포인트 메시 컴포넌트
	SpawnPointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpawnPointMesh"));
	SpawnPointMesh->SetupAttachment(RootComponent);
	SpawnPointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnPointMesh->SetVisibility(false); // 게임에서는 보이지 않음

	// 스폰 반지름 컴포넌트
	SpawnRadiusComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnRadiusComponent"));
	SpawnRadiusComponent->SetupAttachment(RootComponent);
	SpawnRadiusComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnRadiusComponent->SetSphereRadius(SpawnSettings.SpawnRadius);
	SpawnRadiusComponent->SetVisibility(false);

#if WITH_EDITOR
	// 에디터에서만 보이도록 설정
	SpawnPointMesh->SetVisibility(true);
	SpawnRadiusComponent->SetVisibility(true);
#endif
}

// 게임 시작 시 초기화
void AHSSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	InitializeSpawnPoint();

	// 자동 활성화가 설정되어 있으면 활성화
	if (bAutoActivate)
	{
		ActivateSpawnPoint();
	}

	// 스폰 반지름 업데이트
	UpdateSpawnRadius();

	// 디버그 정보 표시 설정
	if (bShowDebugInfo)
	{
		SetActorTickEnabled(true);
	}
}

// 스폰 포인트 초기화
void AHSSpawnPoint::InitializeSpawnPoint()
{
	// 스폰된 적 배열 초기화
	SpawnedEnemies.Empty();
	SpawnedEnemies.Reserve(SpawnSettings.MaxSpawnedEnemies); // 메모리 최적화

	// 타이머 초기화
	GetWorldTimerManager().ClearTimer(SpawnDelayTimer);
	GetWorldTimerManager().ClearTimer(SpawnCooldownTimer);

	// 로그 출력
	LogSpawnInfo(FString::Printf(TEXT("스폰 포인트 초기화 완료: %s"), *GetName()));
}

// 매 프레임 업데이트 (디버그 정보 표시시에만)
void AHSSpawnPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 디버그 정보 그리기
	if (bShowDebugInfo)
	{
		DrawDebugInfo();
	}

	// 죽은 적들 정리 (성능 최적화를 위해 주기적으로)
	static float CleanupTimer = 0.0f;
	CleanupTimer += DeltaTime;
	if (CleanupTimer >= 2.0f) // 2초마다 정리
	{
		ClearDeadEnemies();
		CleanupTimer = 0.0f;
	}
}

// 스폰 포인트 활성화
void AHSSpawnPoint::ActivateSpawnPoint()
{
	if (CurrentState == EHSSpawnPointState::Disabled)
	{
		LogSpawnInfo(TEXT("비활성화된 스폰 포인트는 활성화할 수 없습니다."));
		return;
	}

	SetSpawnPointState(EHSSpawnPointState::Active);
	LogSpawnInfo(TEXT("스폰 포인트 활성화됨"));
}

// 스폰 포인트 비활성화
void AHSSpawnPoint::DeactivateSpawnPoint()
{
	SetSpawnPointState(EHSSpawnPointState::Inactive);
	
	// 진행 중인 타이머들 정리
	GetWorldTimerManager().ClearTimer(SpawnDelayTimer);
	GetWorldTimerManager().ClearTimer(SpawnCooldownTimer);

	LogSpawnInfo(TEXT("스폰 포인트 비활성화됨"));
}

// 적 스폰 가능 여부 확인
bool AHSSpawnPoint::CanSpawnEnemy() const
{
	// 기본 조건 검사
	if (!ValidateSpawnConditions())
	{
		return false;
	}

	// 스폰 쿨다운 확인
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastSpawnTime < SpawnSettings.SpawnCooldown)
	{
		return false;
	}

	// 최대 스폰 개수 확인
	if (GetSpawnedEnemyCount() >= SpawnSettings.MaxSpawnedEnemies)
	{
		return false;
	}

	// 스폰 확률 체크
	if (FMath::RandRange(0.0f, 1.0f) > SpawnSettings.SpawnChance)
	{
		return false;
	}

	return true;
}

// 적 스폰 실행
AHSEnemyBase* AHSSpawnPoint::SpawnEnemy()
{
	if (!CanSpawnEnemy())
	{
		return nullptr;
	}

	// 스폰 위치 찾기
	FVector SpawnLocation = FindValidSpawnLocation();
	if (SpawnLocation == FVector::ZeroVector)
	{
		LogSpawnInfo(TEXT("유효한 스폰 위치를 찾을 수 없습니다."));
		return nullptr;
	}

	// 스폰 방향 설정 (랜덤)
	FRotator SpawnRotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

	// 스폰 매개변수 설정
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 적 스폰
	AHSEnemyBase* SpawnedEnemy = GetWorld()->SpawnActor<AHSEnemyBase>(
		SpawnSettings.EnemyClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (SpawnedEnemy)
	{
		// 스폰된 적 등록
		RegisterSpawnedEnemy(SpawnedEnemy);

		// 상태 업데이트
		SetSpawnPointState(EHSSpawnPointState::Spawning);
		LastSpawnTime = GetWorld()->GetTimeSeconds();
		TotalSpawnedCount++;

		// 쿨다운 타이머 시작
		if (SpawnSettings.SpawnCooldown > 0.0f)
		{
			GetWorldTimerManager().SetTimer(
				SpawnCooldownTimer,
				this,
				&AHSSpawnPoint::OnSpawnCooldownComplete,
				SpawnSettings.SpawnCooldown,
				false
			);
		}

		// 이벤트 알림
		OnEnemySpawned.Broadcast(SpawnedEnemy, this);

		// 스폰 매니저에 알림
		if (SpawnManager)
		{
			SpawnManager->OnEnemySpawnedFromPoint(SpawnedEnemy, this);
		}

		// 상태 체크 및 업데이트
		CheckAndUpdateOccupiedState();

		LogSpawnInfo(FString::Printf(TEXT("적 스폰 성공: %s (총 %d마리)"), 
			*SpawnedEnemy->GetName(), GetSpawnedEnemyCount()));

		return SpawnedEnemy;
	}
	else
	{
		LogSpawnInfo(TEXT("적 스폰 실패"));
		return nullptr;
	}
}

// 딜레이 후 스폰
void AHSSpawnPoint::SpawnEnemyWithDelay()
{
	if (SpawnSettings.SpawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			SpawnDelayTimer,
			this,
			&AHSSpawnPoint::OnSpawnDelayComplete,
			SpawnSettings.SpawnDelay,
			false
		);
	}
	else
	{
		SpawnEnemy();
	}
}

// 강제 스폰 (조건 무시)
void AHSSpawnPoint::ForceSpawn()
{
	if (!SpawnSettings.EnemyClass)
	{
		LogSpawnInfo(TEXT("스폰할 적 클래스가 설정되지 않았습니다."));
		return;
	}

	// 강제 스폰을 위해 임시로 상태 변경
	EHSSpawnPointState OriginalState = CurrentState;
	SetSpawnPointState(EHSSpawnPointState::Active);

	AHSEnemyBase* SpawnedEnemy = SpawnEnemy();
	
	// 원래 상태로 복원 (단, 성공적으로 스폰된 경우 제외)
	if (!SpawnedEnemy && OriginalState != EHSSpawnPointState::Active)
	{
		SetSpawnPointState(OriginalState);
	}
}

// 스폰 포인트 상태 설정
void AHSSpawnPoint::SetSpawnPointState(EHSSpawnPointState NewState)
{
	if (CurrentState != NewState)
	{
		CurrentState = NewState;
		OnSpawnPointStateChanged.Broadcast(NewState);
	}
}

// 죽은 적들 정리
void AHSSpawnPoint::ClearDeadEnemies()
{
	// 성능 최적화: RemoveAll 대신 역순 순회 사용
	for (int32 i = SpawnedEnemies.Num() - 1; i >= 0; i--)
	{
		if (!IsValid(SpawnedEnemies[i]) || SpawnedEnemies[i]->IsDead())
		{
			SpawnedEnemies.RemoveAtSwap(i); // RemoveAtSwap이 RemoveAt보다 빠름
		}
	}

	// 상태 업데이트
	CheckAndUpdateOccupiedState();
}

// 스폰된 모든 적 제거
void AHSSpawnPoint::KillAllSpawnedEnemies()
{
	for (AHSEnemyBase* Enemy : SpawnedEnemies)
	{
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			Enemy->Die();
		}
	}

	// 죽은 적들 정리
	ClearDeadEnemies();
}

// 스폰 설정 변경
void AHSSpawnPoint::SetSpawnSettings(const FHSSpawnSettings& NewSettings)
{
	SpawnSettings = NewSettings;
	UpdateSpawnRadius();

	LogSpawnInfo(TEXT("스폰 설정이 업데이트되었습니다."));
}

// 적 클래스 설정
void AHSSpawnPoint::SetEnemyClass(TSubclassOf<AHSEnemyBase> NewEnemyClass)
{
	SpawnSettings.EnemyClass = NewEnemyClass;
}

// 랜덤 스폰 위치 계산
FVector AHSSpawnPoint::GetRandomSpawnLocation() const
{
	FVector BaseLocation = GetActorLocation();
	
	// 원형 범위 내 랜덤 위치 생성
	float RandomAngle = FMath::RandRange(0.0f, 2.0f * PI);
	float RandomRadius = FMath::RandRange(0.0f, SpawnSettings.SpawnRadius);
	
	FVector RandomOffset = FVector(
		RandomRadius * FMath::Cos(RandomAngle),
		RandomRadius * FMath::Sin(RandomAngle),
		0.0f
	);

	return BaseLocation + RandomOffset;
}

// 위치가 비어있는지 확인
bool AHSSpawnPoint::IsLocationClear(const FVector& Location) const
{
	// 충돌 체크를 위한 쿼리 설정
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;

	// 적 크기만큼의 구체로 충돌 체크
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(MinSpawnDistance);
	
	bool bHit = GetWorld()->OverlapBlockingTestByChannel(
		Location,
		FQuat::Identity,
		ECollisionChannel::ECC_Pawn,
		CollisionShape,
		QueryParams
	);

	return !bHit;
}

// 스폰 반지름 설정
void AHSSpawnPoint::SetSpawnRadius(float NewRadius)
{
	SpawnSettings.SpawnRadius = FMath::Max(NewRadius, 10.0f); // 최소값 보장
	UpdateSpawnRadius();
}

// 스폰 매니저 설정
void AHSSpawnPoint::SetSpawnManager(AHSEnemySpawner* InSpawnManager)
{
	SpawnManager = InSpawnManager;
}

// 스폰 반지름 업데이트
void AHSSpawnPoint::UpdateSpawnRadius()
{
	if (SpawnRadiusComponent)
	{
		SpawnRadiusComponent->SetSphereRadius(SpawnSettings.SpawnRadius);
	}
}

// 스폰 조건 검증
bool AHSSpawnPoint::ValidateSpawnConditions() const
{
	// 활성화 상태 확인
	if (CurrentState != EHSSpawnPointState::Active && CurrentState != EHSSpawnPointState::Spawning)
	{
		return false;
	}

	// 적 클래스 확인
	if (!SpawnSettings.EnemyClass)
	{
		return false;
	}

	// 월드 확인
	if (!GetWorld())
	{
		return false;
	}

	return true;
}

// 유효한 스폰 위치 찾기
FVector AHSSpawnPoint::FindValidSpawnLocation() const
{
	for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; Attempt++)
	{
		FVector CandidateLocation = GetRandomSpawnLocation();
		
		// 지면에 위치하도록 조정
		FHitResult HitResult;
		FVector StartLocation = CandidateLocation + FVector(0.0f, 0.0f, 500.0f);
		FVector EndLocation = CandidateLocation - FVector(0.0f, 0.0f, 1000.0f);
		
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		
		if (GetWorld()->LineTraceSingleByChannel(
			HitResult,
			StartLocation,
			EndLocation,
			ECollisionChannel::ECC_WorldStatic,
			QueryParams))
		{
			CandidateLocation = HitResult.Location + FVector(0.0f, 0.0f, 10.0f); // 지면에서 조금 위
		}

		// 위치가 비어있는지 확인
		if (IsLocationClear(CandidateLocation))
		{
			return CandidateLocation;
		}
	}

	// 적당한 위치를 찾지 못한 경우 기본 위치 반환
	return GetActorLocation();
}

// 스폰 딜레이 완료
void AHSSpawnPoint::OnSpawnDelayComplete()
{
	SpawnEnemy();
}

// 스폰 쿨다운 완료
void AHSSpawnPoint::OnSpawnCooldownComplete()
{
	// 쿨다운이 끝나면 다시 활성화 상태로
	if (CurrentState == EHSSpawnPointState::Cooldown)
	{
		SetSpawnPointState(EHSSpawnPointState::Active);
	}
}

// 스폰된 적 등록
void AHSSpawnPoint::RegisterSpawnedEnemy(AHSEnemyBase* Enemy)
{
	if (IsValid(Enemy))
	{
		SpawnedEnemies.AddUnique(Enemy);
		
		// 적의 죽음 이벤트에 바인딩
		Enemy->OnEnemyDeath.AddDynamic(this, &AHSSpawnPoint::OnSpawnedEnemyDeath);
	}
}

// 스폰된 적 등록 해제
void AHSSpawnPoint::UnregisterSpawnedEnemy(AHSEnemyBase* Enemy)
{
	if (IsValid(Enemy))
	{
		SpawnedEnemies.RemoveSwap(Enemy);
		
		// 이벤트 바인딩 해제
		Enemy->OnEnemyDeath.RemoveDynamic(this, &AHSSpawnPoint::OnSpawnedEnemyDeath);
	}

	CheckAndUpdateOccupiedState();
}

// 스폰된 적 죽음 처리
void AHSSpawnPoint::OnSpawnedEnemyDeath(AHSEnemyBase* DeadEnemy)
{
	UnregisterSpawnedEnemy(DeadEnemy);
	OnEnemyDied.Broadcast(DeadEnemy, this);

	// 스폰 매니저에 알림
	if (SpawnManager)
	{
		SpawnManager->OnEnemyDiedFromPoint(DeadEnemy, this);
	}

	LogSpawnInfo(FString::Printf(TEXT("스폰된 적 사망: %s (남은 적: %d마리)"), 
		*DeadEnemy->GetName(), GetSpawnedEnemyCount()));
}

// 점유 상태 확인 및 업데이트
void AHSSpawnPoint::CheckAndUpdateOccupiedState()
{
	int32 AliveEnemyCount = 0;
	for (AHSEnemyBase* Enemy : SpawnedEnemies)
	{
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			AliveEnemyCount++;
		}
	}

	// 상태 업데이트
	if (AliveEnemyCount >= SpawnSettings.MaxSpawnedEnemies)
	{
		SetSpawnPointState(EHSSpawnPointState::Occupied);
	}
	else if (CurrentState == EHSSpawnPointState::Occupied)
	{
		SetSpawnPointState(EHSSpawnPointState::Active);
	}
}

// 디버그 정보 그리기
void AHSSpawnPoint::DrawDebugInfo() const
{
	if (!GetWorld())
		return;

	FVector Location = GetActorLocation();
	
	// 스폰 반지름 그리기
	FColor RadiusColor = (CurrentState == EHSSpawnPointState::Active) ? FColor::Green : FColor::Red;
	DrawDebugSphere(GetWorld(), Location, SpawnSettings.SpawnRadius, 16, RadiusColor, false, -1.0f, 0, 2.0f);
	
	// 상태 텍스트 표시
	FString StateText = FString::Printf(TEXT("State: %s\nEnemies: %d/%d\nTotal Spawned: %d"), 
		*UEnum::GetValueAsString(CurrentState), 
		GetSpawnedEnemyCount(), 
		SpawnSettings.MaxSpawnedEnemies,
		TotalSpawnedCount);
	
	DrawDebugString(GetWorld(), Location + FVector(0, 0, 100), StateText, nullptr, FColor::White, 0.0f, true);
}

// 로그 출력
void AHSSpawnPoint::LogSpawnInfo(const FString& Message) const
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] %s"), *GetName(), *Message);
}

#if WITH_EDITOR
// 에디터 전용: 테스트 스폰
void AHSSpawnPoint::TestSpawn()
{
	if (!SpawnSettings.EnemyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("적 클래스가 설정되지 않았습니다."));
		return;
	}

	ForceSpawn();
}

// 에디터 전용: 모든 스폰된 적 제거
void AHSSpawnPoint::ClearAllSpawned()
{
	KillAllSpawnedEnemies();
}

// 에디터 전용: 스폰 반지름 표시
void AHSSpawnPoint::ShowSpawnRadius()
{
	bShowDebugInfo = !bShowDebugInfo;
	SetActorTickEnabled(bShowDebugInfo);
}
#endif
