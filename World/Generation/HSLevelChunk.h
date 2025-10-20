// HSLevelChunk.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "HSLevelChunk.generated.h"

// 전방 선언
class UProceduralMeshComponent;
class HSProceduralMeshGenerator;
class UInstancedStaticMeshComponent;
class AHSObjectPool;
class AHSResourceNode;

/**
 * 청크 데이터 구조체
 * 청크의 기본 정보를 저장
 */
USTRUCT(BlueprintType)
struct FChunkData : public FTableRowBase
{
    GENERATED_BODY()

    // 청크의 그리드 좌표
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint ChunkCoordinate;
    
    // 청크의 월드 위치
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldPosition;
    
    // 청크의 크기 (한 변의 길이)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChunkSize = 5000.0f;
    
    // 청크의 높이 맵 데이터
    UPROPERTY()
    TArray<float> HeightMap;
    
    // 청크의 바이옴 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BiomeType = 0;
    
    // 청크의 난이도 레벨
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DifficultyLevel = 1;
    
    // 청크가 로드되었는지 여부
    UPROPERTY(BlueprintReadOnly)
    bool bIsLoaded = false;
    
    FChunkData()
    {
        ChunkCoordinate = FIntPoint(0, 0);
        WorldPosition = FVector::ZeroVector;
        ChunkSize = 5000.0f;
        BiomeType = 0;
        DifficultyLevel = 1;
        bIsLoaded = false;
    }
};

/**
 * 레벨 청크 액터 클래스
 * 월드의 한 부분(청크)를 관리하는 클래스
 */
UCLASS()
class HUNTINGSPIRIT_API AHSLevelChunk : public AActor
{
    GENERATED_BODY()
    
public:
    // 생성자
    AHSLevelChunk();

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;
    
    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;
    
    // 액터가 파괴될 때 호출
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // 청크 초기화 함수
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void InitializeChunk(const FChunkData& InChunkData);
    
    // 청크 로드 함수
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void LoadChunk();
    
    // 청크 언로드 함수
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void UnloadChunk();
    
    // 청크 메시 생성 함수
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void GenerateChunkMesh();
    
    // 청크에 오브젝트 배치 함수
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void SpawnChunkObjects();
    
    // 인접 청크 연결 함수
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void ConnectToNeighborChunks();
    
    // 청크 데이터 가져오기
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    FChunkData GetChunkData() const { return ChunkData; }
    
    // 청크 좌표 가져오기
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    FIntPoint GetChunkCoordinate() const { return ChunkData.ChunkCoordinate; }
    
    // 청크가 로드되었는지 확인
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    bool IsChunkLoaded() const { return ChunkData.bIsLoaded; }
    
    // LOD 레벨 설정
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void SetLODLevel(int32 NewLODLevel);
    
    // 플레이어와의 거리 계산
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    float GetDistanceToPlayer() const;
    
protected:
    // 청크 데이터
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
    FChunkData ChunkData;
    
    // 절차적 메시 컴포넌트 (지형용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
    UProceduralMeshComponent* TerrainMeshComponent;
    
    // 인스턴스드 스태틱 메시 컴포넌트 (오브젝트 배치용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
    TMap<FString, UInstancedStaticMeshComponent*> InstancedMeshComponents;
    
    // 인접 청크 참조
    UPROPERTY()
    TMap<FIntPoint, AHSLevelChunk*> NeighborChunks;
    
    // 현재 LOD 레벨
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
    int32 CurrentLODLevel;
    
    // 청크 내 스폰된 액터들
    UPROPERTY()
    TArray<AActor*> SpawnedActors;
    
    // 메시 생성기 참조
    HSProceduralMeshGenerator* MeshGenerator;
    
    // 청크 로드/언로드 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk Settings")
    float ChunkLoadDistance = 15000.0f;
    
    // 청크 LOD 거리 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk Settings")
    TArray<float> LODDistances = {5000.0f, 10000.0f, 20000.0f};
    
    // 오브젝트 밀도 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ObjectDensity = 0.5f;
    
private:
    // 청크 메모리 정리
    void CleanupChunk();
    
    // LOD에 따른 메시 업데이트
    void UpdateMeshForLOD();
    
    // 청크 경계 블렌딩 처리
    void BlendChunkBorders();
    
    // 성능 최적화를 위한 오브젝트 풀링 처리
    void HandleObjectPooling();
    
    // HeightMap 및 좌표 계산 헬퍼
    int32 GetHeightMapSize() const;
    bool TryGetHeightAtMapCoordinate(int32 X, int32 Y, float& OutHeight) const;
    FVector CalculateLocalLocation(int32 X, int32 Y, float Height) const;
    float CalculateSlopeDegrees(int32 X, int32 Y) const;
    
    // 인스턴스드 메시 헬퍼
    UInstancedStaticMeshComponent* GetOrCreateInstancedComponent(const FString& Key);
    void ConfigureInstanceComponentLOD(UInstancedStaticMeshComponent* Component) const;
    
    // 리소스 노드 풀 관리
    void EnsureResourceNodePool();
    
    // 오브젝트 배치 헬퍼 함수들
    void SpawnTreesInChunk();
    void SpawnRocksInChunk();
    void SpawnGrassInChunk();
    void SpawnCactiInChunk();
    void SpawnDesertRocksInChunk();
    void SpawnSnowTreesInChunk();
    void SpawnIceCrystalsInChunk();
    void SpawnResourceNodesInChunk();
    void SetupEnemySpawnPoints();
    void UpdateObjectVisibilityForLOD();
    
    // 풀에서 사용 중인 리소스 노드 추적
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> ActiveResourceNodes;
    
    UPROPERTY()
    TWeakObjectPtr<AHSObjectPool> ResourceNodePool;
};
