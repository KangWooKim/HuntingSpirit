// HSLevelChunk.cpp
#include "HSLevelChunk.h"
#include "ProceduralMeshComponent.h"
#include "HSProceduralMeshGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "../Resources/HSResourceNode.h"
#include "../../Optimization/ObjectPool/HSObjectPool.h"

// 생성자
AHSLevelChunk::AHSLevelChunk()
{
    // 틱 비활성화 (필요 시에만 활성화)
    PrimaryActorTick.bCanEverTick = false;
    
    // 루트 컴포넌트 생성
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    
    // 절차적 메시 컴포넌트 생성
    TerrainMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    TerrainMeshComponent->SetupAttachment(RootComponent);
    TerrainMeshComponent->bUseAsyncCooking = true; // 비동기 쿠킹으로 성능 향상
    
    // 기본 LOD 레벨 설정
    CurrentLODLevel = 0;
    
    // 네트워크 복제 설정
    bReplicates = true;
    bAlwaysRelevant = false;
    NetCullDistanceSquared = FMath::Square(30000.0f); // 300미터 이상에서는 네트워크 컬링
}

// 게임 시작 시 호출
void AHSLevelChunk::BeginPlay()
{
    Super::BeginPlay();
    
    // 메시 생성기 초기화
    MeshGenerator = new HSProceduralMeshGenerator();
    
    // 초기 LOD 설정
    SetLODLevel(0);
}

// 매 프레임 호출
void AHSLevelChunk::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 플레이어와의 거리에 따른 LOD 자동 조정
    float DistanceToPlayer = GetDistanceToPlayer();
    
    for (int32 i = LODDistances.Num() - 1; i >= 0; i--)
    {
        if (DistanceToPlayer > LODDistances[i])
        {
            SetLODLevel(i + 1);
            break;
        }
        else if (i == 0)
        {
            SetLODLevel(0);
        }
    }
}

// 액터가 파괴될 때 호출
void AHSLevelChunk::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 청크 정리
    CleanupChunk();
    
    // 메시 생성기 삭제
    if (MeshGenerator)
    {
        delete MeshGenerator;
        MeshGenerator = nullptr;
    }
    
    Super::EndPlay(EndPlayReason);
}

// 청크 초기화 함수
void AHSLevelChunk::InitializeChunk(const FChunkData& InChunkData)
{
    ChunkData = InChunkData;
    
    // 청크 위치 설정
    SetActorLocation(ChunkData.WorldPosition);
    
    // 청크 크기에 따른 스케일 조정
    float ScaleFactor = ChunkData.ChunkSize / 5000.0f; // 기본 크기 5000 기준
    SetActorScale3D(FVector(ScaleFactor));
    
    UE_LOG(LogTemp, Log, TEXT("청크 초기화: 좌표 (%d, %d), 위치 (%s)"), 
        ChunkData.ChunkCoordinate.X, 
        ChunkData.ChunkCoordinate.Y,
        *ChunkData.WorldPosition.ToString());
}

// 청크 로드 함수
void AHSLevelChunk::LoadChunk()
{
    if (ChunkData.bIsLoaded)
    {
        UE_LOG(LogTemp, Warning, TEXT("청크가 이미 로드되어 있습니다: (%d, %d)"), 
            ChunkData.ChunkCoordinate.X, 
            ChunkData.ChunkCoordinate.Y);
        return;
    }
    
    // 청크 메시 생성
    GenerateChunkMesh();
    
    // 오브젝트 배치
    SpawnChunkObjects();
    
    // 인접 청크 연결
    ConnectToNeighborChunks();
    
    // 로드 상태 업데이트
    ChunkData.bIsLoaded = true;
    
    // 틱 활성화 (LOD 관리용)
    PrimaryActorTick.bCanEverTick = true;
    
    UE_LOG(LogTemp, Log, TEXT("청크 로드 완료: (%d, %d)"), 
        ChunkData.ChunkCoordinate.X, 
        ChunkData.ChunkCoordinate.Y);
}

