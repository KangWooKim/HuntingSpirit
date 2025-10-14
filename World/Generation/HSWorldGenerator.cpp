// HSWorldGenerator.cpp
#include "HSWorldGenerator.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"

AHSWorldGenerator::AHSWorldGenerator()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(DefaultRoot);
    
    // 기본 설정
    GenerationSettings.WorldSizeInChunks = 20;
    GenerationSettings.ChunkSize = 5000.0f;
    GenerationSettings.TerrainResolution = 64;
    GenerationSettings.bUseRandomSeed = true;
    GenerationSettings.MaxChunksToGeneratePerFrame = 1;
    GenerationSettings.ChunkUnloadDistance = 15000.0f;
    
    bIsGenerating = false;
    bBossSpawned = false;
    TotalGenerationTime = 0.0f;
    ChunksGeneratedThisFrame = 0;
}

void AHSWorldGenerator::BeginPlay()
{
    Super::BeginPlay();
    
    // 시드 설정
    if (GenerationSettings.bUseRandomSeed)
    {
        GenerationSettings.RandomSeed = FMath::Rand();
    }
    RandomStream.Initialize(GenerationSettings.RandomSeed);
    
    // 바이옴 데이터 검증
    if (AvailableBiomes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("HSWorldGenerator: 사용 가능한 바이옴이 없습니다!"));
        return;
    }
    
    // 월드 생성 시작
    StartWorldGeneration();
}

void AHSWorldGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (bIsGenerating)
    {
        TotalGenerationTime += DeltaTime;
        ChunksGeneratedThisFrame = 0;
        
        // 프레임당 청크 생성 처리
        ProcessChunkGeneration();
        
        // 플레이어 주변 청크 업데이트
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            if (APawn* PlayerPawn = PC->GetPawn())
            {
                UpdateChunksAroundPlayer(PlayerPawn->GetActorLocation());
            }
        }
        
        // 먼 청크 정리 (메모리 최적화)
        CleanupDistantChunks();
    }
}

void AHSWorldGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopWorldGeneration();

    // 모든 생성된 청크 정리
    TArray<FIntPoint> ChunkKeys;
    GeneratedChunks.GetKeys(ChunkKeys);
    for (const FIntPoint& ChunkCoord : ChunkKeys)
    {
        UnloadChunk(ChunkCoord);
    }
    GeneratedChunks.Empty();
    
    // 인스턴스 메시 컴포넌트 정리
    for (auto& ComponentPair : InstancedMeshComponents)
    {
        if (ComponentPair.Value)
        {
            ComponentPair.Value->DestroyComponent();
        }
    }
    InstancedMeshComponents.Empty();
    
    Super::EndPlay(EndPlayReason);
}

void AHSWorldGenerator::StartWorldGeneration()
{
    if (bIsGenerating)
    {
        return;
    }
    
    bIsGenerating = true;
    OnWorldGenerationProgress.Broadcast(0.0f, TEXT("월드 생성을 시작합니다..."));
    
    // 바이옴 맵 생성
    GenerateBiomeMap();
    
    // 초기 청크 생성 (플레이어 스폰 위치 주변)
    FIntPoint CenterChunk(0, 0);
    int32 InitialRadius = 3;
    
    for (int32 X = -InitialRadius; X <= InitialRadius; X++)
    {
        for (int32 Y = -InitialRadius; Y <= InitialRadius; Y++)
        {
            FIntPoint ChunkCoord(CenterChunk.X + X, CenterChunk.Y + Y);
            ChunkGenerationQueue.Enqueue(ChunkCoord);
        }
    }
    
    // 보스 스폰 위치 설정
    int32 BossDistance = FMath::Max(5, GenerationSettings.WorldSizeInChunks / 4);
    GenerationSettings.BossSpawnChunk = FIntPoint(
        RandomStream.RandRange(-BossDistance, BossDistance),
        RandomStream.RandRange(-BossDistance, BossDistance)
    );
}

void AHSWorldGenerator::StopWorldGeneration()
{
    bIsGenerating = false;
    ChunkGenerationQueue.Empty();
}

