// HSLevelChunk.cpp
#include "HSLevelChunk.h"
#include "ProceduralMeshComponent.h"
#include "HSProceduralMeshGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
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
    
    // 스폰된 액터들을 오브젝트 풀로 반환
    for (AActor* Actor : SpawnedActors)
    {
        if (Actor)
        {
            // 오브젝트 풀로 반환 (구현 필요)
            Actor->Destroy();
        }
    }
    SpawnedActors.Empty();
    
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
            // 경계 정점들의 높이 값 블렌딩 (구현 상세 필요)
            // 이는 메시 생성기에서 처리하도록 위임
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
    // 오브젝트 풀 매니저 가져오기 (구현 필요)
    // 청크 로드 시 필요한 오브젝트들을 풀에서 가져와 배치
    // 청크 언로드 시 오브젝트들을 풀로 반환
}

// 나무 배치 함수 (헬퍼 함수)
void AHSLevelChunk::SpawnTreesInChunk()
{
    // 인스턴스드 메시 컴포넌트 생성 또는 가져오기
    UInstancedStaticMeshComponent* TreeInstances = nullptr;
    if (!InstancedMeshComponents.Contains("Trees"))
    {
        TreeInstances = NewObject<UInstancedStaticMeshComponent>(this);
        TreeInstances->SetupAttachment(RootComponent);
        TreeInstances->RegisterComponent();
        InstancedMeshComponents.Add("Trees", TreeInstances);
    }
    else
    {
        TreeInstances = InstancedMeshComponents["Trees"];
    }
    
    // 나무 메시 설정 (에디터에서 설정하거나 코드로 로드)
    // TreeInstances->SetStaticMesh(TreeMesh);
    
    // 밀도에 따른 나무 배치
    int32 TreeCount = FMath::RandRange(10, 50) * ObjectDensity;
    for (int32 i = 0; i < TreeCount; i++)
    {
        FVector RandomLocation = FVector(
            FMath::RandRange(-ChunkData.ChunkSize * 0.5f, ChunkData.ChunkSize * 0.5f),
            FMath::RandRange(-ChunkData.ChunkSize * 0.5f, ChunkData.ChunkSize * 0.5f),
            0.0f
        );
        
        // 높이 맵에서 Z 좌표 가져오기
        // RandomLocation.Z = GetHeightAtLocation(RandomLocation);
        
        FTransform TreeTransform(
            FRotator(0, FMath::RandRange(0, 360), 0),
            RandomLocation,
            FVector(FMath::RandRange(0.8f, 1.2f))
        );
        
        TreeInstances->AddInstance(TreeTransform);
    }
}

// 기타 헬퍼 함수들 (구현 생략)
void AHSLevelChunk::SpawnRocksInChunk() {}
void AHSLevelChunk::SpawnGrassInChunk() {}
void AHSLevelChunk::SpawnCactiInChunk() {}
void AHSLevelChunk::SpawnDesertRocksInChunk() {}
void AHSLevelChunk::SpawnSnowTreesInChunk() {}
void AHSLevelChunk::SpawnIceCrystalsInChunk() {}
void AHSLevelChunk::SpawnResourceNodesInChunk() {}
void AHSLevelChunk::SetupEnemySpawnPoints() {}
void AHSLevelChunk::UpdateObjectVisibilityForLOD() {}
