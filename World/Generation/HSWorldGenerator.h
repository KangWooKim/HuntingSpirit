// HSWorldGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "HSBiomeData.h"
#include "HSWorldGenerator.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * 월드 청크 정보를 담는 구조체
 */
USTRUCT(BlueprintType)
struct FWorldChunk
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FIntPoint ChunkCoordinate;

    UPROPERTY(BlueprintReadWrite)
    UHSBiomeData* BiomeData;

    UPROPERTY(BlueprintReadWrite)
    TArray<AActor*> SpawnedActors;

    UPROPERTY(BlueprintReadWrite)
    bool bIsGenerated;

    UPROPERTY(BlueprintReadWrite)
    float GenerationTime;

    FWorldChunk()
    {
        ChunkCoordinate = FIntPoint::ZeroValue;
        BiomeData = nullptr;
        bIsGenerated = false;
        GenerationTime = 0.0f;
    }
};

/**
 * 월드 생성 설정
 */
USTRUCT(BlueprintType)
struct FWorldGenerationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1"))
    int32 WorldSizeInChunks = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "100.0"))
    float ChunkSize = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1"))
    int32 TerrainResolution = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 RandomSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bUseRandomSeed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1"))
    int32 MaxChunksToGeneratePerFrame = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1000.0"))
    float ChunkUnloadDistance = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
    FIntPoint BossSpawnChunk;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
    TArray<TSoftClassPtr<AActor>> PossibleBosses;
};

/**
 * 월드 생성 진행 상황 델리게이트
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldGenerationProgress, float, Progress, FString, StatusMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldGenerationComplete);

/**
 * 절차적 월드 생성을 담당하는 액터 클래스
 * 청크 기반 시스템으로 대규모 월드를 효율적으로 생성 및 관리
 */
UCLASS()
class HUNTINGSPIRIT_API AHSWorldGenerator : public AActor
{
    GENERATED_BODY()

public:
    AHSWorldGenerator();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // 월드 생성 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
    FWorldGenerationSettings GenerationSettings;

    // 사용 가능한 바이옴 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
    TArray<UHSBiomeData*> AvailableBiomes;

    // 월드 생성 진행 상황 이벤트
    UPROPERTY(BlueprintAssignable, Category = "World Generation")
    FOnWorldGenerationProgress OnWorldGenerationProgress;

    UPROPERTY(BlueprintAssignable, Category = "World Generation")
    FOnWorldGenerationComplete OnWorldGenerationComplete;

public:
    /**
     * 월드 생성 시작
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void StartWorldGeneration();

    /**
     * 월드 생성 중지
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void StopWorldGeneration();

    /**
     * 특정 청크 생성
     * @param ChunkCoordinate 생성할 청크 좌표
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void GenerateChunk(const FIntPoint& ChunkCoordinate);

    /**
     * 특정 청크 언로드
     * @param ChunkCoordinate 언로드할 청크 좌표
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void UnloadChunk(const FIntPoint& ChunkCoordinate);

    /**
     * 플레이어 주변 청크 업데이트
     * @param PlayerLocation 플레이어 위치
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void UpdateChunksAroundPlayer(const FVector& PlayerLocation);

    /**
     * 월드 좌표를 청크 좌표로 변환
     * @param WorldLocation 월드 위치
     * @return 청크 좌표
     */
    UFUNCTION(BlueprintPure, Category = "World Generation")
    FIntPoint WorldToChunkCoordinate(const FVector& WorldLocation) const;

    /**
     * 청크 좌표를 월드 좌표로 변환
     * @param ChunkCoordinate 청크 좌표
     * @return 월드 위치 (청크 중심)
     */
    UFUNCTION(BlueprintPure, Category = "World Generation")
    FVector ChunkToWorldLocation(const FIntPoint& ChunkCoordinate) const;

    /**
     * 특정 위치의 바이옴 가져오기
     * @param WorldLocation 월드 위치
     * @return 해당 위치의 바이옴 데이터
     */
    UFUNCTION(BlueprintPure, Category = "World Generation")
    UHSBiomeData* GetBiomeAtLocation(const FVector& WorldLocation) const;

    /**
     * 보스 스폰
     */
    UFUNCTION(BlueprintCallable, Category = "Boss")
    void SpawnBoss();

protected:
    // 생성된 청크들
    UPROPERTY()
    TMap<FIntPoint, FWorldChunk> GeneratedChunks;

    // 청크 생성 큐
    TQueue<FIntPoint> ChunkGenerationQueue;

    // 현재 생성 중인지 여부
    bool bIsGenerating;

    // 난수 스트림
    FRandomStream RandomStream;

    // 오브젝트 풀링을 위한 인스턴스 메시 컴포넌트들
    UPROPERTY()
    TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> InstancedMeshComponents;

    // 보스가 스폰되었는지 여부
    bool bBossSpawned;

protected:
    /**
     * 바이옴 맵 생성 (Voronoi 다이어그램 기반)
     */
    void GenerateBiomeMap();

    /**
     * 청크의 지형 메시 생성
     * @param ChunkCoordinate 청크 좌표
     * @param BiomeData 바이옴 데이터
     * @return 생성된 프로시저럴 메시 컴포넌트
     */
    UProceduralMeshComponent* GenerateTerrainMesh(const FIntPoint& ChunkCoordinate, UHSBiomeData* BiomeData);

    /**
     * 청크에 오브젝트 스폰
     * @param Chunk 청크 정보
     */
    void SpawnObjectsInChunk(FWorldChunk& Chunk);

    /**
     * 인스턴스 메시로 오브젝트 스폰 (최적화)
     * @param StaticMesh 스태틱 메시
     * @param Transform 트랜스폼
     */
    void SpawnInstancedMesh(UStaticMesh* StaticMesh, const FTransform& Transform);

    /**
     * 청크 간 경계 부드럽게 처리
     * @param ChunkCoordinate 청크 좌표
     */
    void SmoothChunkBoundaries(const FIntPoint& ChunkCoordinate);

    /**
     * 청크 생성 진행 처리 (비동기)
     */
    void ProcessChunkGeneration();

    /**
     * 메모리 최적화를 위한 청크 정리
     */
    void CleanupDistantChunks();

    /**
     * 특정 위치에서 가장 가까운 바이옴 시드 찾기
     * @param Location 위치
     * @return 가장 가까운 바이옴 인덱스
     */
    int32 FindClosestBiomeSeed(const FVector2D& Location) const;

private:
    // 바이옴 시드 포인트들 (Voronoi 다이어그램용)
    TArray<FVector2D> BiomeSeedPoints;
    TArray<int32> BiomeSeedIndices;

    // 성능 모니터링
    float TotalGenerationTime;
    int32 ChunksGeneratedThisFrame;
};