void AHSWorldGenerator::GenerateChunk(const FIntPoint& ChunkCoordinate)
{
    // 이미 생성된 청크인지 확인
    if (GeneratedChunks.Contains(ChunkCoordinate))
    {
        return;
    }
    
    FWorldChunk NewChunk;
    NewChunk.ChunkCoordinate = ChunkCoordinate;
    
    // 청크의 바이옴 결정
    FVector ChunkWorldPos = ChunkToWorldLocation(ChunkCoordinate);
    NewChunk.BiomeData = GetBiomeAtLocation(ChunkWorldPos);
    
    if (!NewChunk.BiomeData)
    {
        UE_LOG(LogTemp, Warning, TEXT("청크 %s에 대한 바이옴 데이터를 찾을 수 없습니다"), *ChunkCoordinate.ToString());
        return;
    }
    
    // 지형 메시 생성
    if (UProceduralMeshComponent* TerrainMesh = GenerateTerrainMesh(ChunkCoordinate, NewChunk.BiomeData))
    {
        // 청크에 오브젝트 스폰
        SpawnObjectsInChunk(NewChunk);

        NewChunk.bIsGenerated = true;
        NewChunk.GenerationTime = TotalGenerationTime;
        NewChunk.SpawnedComponents.Add(TerrainMesh);
        
        // 청크 저장
        GeneratedChunks.Add(ChunkCoordinate, NewChunk);
        
        // 진행 상황 업데이트
        float Progress = (float)GeneratedChunks.Num() / (float)(GenerationSettings.WorldSizeInChunks * GenerationSettings.WorldSizeInChunks);
        OnWorldGenerationProgress.Broadcast(Progress, FString::Printf(TEXT("청크 %s 생성 완료"), *ChunkCoordinate.ToString()));
        
        // 보스 스폰 체크
        if (!bBossSpawned && ChunkCoordinate == GenerationSettings.BossSpawnChunk)
        {
            SpawnBoss();
        }
    }
}