// 청크 언로드 함수
void AHSLevelChunk::UnloadChunk()
{
    if (!ChunkData.bIsLoaded)
    {
        return;
    }
    
    // 청크 정리
    CleanupChunk();
    
    // 로드 상태 업데이트
    ChunkData.bIsLoaded = false;
    
    // 틱 비활성화
    PrimaryActorTick.bCanEverTick = false;
    
    UE_LOG(LogTemp, Log, TEXT("청크 언로드 완료: (%d, %d)"), 
        ChunkData.ChunkCoordinate.X, 
        ChunkData.ChunkCoordinate.Y);
}

// 청크 메시 생성 함수
void AHSLevelChunk::GenerateChunkMesh()
{
    if (!MeshGenerator || !TerrainMeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("메시 생성기 또는 메시 컴포넌트가 유효하지 않습니다"));
        return;
    }
    
    // 메시 생성기를 통해 지형 메시 생성
    MeshGenerator->GenerateTerrainMesh(
        TerrainMeshComponent,
        ChunkData.ChunkSize,
        ChunkData.HeightMap,
        CurrentLODLevel
    );
    
    // 청크 경계 블렌딩
    BlendChunkBorders();
    
    UE_LOG(LogTemp, Log, TEXT("청크 메시 생성 완료: (%d, %d)"), 
        ChunkData.ChunkCoordinate.X, 
        ChunkData.ChunkCoordinate.Y);
}

// 청크에 오브젝트 배치 함수
void AHSLevelChunk::SpawnChunkObjects()
{
    // 오브젝트 풀에서 가져오기 위한 준비
    HandleObjectPooling();
    
    // 바이옴 타입에 따른 오브젝트 배치
    switch (ChunkData.BiomeType)
    {
        case 0: // 숲 바이옴
        {
            // 나무 배치
            SpawnTreesInChunk();
            // 바위 배치
            SpawnRocksInChunk();
            // 풀 배치
            SpawnGrassInChunk();
            break;
        }
        case 1: // 사막 바이옴
        {
            // 선인장 배치
            SpawnCactiInChunk();
            // 모래 바위 배치
            SpawnDesertRocksInChunk();
            break;
        }
        case 2: // 설원 바이옴
        {
            // 눈 덮인 나무 배치
            SpawnSnowTreesInChunk();
            // 얼음 결정 배치
            SpawnIceCrystalsInChunk();
            break;
        }
        default:
            break;
    }
    
    // 자원 노드 배치
    SpawnResourceNodesInChunk();
    
    // 적 스폰 포인트 설정
    SetupEnemySpawnPoints();
}

// 인접 청크 연결 함수
void AHSLevelChunk::ConnectToNeighborChunks()
{
    // 8방향 인접 청크 확인
    TArray<FIntPoint> NeighborOffsets = {
        FIntPoint(1, 0),   // 동
        FIntPoint(-1, 0),  // 서
        FIntPoint(0, 1),   // 북
        FIntPoint(0, -1),  // 남
        FIntPoint(1, 1),   // 북동
        FIntPoint(-1, 1),  // 북서
        FIntPoint(1, -1),  // 남동
        FIntPoint(-1, -1)  // 남서
    };
    
    for (const FIntPoint& Offset : NeighborOffsets)
    {
        FIntPoint NeighborCoord = ChunkData.ChunkCoordinate + Offset;
        
        // 월드에서 인접 청크 찾기
        TArray<AActor*> FoundChunks;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHSLevelChunk::StaticClass(), FoundChunks);
        
        for (AActor* Actor : FoundChunks)
        {
            AHSLevelChunk* Chunk = Cast<AHSLevelChunk>(Actor);
            if (Chunk && Chunk->GetChunkCoordinate() == NeighborCoord)
            {
                NeighborChunks.Add(NeighborCoord, Chunk);
                break;
            }
        }
    }
}

// LOD 레벨 설정
void AHSLevelChunk::SetLODLevel(int32 NewLODLevel)
{
    if (CurrentLODLevel != NewLODLevel)
    {
        CurrentLODLevel = FMath::Clamp(NewLODLevel, 0, 3);
        
        // LOD에 따른 메시 업데이트
        UpdateMeshForLOD();
        
        // 오브젝트 표시 업데이트
        UpdateObjectVisibilityForLOD();
    }
}

