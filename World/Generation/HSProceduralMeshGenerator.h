// HSProceduralMeshGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * 절차적 메시 정점 데이터
 */
struct FMeshVertexData
{
    FVector Position;
    FVector Normal;
    FVector2D UV;
    FColor Color;
    
    FMeshVertexData()
    {
        Position = FVector::ZeroVector;
        Normal = FVector::UpVector;
        UV = FVector2D::ZeroVector;
        Color = FColor::White;
    }
};

/**
 * 메시 LOD 설정
 */
struct FMeshLODSettings
{
    int32 VertexReductionFactor; // 정점 감소 비율 (1, 2, 4, 8...)
    float NormalSmoothingAngle;   // 노말 스무딩 각도
    bool bGenerateTangents;       // 탄젠트 생성 여부
    
    FMeshLODSettings()
    {
        VertexReductionFactor = 1;
        NormalSmoothingAngle = 60.0f;
        bGenerateTangents = true;
    }
};

/**
 * 절차적 메시 생성기 클래스
 * 다양한 종류의 절차적 메시를 생성하는 유틸리티 클래스
 */
class HUNTINGSPIRIT_API HSProceduralMeshGenerator
{
public:
    // 생성자 & 소멸자
    HSProceduralMeshGenerator();
    ~HSProceduralMeshGenerator();
    
    // 지형 메시 생성 함수
    void GenerateTerrainMesh(
        UProceduralMeshComponent* MeshComponent,
        float ChunkSize,
        const TArray<float>& HeightMap,
        int32 LODLevel = 0
    );
    
    // 평면 메시 생성 함수
    void GeneratePlaneMesh(
        UProceduralMeshComponent* MeshComponent,
        float Width,
        float Height,
        int32 SubdivisionsX,
        int32 SubdivisionsY
    );
    
    // 박스 메시 생성 함수
    void GenerateBoxMesh(
        UProceduralMeshComponent* MeshComponent,
        FVector BoxSize,
        int32 SubdivisionsPerFace = 0
    );
    
    // 구체 메시 생성 함수
    void GenerateSphereMesh(
        UProceduralMeshComponent* MeshComponent,
        float Radius,
        int32 LatitudeSegments = 32,
        int32 LongitudeSegments = 32
    );
    
    // 실린더 메시 생성 함수
    void GenerateCylinderMesh(
        UProceduralMeshComponent* MeshComponent,
        float Radius,
        float Height,
        int32 RadialSegments = 32,
        int32 HeightSegments = 1
    );
    
    // 청크 경계 정점 블렌딩 함수
    void BlendBorderVertices(
        TArray<float>& HeightMap,
        const TArray<float>& NeighborHeightMap,
        FIntPoint Direction
    );
    
    // 높이 맵에서 노말 계산 함수
    FVector CalculateNormalFromHeightMap(
        const TArray<float>& HeightMap,
        int32 X,
        int32 Y,
        int32 MapSize,
        float Scale
    );
    
    // LOD 설정 함수
    void SetLODSettings(int32 LODLevel, const FMeshLODSettings& Settings);
    FMeshLODSettings GetLODSettings(int32 LODLevel) const;
    
    // 메시 최적화 함수
    void OptimizeMesh(
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        float WeldThreshold = 0.1f
    );
    
    // UV 매핑 함수
    void GenerateUVMapping(
        TArray<FVector2D>& UVs,
        const TArray<FVector>& Vertices,
        float UVScale = 1.0f
    );
    
    // 탄젠트 계산 함수
    void CalculateTangents(
        const TArray<FVector>& Vertices,
        const TArray<FVector2D>& UVs,
        const TArray<int32>& Triangles,
        TArray<FProcMeshTangent>& Tangents
    );
    
    // 메시 통계 정보 가져오기
    void GetMeshStatistics(
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        int32& OutVertexCount,
        int32& OutTriangleCount,
        float& OutMemoryUsage
    );
    
private:
    // LOD 설정 맵
    TMap<int32, FMeshLODSettings> LODSettingsMap;
    
    // 메시 생성 헬퍼 함수들
    void CreateTerrainVertices(
        TArray<FVector>& OutVertices,
        TArray<FVector>& OutNormals,
        TArray<FVector2D>& OutUVs,
        const TArray<float>& HeightMap,
        float ChunkSize,
        int32 LODLevel
    );
    
    void CreateTerrainTriangles(
        TArray<int32>& OutTriangles,
        int32 GridSizeX,
        int32 GridSizeY,
        int32 LODLevel
    );
    
    // 정점 용접 함수 (중복 정점 제거)
    void WeldVertices(
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        float Threshold
    );
    
    // 삼각형 스트립 최적화
    void OptimizeTriangleStrip(TArray<int32>& Triangles);
    
    // 노말 스무딩 함수
    void SmoothNormals(
        TArray<FVector>& Normals,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        float SmoothingAngle
    );
    
    // 높이 맵 보간 함수
    float InterpolateHeight(
        const TArray<float>& HeightMap,
        float X,
        float Y,
        int32 MapSize
    );
    
    // 평면 메시 데이터 생성 헬퍼 함수
    void GeneratePlaneMeshData(
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        FVector Origin,
        FVector Right,
        FVector Up,
        float Width,
        float Height,
        int32 SubdivisionsX,
        int32 SubdivisionsY
    );
    
    // 메모리 풀 관리
    void InitializeMemoryPools();
    void CleanupMemoryPools();
    
    // 성능 카운터
    struct PerformanceStats
    {
        float LastGenerationTime;
        int32 LastVertexCount;
        int32 LastTriangleCount;
        
        PerformanceStats()
        {
            LastGenerationTime = 0.0f;
            LastVertexCount = 0;
            LastTriangleCount = 0;
        }
    } PerfStats;
    
    // 메모리 풀 (재사용을 위한 버퍼)
    TArray<FVector> VertexPool;
    TArray<FVector> NormalPool;
    TArray<FVector2D> UVPool;
    TArray<int32> TrianglePool;
    TArray<FProcMeshTangent> TangentPool;
    
    // 최대 풀 크기
    static constexpr int32 MaxPoolSize = 65536; // 64K 정점
};