void AHSWorldGenerator::UnloadChunk(const FIntPoint& ChunkCoordinate)
{
    FWorldChunk* Chunk = GeneratedChunks.Find(ChunkCoordinate);
    if (!Chunk)
    {
        return;
    }
    
    // 스폰된 액터들 제거
    for (AActor* Actor : Chunk->SpawnedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    Chunk->SpawnedActors.Empty();

    for (TObjectPtr<UPrimitiveComponent>& Component : Chunk->SpawnedComponents)
    {
        if (Component)
        {
            Component->DestroyComponent();
        }
    }
    Chunk->SpawnedComponents.Empty();

    for (FChunkInstancedMeshEntry& Entry : Chunk->InstancedMeshEntries)
    {
        if (UHierarchicalInstancedStaticMeshComponent* InstanceComponent = Entry.Component.Get())
        {
            Entry.InstanceIndices.Sort(TGreater<int32>());
            for (int32 InstanceIndex : Entry.InstanceIndices)
            {
                const int32 InstanceCount = InstanceComponent->GetInstanceCount();
                if (InstanceIndex >= 0 && InstanceIndex < InstanceCount)
                {
                    InstanceComponent->RemoveInstance(InstanceIndex);
                }
            }

            if (InstanceComponent->GetInstanceCount() == 0)
            {
                if (UStaticMesh* Mesh = InstanceComponent->GetStaticMesh())
                {
                    InstancedMeshComponents.Remove(Mesh);
                }
                InstanceComponent->DestroyComponent();
            }
        }
    }
    Chunk->InstancedMeshEntries.Empty();

    // 청크 제거
    GeneratedChunks.Remove(ChunkCoordinate);
}

void AHSWorldGenerator::UpdateChunksAroundPlayer(const FVector& PlayerLocation)
{
    FIntPoint PlayerChunk = WorldToChunkCoordinate(PlayerLocation);
    int32 LoadRadius = FMath::CeilToInt(GenerationSettings.ChunkUnloadDistance / GenerationSettings.ChunkSize);
    
    // 플레이어 주변에 청크 생성
    for (int32 X = -LoadRadius; X <= LoadRadius; X++)
    {
        for (int32 Y = -LoadRadius; Y <= LoadRadius; Y++)
        {
            FIntPoint ChunkCoord(PlayerChunk.X + X, PlayerChunk.Y + Y);
            
            // 월드 경계 체크
            if (FMath::Abs(ChunkCoord.X) <= GenerationSettings.WorldSizeInChunks / 2 &&
                FMath::Abs(ChunkCoord.Y) <= GenerationSettings.WorldSizeInChunks / 2)
            {
                if (!GeneratedChunks.Contains(ChunkCoord))
                {
                    ChunkGenerationQueue.Enqueue(ChunkCoord);
                }
            }
        }
    }
}

FIntPoint AHSWorldGenerator::WorldToChunkCoordinate(const FVector& WorldLocation) const
{
    return FIntPoint(
        FMath::FloorToInt(WorldLocation.X / GenerationSettings.ChunkSize),
        FMath::FloorToInt(WorldLocation.Y / GenerationSettings.ChunkSize)
    );
}

FVector AHSWorldGenerator::ChunkToWorldLocation(const FIntPoint& ChunkCoordinate) const
{
    return FVector(
        ChunkCoordinate.X * GenerationSettings.ChunkSize + GenerationSettings.ChunkSize * 0.5f,
        ChunkCoordinate.Y * GenerationSettings.ChunkSize + GenerationSettings.ChunkSize * 0.5f,
        0.0f
    );
}

UHSBiomeData* AHSWorldGenerator::GetBiomeAtLocation(const FVector& WorldLocation) const
{
    if (AvailableBiomes.Num() == 0)
    {
        return nullptr;
    }
    
    // Voronoi 다이어그램을 사용한 바이옴 결정
    FVector2D Location2D(WorldLocation.X, WorldLocation.Y);
    int32 ClosestBiomeIndex = FindClosestBiomeSeed(Location2D);
    
    if (ClosestBiomeIndex >= 0 && ClosestBiomeIndex < AvailableBiomes.Num())
    {
        return AvailableBiomes[ClosestBiomeIndex];
    }
    
    return AvailableBiomes[0];
}

void AHSWorldGenerator::SpawnBoss()
{
    if (bBossSpawned || GenerationSettings.PossibleBosses.Num() == 0)
    {
        return;
    }
    
    // 랜덤하게 보스 선택
    int32 BossIndex = RandomStream.RandRange(0, GenerationSettings.PossibleBosses.Num() - 1);
    TSoftClassPtr<AActor> BossClassPtr = GenerationSettings.PossibleBosses[BossIndex];
    
    // 보스 클래스 로드
    if (!BossClassPtr.IsNull())
    {
        UClass* BossClass = BossClassPtr.LoadSynchronous();
        if (BossClass)
        {
            FVector BossSpawnLocation = ChunkToWorldLocation(GenerationSettings.BossSpawnChunk);
            BossSpawnLocation.Z += 500.0f; // 지면 위로 스폰
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            
            if (AActor* BossActor = GetWorld()->SpawnActor<AActor>(BossClass, BossSpawnLocation, FRotator::ZeroRotator, SpawnParams))
            {
                bBossSpawned = true;
                OnWorldGenerationProgress.Broadcast(1.0f, TEXT("보스가 월드에 출현했습니다!"));
                
                // 보스 주변에 특별한 효과나 환경 변화 적용 가능
            }
        }
    }
}

void AHSWorldGenerator::GenerateBiomeMap()
{
    // Voronoi 다이어그램을 위한 시드 포인트 생성
    int32 NumBiomeSeeds = FMath::Max(10, GenerationSettings.WorldSizeInChunks / 2);
    float WorldSize = GenerationSettings.WorldSizeInChunks * GenerationSettings.ChunkSize;
    
    BiomeSeedPoints.Empty();
    BiomeSeedIndices.Empty();
    
    for (int32 i = 0; i < NumBiomeSeeds; i++)
    {
        FVector2D SeedPoint(
            RandomStream.FRandRange(-WorldSize * 0.5f, WorldSize * 0.5f),
            RandomStream.FRandRange(-WorldSize * 0.5f, WorldSize * 0.5f)
        );
        
        BiomeSeedPoints.Add(SeedPoint);
        
        // 가중치 기반 바이옴 선택
        float TotalWeight = 0.0f;
        for (const UHSBiomeData* Biome : AvailableBiomes)
        {
            if (Biome)
            {
                TotalWeight += Biome->GenerationWeight;
            }
        }
        
        float RandomWeight = RandomStream.FRandRange(0.0f, TotalWeight);
        float CurrentWeight = 0.0f;
        int32 SelectedBiomeIndex = 0;
        
        for (int32 j = 0; j < AvailableBiomes.Num(); j++)
        {
            if (AvailableBiomes[j])
            {
                CurrentWeight += AvailableBiomes[j]->GenerationWeight;
                if (RandomWeight <= CurrentWeight)
                {
                    SelectedBiomeIndex = j;
                    break;
                }
            }
        }
        
        BiomeSeedIndices.Add(SelectedBiomeIndex);
    }
}

UProceduralMeshComponent* AHSWorldGenerator::GenerateTerrainMesh(const FIntPoint& ChunkCoordinate, UHSBiomeData* BiomeData)
{
    if (!BiomeData)
    {
        return nullptr;
    }

    // 프로시저럴 메시 컴포넌트 생성
    UProceduralMeshComponent* TerrainMesh = NewObject<UProceduralMeshComponent>(this, UProceduralMeshComponent::StaticClass(), NAME_None, RF_Transient);
    AddInstanceComponent(TerrainMesh);
    TerrainMesh->SetupAttachment(RootComponent);
    TerrainMesh->SetMobility(EComponentMobility::Movable);
    TerrainMesh->RegisterComponent();

    // 지형 데이터 생성
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;
    
    int32 Resolution = GenerationSettings.TerrainResolution;
    float CellSize = GenerationSettings.ChunkSize / (Resolution - 1);
    FVector ChunkWorldPos = ChunkToWorldLocation(ChunkCoordinate);
    FVector ChunkStartPos = ChunkWorldPos - FVector(GenerationSettings.ChunkSize * 0.5f, GenerationSettings.ChunkSize * 0.5f, 0.0f);
    
    // 버텍스 생성
    for (int32 Y = 0; Y < Resolution; Y++)
    {
        for (int32 X = 0; X < Resolution; X++)
        {
            FVector VertexPosition = ChunkStartPos + FVector(X * CellSize, Y * CellSize, 0.0f);
            
            // 바이옴 데이터를 사용해 높이 계산
            float Height = BiomeData->CalculateTerrainHeightAtPosition(
                FVector2D(VertexPosition.X, VertexPosition.Y), 
                GenerationSettings.RandomSeed
            );
            
            VertexPosition.Z = Height;
            Vertices.Add(VertexPosition - ChunkWorldPos); // 로컬 좌표로 변환
            
            // UV 좌표
            UVs.Add(FVector2D((float)X / (Resolution - 1), (float)Y / (Resolution - 1)));
            
            // 버텍스 컬러 (높이 기반)
            float HeightRatio = FMath::Clamp(Height / BiomeData->TerrainHeightMultiplier, 0.0f, 1.0f);
            VertexColors.Add(FColor(HeightRatio * 255, HeightRatio * 255, HeightRatio * 255, 255));
        }
    }
    
    // 삼각형 인덱스 생성
    for (int32 Y = 0; Y < Resolution - 1; Y++)
    {
        for (int32 X = 0; X < Resolution - 1; X++)
        {
            int32 TopLeft = Y * Resolution + X;
            int32 TopRight = TopLeft + 1;
            int32 BottomLeft = (Y + 1) * Resolution + X;
            int32 BottomRight = BottomLeft + 1;
            
            // 첫 번째 삼각형
            Triangles.Add(TopLeft);
            Triangles.Add(BottomLeft);
            Triangles.Add(TopRight);
            
            // 두 번째 삼각형
            Triangles.Add(TopRight);
            Triangles.Add(BottomLeft);
            Triangles.Add(BottomRight);
        }
    }
    
    // 노멀 계산
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        Normals.Add(FVector::UpVector); // 간단한 구현, 실제로는 주변 버텍스를 고려해 계산해야 함
    }
    
    // 메시 생성
    TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
    
    // 머티리얼 적용
    if (BiomeData->EnvironmentSettings.TerrainMaterials.Num() > 0)
    {
        TerrainMesh->SetMaterial(0, BiomeData->EnvironmentSettings.TerrainMaterials[0]);
    }
    
    // 콜리전 설정
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TerrainMesh->SetCollisionResponseToAllChannels(ECR_Block);
    
    TerrainMesh->SetWorldLocation(ChunkWorldPos);

    return TerrainMesh;
}