// 플레이어와의 거리 계산
float AHSLevelChunk::GetDistanceToPlayer() const
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC || !PC->GetPawn())
    {
        return FLT_MAX;
    }
    
    return FVector::Dist(GetActorLocation(), PC->GetPawn()->GetActorLocation());
}

// 청크 메모리 정리
void AHSLevelChunk::CleanupChunk()
{
    // 메시 클리어
    if (TerrainMeshComponent)
    {
        TerrainMeshComponent->ClearAllMeshSections();
    }
    
    // 인스턴스드 메시 컴포넌트 정리
    for (auto& Pair : InstancedMeshComponents)
    {
        if (Pair.Value)
        {
            Pair.Value->ClearInstances();
            Pair.Value->DestroyComponent();
        }
    }
    InstancedMeshComponents.Empty();
    
    // 스폰된 액터 정리
    auto ReturnOrDestroyResourceNode = [this](AActor* Actor)
    {
        if (!Actor)
        {
            return;
        }

        if (ResourceNodePool.IsValid())
        {
            ResourceNodePool->ReturnObjectToPool(Actor);
        }
        else
        {
            Actor->Destroy();
        }
    };

    for (AActor*& Actor : SpawnedActors)
    {
        if (!Actor)
        {
            continue;
        }

        if (Actor->IsA(AHSResourceNode::StaticClass()))
        {
            const TWeakObjectPtr<AActor> ActorPtr(Actor);
            if (ActiveResourceNodes.Contains(ActorPtr))
            {
                ReturnOrDestroyResourceNode(Actor);
            }
        }
        else
        {
            Actor->Destroy();
        }

        Actor = nullptr;
    }
    SpawnedActors.Empty();
    ActiveResourceNodes.Empty();
    
    // 인접 청크 참조 클리어
    NeighborChunks.Empty();
}

// LOD에 따른 메시 업데이트
void AHSLevelChunk::UpdateMeshForLOD()
{
    if (ChunkData.bIsLoaded && MeshGenerator)
    {
        // 현재 LOD에 맞는 메시 재생성
        GenerateChunkMesh();
    }
}

// 청크 경계 블렌딩 처리
void AHSLevelChunk::BlendChunkBorders()
{
    // 인접 청크와의 경계 부분 높이 값 평균화
    for (const auto& Pair : NeighborChunks)
    {
        AHSLevelChunk* NeighborChunk = Pair.Value;
        if (NeighborChunk && NeighborChunk->IsChunkLoaded())
        {
            // 경계 정점들의 높이 값을 혼합해 이음새를 최소화하도록 메시 생성기에 위임
            if (MeshGenerator)
            {
                MeshGenerator->BlendBorderVertices(
                    ChunkData.HeightMap,
                    NeighborChunk->GetChunkData().HeightMap,
                    Pair.Key - ChunkData.ChunkCoordinate
                );
            }
        }
    }
}

// 성능 최적화를 위한 오브젝트 풀링 처리
void AHSLevelChunk::HandleObjectPooling()
{
    EnsureResourceNodePool();
}

void AHSLevelChunk::EnsureResourceNodePool()
{
    if (ResourceNodePool.IsValid())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AHSObjectPool> It(World); It; ++It)
    {
        AHSObjectPool* Pool = *It;
        if (Pool && Pool->GetPoolClass() == AHSResourceNode::StaticClass())
        {
            ResourceNodePool = Pool;
            return;
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (AHSObjectPool* NewPool = World->SpawnActor<AHSObjectPool>(AHSObjectPool::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, SpawnParams))
    {
        NewPool->InitializePool(AHSResourceNode::StaticClass(), 12, World);
        ResourceNodePool = NewPool;
    }
}

int32 AHSLevelChunk::GetHeightMapSize() const
{
    const int32 NumValues = ChunkData.HeightMap.Num();
    if (NumValues == 0)
    {
        return 0;
    }

    const int32 MapSize = FMath::RoundToInt(FMath::Sqrt(static_cast<float>(NumValues)));
    return (MapSize * MapSize == NumValues) ? MapSize : 0;
}

bool AHSLevelChunk::TryGetHeightAtMapCoordinate(int32 X, int32 Y, float& OutHeight) const
{
    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return false;
    }

    if (X < 0 || Y < 0 || X >= MapSize || Y >= MapSize)
    {
        return false;
    }

    const int32 Index = Y * MapSize + X;
    if (!ChunkData.HeightMap.IsValidIndex(Index))
    {
        return false;
    }

    OutHeight = ChunkData.HeightMap[Index];
    return true;
}