void AHSWorldGenerator::SpawnObjectsInChunk(FWorldChunk& Chunk)
{
    if (!Chunk.BiomeData)
    {
        return;
    }
    
    FVector ChunkWorldPos = ChunkToWorldLocation(Chunk.ChunkCoordinate);
    FVector ChunkStartPos = ChunkWorldPos - FVector(GenerationSettings.ChunkSize * 0.5f, GenerationSettings.ChunkSize * 0.5f, 0.0f);
    
    // 자원 노드 스폰
    TArray<FBiomeSpawnableObject> ResourcestoSpawn = Chunk.BiomeData->FilterSpawnablesByProbability(
        Chunk.BiomeData->ResourceNodes, 
        RandomStream
    );
    
    for (const FBiomeSpawnableObject& Resource : ResourcestoSpawn)
    {
        if (Resource.ActorClass.IsNull())
        {
            continue;
        }
        
        UClass* ActorClass = Resource.ActorClass.LoadSynchronous();
        if (!ActorClass)
        {
            continue;
        }
        
        int32 SpawnCount = RandomStream.RandRange(Resource.MinSpawnCount, Resource.MaxSpawnCount);
        
        for (int32 i = 0; i < SpawnCount; i++)
        {
            // 랜덤 위치 생성
            FVector SpawnLocation = ChunkStartPos + FVector(
                RandomStream.FRandRange(0.0f, GenerationSettings.ChunkSize),
                RandomStream.FRandRange(0.0f, GenerationSettings.ChunkSize),
                0.0f
            );
            
            // 높이 조정
            float Height = Chunk.BiomeData->CalculateTerrainHeightAtPosition(
                FVector2D(SpawnLocation.X, SpawnLocation.Y),
                GenerationSettings.RandomSeed
            );
            SpawnLocation.Z = Height + Resource.SpawnOffset.Z;
            
            // 액터 스폰
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            
            if (AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
            {
                Chunk.SpawnedActors.Add(SpawnedActor);
                
                // 표면 정렬
                if (Resource.bAlignToSurface)
                {
                    // 레이캐스트로 표면 노멀 찾기
                    FHitResult HitResult;
                    FVector TraceStart = SpawnLocation + FVector(0, 0, 1000);
                    FVector TraceEnd = SpawnLocation - FVector(0, 0, 1000);
                    
                    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic))
                    {
                        FRotator SurfaceRotation = HitResult.Normal.Rotation();
                        SurfaceRotation.Pitch -= 90.0f; // 노멀을 업 벡터로 변환
                        SpawnedActor->SetActorRotation(SurfaceRotation);
                    }
                }
            }
        }
    }
    
    // 환경 프롭 스폰 (인스턴스 메시로 최적화)
    TArray<FBiomeSpawnableObject> PropsToSpawn = Chunk.BiomeData->FilterSpawnablesByProbability(
        Chunk.BiomeData->EnvironmentProps, 
        RandomStream
    );
    
    // 나머지 구현은 유사한 패턴으로 진행...
}

void AHSWorldGenerator::SpawnInstancedMesh(FWorldChunk& Chunk, UStaticMesh* StaticMesh, const FTransform& Transform)
{
    if (!StaticMesh)
    {
        return;
    }
    
    // 해당 메시에 대한 인스턴스 컴포넌트 찾기 또는 생성
    UHierarchicalInstancedStaticMeshComponent* InstanceComponent = nullptr;
    
    if (InstancedMeshComponents.Contains(StaticMesh))
    {
        InstanceComponent = InstancedMeshComponents[StaticMesh];
    }
    else
    {
        // 새 인스턴스 컴포넌트 생성
        InstanceComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        AddInstanceComponent(InstanceComponent);
        InstanceComponent->SetupAttachment(RootComponent);
        InstanceComponent->SetStaticMesh(StaticMesh);
        InstanceComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        InstanceComponent->SetMobility(EComponentMobility::Movable);
        InstanceComponent->RegisterComponent();
        
        InstancedMeshComponents.Add(StaticMesh, InstanceComponent);
    }
    
    // 인스턴스 추가
    const int32 InstanceIndex = InstanceComponent->AddInstance(Transform);

    FChunkInstancedMeshEntry* Entry = Chunk.InstancedMeshEntries.FindByPredicate([InstanceComponent](const FChunkInstancedMeshEntry& Existing)
    {
        return Existing.Component == InstanceComponent;
    });

    if (!Entry)
    {
        FChunkInstancedMeshEntry& NewEntry = Chunk.InstancedMeshEntries.AddDefaulted_GetRef();
        NewEntry.Component = InstanceComponent;
        Entry = &NewEntry;
    }

    Entry->InstanceIndices.Add(InstanceIndex);
}