FVector AHSLevelChunk::CalculateLocalLocation(int32 X, int32 Y, float Height) const
{
    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return FVector::ZeroVector;
    }

    const float CellSize = ChunkData.ChunkSize / (MapSize - 1);
    return FVector(
        X * CellSize - ChunkData.ChunkSize * 0.5f,
        Y * CellSize - ChunkData.ChunkSize * 0.5f,
        Height
    );
}

float AHSLevelChunk::CalculateSlopeDegrees(int32 X, int32 Y) const
{
    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2 || !MeshGenerator)
    {
        return 0.0f;
    }

    const float CellSize = ChunkData.ChunkSize / (MapSize - 1);
    const FVector Normal = MeshGenerator->CalculateNormalFromHeightMap(ChunkData.HeightMap, X, Y, MapSize, CellSize).GetSafeNormal();
    const float Dot = FMath::Clamp(FVector::DotProduct(Normal, FVector::UpVector), -1.0f, 1.0f);
    return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

UInstancedStaticMeshComponent* AHSLevelChunk::GetOrCreateInstancedComponent(const FString& Key)
{
    if (UInstancedStaticMeshComponent** FoundComponent = InstancedMeshComponents.Find(Key))
    {
        if (FoundComponent && *FoundComponent)
        {
            return *FoundComponent;
        }
    }

    UInstancedStaticMeshComponent* NewComponent = NewObject<UInstancedStaticMeshComponent>(this);
    if (!NewComponent)
    {
        return nullptr;
    }

    NewComponent->SetupAttachment(RootComponent);
    NewComponent->SetMobility(EComponentMobility::Movable);
    NewComponent->RegisterComponent();
    ConfigureInstanceComponentLOD(NewComponent);

    InstancedMeshComponents.Add(Key, NewComponent);
    return NewComponent;
}

void AHSLevelChunk::ConfigureInstanceComponentLOD(UInstancedStaticMeshComponent* Component) const
{
    if (!Component)
    {
        return;
    }

    const float EndCullDistance = ChunkLoadDistance * (1.0f + FMath::Clamp<float>(CurrentLODLevel, 0.0f, 3.0f));
    Component->SetCullDistances(0.0f, EndCullDistance);
}

// 나무 배치 함수 (헬퍼 함수)
void AHSLevelChunk::SpawnTreesInChunk()
{
    UInstancedStaticMeshComponent* TreeInstances = GetOrCreateInstancedComponent(TEXT("Trees"));
    if (!TreeInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const float NormalizedDensity = FMath::Clamp(ObjectDensity, 0.05f, 1.0f);
    const int32 Step = FMath::Clamp(FMath::RoundToInt(6.0f / NormalizedDensity), 1, MapSize - 1);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope > 35.0f)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.37f + Y * 0.23f) * 0.5f);
            const float ScaleScalar = 0.9f + 0.2f * FMath::Clamp(Noise, -1.0f, 1.0f);
            const float Yaw = FMath::Fmod(static_cast<float>(X * 17 + Y * 29), 360.0f);

            TreeInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LocalLocation,
                FVector(ScaleScalar)
            ));
        }
    }
}

void AHSLevelChunk::SpawnRocksInChunk()
{
    UInstancedStaticMeshComponent* RockInstances = GetOrCreateInstancedComponent(TEXT("Rocks"));
    if (!RockInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const float Density = FMath::Clamp(ObjectDensity * 0.6f + 0.2f, 0.1f, 1.0f);
    const int32 Step = FMath::Clamp(FMath::RoundToInt(10.0f / Density), 1, MapSize - 1);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope < 12.0f || Slope > 55.0f)
            {
                continue;
            }

            if (((X + Y) & 1) != 0)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.31f + Y * 0.47f) * 0.4f);
            const float Scale = FMath::Lerp(0.8f, 1.2f, (Noise * 0.5f) + 0.5f);
            const float Yaw = FMath::Fmod(static_cast<float>(X * 23 + Y * 41), 360.0f);

            RockInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LocalLocation,
                FVector(Scale)
            ));
        }
    }
}