void AHSWorldGenerator::ProcessChunkGeneration()
{
    while (!ChunkGenerationQueue.IsEmpty() && ChunksGeneratedThisFrame < GenerationSettings.MaxChunksToGeneratePerFrame)
    {
        FIntPoint ChunkToGenerate;
        if (ChunkGenerationQueue.Dequeue(ChunkToGenerate))
        {
            GenerateChunk(ChunkToGenerate);
            ChunksGeneratedThisFrame++;
        }
    }
    
    // 모든 청크 생성 완료 체크
    if (ChunkGenerationQueue.IsEmpty() && !bBossSpawned)
    {
        // 보스 청크가 아직 생성되지 않았다면 강제 생성
        if (!GeneratedChunks.Contains(GenerationSettings.BossSpawnChunk))
        {
            GenerateChunk(GenerationSettings.BossSpawnChunk);
        }
    }
    
    if (ChunkGenerationQueue.IsEmpty() && bBossSpawned)
    {
        OnWorldGenerationComplete.Broadcast();
    }
}

void AHSWorldGenerator::CleanupDistantChunks()
{
    if (!IsValid(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
    {
        return;
    }
    
    APawn* PlayerPawn = UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetPawn();
    if (!IsValid(PlayerPawn))
    {
        return;
    }
    
    FVector PlayerLocation = PlayerPawn->GetActorLocation();
    TArray<FIntPoint> ChunksToUnload;
    
    // 먼 청크 찾기
    for (const auto& ChunkPair : GeneratedChunks)
    {
        FVector ChunkWorldPos = ChunkToWorldLocation(ChunkPair.Key);
        float Distance = FVector::Dist2D(PlayerLocation, ChunkWorldPos);
        
        if (Distance > GenerationSettings.ChunkUnloadDistance)
        {
            ChunksToUnload.Add(ChunkPair.Key);
        }
    }
    
    // 청크 언로드
    for (const FIntPoint& ChunkCoord : ChunksToUnload)
    {
        UnloadChunk(ChunkCoord);
    }
}

int32 AHSWorldGenerator::FindClosestBiomeSeed(const FVector2D& Location) const
{
    if (BiomeSeedPoints.Num() == 0)
    {
        return -1;
    }
    
    int32 ClosestIndex = 0;
    float ClosestDistance = FVector2D::DistSquared(Location, BiomeSeedPoints[0]);
    
    for (int32 i = 1; i < BiomeSeedPoints.Num(); i++)
    {
        float Distance = FVector2D::DistSquared(Location, BiomeSeedPoints[i]);
        if (Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestIndex = i;
        }
    }
    
    return BiomeSeedIndices.IsValidIndex(ClosestIndex) ? BiomeSeedIndices[ClosestIndex] : 0;
}