void AHSLevelChunk::SpawnGrassInChunk()
{
    UInstancedStaticMeshComponent* GrassInstances = GetOrCreateInstancedComponent(TEXT("Grass"));
    if (!GrassInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const float Density = FMath::Clamp(ObjectDensity * 1.5f, 0.1f, 1.0f);
    const int32 Step = FMath::Clamp(FMath::RoundToInt(4.0f / Density), 1, MapSize - 1);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope > 9.0f)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.9f + Y * 0.4f) * 0.25f);
            const float Scale = 0.75f + 0.25f * ((Noise * 0.5f) + 0.5f);
            const float Yaw = FMath::Fmod(static_cast<float>(X * 13 + Y * 17), 360.0f);

            GrassInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LocalLocation,
                FVector(Scale)
            ));
        }
    }
}

void AHSLevelChunk::SpawnCactiInChunk()
{
    UInstancedStaticMeshComponent* CactusInstances = GetOrCreateInstancedComponent(TEXT("Cacti"));
    if (!CactusInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const int32 Step = FMath::Max(1, MapSize / 6);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope > 8.0f || ((X + Y) % 3) != 0)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.2f + Y * 0.6f) * 0.3f);
            const float Scale = 0.85f + 0.3f * ((Noise * 0.5f) + 0.5f);
            const float Yaw = FMath::Fmod(static_cast<float>(X * 19 + Y * 11), 360.0f);

            CactusInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LocalLocation,
                FVector(Scale)
            ));
        }
    }
}

void AHSLevelChunk::SpawnDesertRocksInChunk()
{
    UInstancedStaticMeshComponent* DesertRockInstances = GetOrCreateInstancedComponent(TEXT("DesertRocks"));
    if (!DesertRockInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const int32 Step = FMath::Clamp(MapSize / 8, 1, MapSize - 1);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope < 6.0f || Slope > 32.0f)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.44f + Y * 0.19f) * 0.35f);
            const float Scale = 0.9f + 0.3f * FMath::Abs(Noise);
            const float Yaw = FMath::Fmod(static_cast<float>(X * 31 + Y * 7), 360.0f);

            DesertRockInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LocalLocation,
                FVector(Scale)
            ));
        }
    }
}

void AHSLevelChunk::SpawnSnowTreesInChunk()
{
    UInstancedStaticMeshComponent* SnowTreeInstances = GetOrCreateInstancedComponent(TEXT("SnowTrees"));
    if (!SnowTreeInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const float Density = FMath::Clamp(ObjectDensity * 0.8f, 0.05f, 1.0f);
    const int32 Step = FMath::Clamp(FMath::RoundToInt(7.0f / Density), 1, MapSize - 1);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope > 28.0f)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.27f + Y * 0.52f) * 0.45f);
            const float Scale = 0.95f + 0.15f * ((Noise * 0.5f) + 0.5f);
            const float Yaw = FMath::Fmod(static_cast<float>(X * 21 + Y * 33), 360.0f);

            SnowTreeInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LocalLocation,
                FVector(Scale)
            ));
        }
    }
}

void AHSLevelChunk::SpawnIceCrystalsInChunk()
{
    UInstancedStaticMeshComponent* IceCrystalInstances = GetOrCreateInstancedComponent(TEXT("IceCrystals"));
    if (!IceCrystalInstances)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const int32 Step = FMath::Clamp(MapSize / 10, 1, MapSize - 1);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            if (Height < 0.0f)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const float Noise = FMath::PerlinNoise1D((X * 0.58f + Y * 0.14f) * 0.32f);
            const float Scale = 0.7f + 0.4f * ((Noise * 0.5f) + 0.5f);
            const float Pitch = FMath::Fmod(static_cast<float>(X * 9 + Y * 5), 45.0f) - 22.5f;
            const float Yaw = FMath::Fmod(static_cast<float>(X * 17 + Y * 25), 360.0f);

            IceCrystalInstances->AddInstance(FTransform(
                FRotator(Pitch, Yaw, 0.0f),
                LocalLocation,
                FVector(Scale)
            ));
        }
    }
}

void AHSLevelChunk::SpawnResourceNodesInChunk()
{
    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    EnsureResourceNodePool();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const int32 Step = FMath::Max(1, MapSize / 5);

    for (int32 Y = Step / 2; Y < MapSize; Y += Step)
    {
        for (int32 X = Step / 2; X < MapSize; X += Step)
        {
            float Height = 0.0f;
            if (!TryGetHeightAtMapCoordinate(X, Y, Height))
            {
                continue;
            }

            const float Slope = CalculateSlopeDegrees(X, Y);
            if (Slope > 20.0f)
            {
                continue;
            }

            const FVector LocalLocation = CalculateLocalLocation(X, Y, Height);
            const FVector WorldLocation = GetActorLocation() + LocalLocation;

            AActor* SpawnedNode = nullptr;
            if (ResourceNodePool.IsValid())
            {
                SpawnedNode = ResourceNodePool->SpawnPooledObject(WorldLocation, FRotator::ZeroRotator);
            }
            else
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
                SpawnedNode = World->SpawnActor<AHSResourceNode>(AHSResourceNode::StaticClass(), WorldLocation, FRotator::ZeroRotator, SpawnParams);
            }

            if (SpawnedNode)
            {
                SpawnedActors.Add(SpawnedNode);
                ActiveResourceNodes.Add(TWeakObjectPtr<AActor>(SpawnedNode));
            }
        }
    }
}

void AHSLevelChunk::SetupEnemySpawnPoints()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const int32 MapSize = GetHeightMapSize();
    if (MapSize < 2)
    {
        return;
    }

    const TArray<FVector2D> NormalizedPoints = {
        FVector2D(0.25f, 0.25f),
        FVector2D(0.75f, 0.25f),
        FVector2D(0.25f, 0.75f),
        FVector2D(0.75f, 0.75f)
    };

    for (const FVector2D& Normalized : NormalizedPoints)
    {
        const int32 X = FMath::Clamp(FMath::RoundToInt(Normalized.X * (MapSize - 1)), 0, MapSize - 1);
        const int32 Y = FMath::Clamp(FMath::RoundToInt(Normalized.Y * (MapSize - 1)), 0, MapSize - 1);

        float Height = 0.0f;
        if (!TryGetHeightAtMapCoordinate(X, Y, Height))
        {
            continue;
        }

        if (CalculateSlopeDegrees(X, Y) > 18.0f)
        {
            continue;
        }

        const FVector LocalLocation = CalculateLocalLocation(X, Y, Height + 50.0f);
        const FVector WorldLocation = GetActorLocation() + LocalLocation;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        if (ATargetPoint* SpawnPoint = World->SpawnActor<ATargetPoint>(WorldLocation, FRotator::ZeroRotator, SpawnParams))
        {
            SpawnedActors.Add(SpawnPoint);
        }
    }
}

void AHSLevelChunk::UpdateObjectVisibilityForLOD()
{
    const bool bShowFineDetails = CurrentLODLevel <= 1;
    const bool bShowCoarseDetails = CurrentLODLevel <= 2;

    for (auto& Pair : InstancedMeshComponents)
    {
        if (UInstancedStaticMeshComponent* Component = Pair.Value)
        {
            const bool bIsGrass = Pair.Key.Contains(TEXT("Grass"));
            const bool bIsIceCrystal = Pair.Key.Contains(TEXT("Ice"));
            const bool bVisible = bIsGrass ? bShowFineDetails : (bIsIceCrystal ? bShowCoarseDetails : true);

            Component->SetVisibility(bVisible);
            Component->SetComponentTickEnabled(bVisible);
            ConfigureInstanceComponentLOD(Component);
        }
    }

    for (const TWeakObjectPtr<AActor>& ResourceNodePtr : ActiveResourceNodes)
    {
        if (AActor* ResourceActor = ResourceNodePtr.Get())
        {
            ResourceActor->SetActorHiddenInGame(CurrentLODLevel > 2);
        }
    }

    for (AActor* Actor : SpawnedActors)
    {
        if (!Actor || Actor->IsA(AHSResourceNode::StaticClass()))
        {
            continue;
        }

        Actor->SetActorHiddenInGame(CurrentLODLevel > 2);
    }
}
