// HSProceduralMeshGenerator.cpp
#include "HSProceduralMeshGenerator.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Engine.h"
#include "Async/ParallelFor.h"

// 생성자
HSProceduralMeshGenerator::HSProceduralMeshGenerator()
{
    // 메모리 풀 초기화
    InitializeMemoryPools();
    
    // 기본 LOD 설정
    FMeshLODSettings LOD0Settings;
    LOD0Settings.VertexReductionFactor = 1;
    LOD0Settings.NormalSmoothingAngle = 60.0f;
    LOD0Settings.bGenerateTangents = true;
    LODSettingsMap.Add(0, LOD0Settings);
    
    FMeshLODSettings LOD1Settings;
    LOD1Settings.VertexReductionFactor = 2;
    LOD1Settings.NormalSmoothingAngle = 45.0f;
    LOD1Settings.bGenerateTangents = true;
    LODSettingsMap.Add(1, LOD1Settings);
    
    FMeshLODSettings LOD2Settings;
    LOD2Settings.VertexReductionFactor = 4;
    LOD2Settings.NormalSmoothingAngle = 30.0f;
    LOD2Settings.bGenerateTangents = false;
    LODSettingsMap.Add(2, LOD2Settings);
    
    FMeshLODSettings LOD3Settings;
    LOD3Settings.VertexReductionFactor = 8;
    LOD3Settings.NormalSmoothingAngle = 15.0f;
    LOD3Settings.bGenerateTangents = false;
    LODSettingsMap.Add(3, LOD3Settings);
}

// 소멸자
HSProceduralMeshGenerator::~HSProceduralMeshGenerator()
{
    // 메모리 풀 정리
    CleanupMemoryPools();
}

// 지형 메시 생성 함수
void HSProceduralMeshGenerator::GenerateTerrainMesh(
    UProceduralMeshComponent* MeshComponent,
    float ChunkSize,
    const TArray<float>& HeightMap,
    int32 LODLevel)
{
    if (!MeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("MeshComponent가 null입니다"));
        return;
    }
    
    // 성능 측정 시작
    double StartTime = FPlatformTime::Seconds();
    
    // LOD 설정 가져오기
    FMeshLODSettings LODSettings = GetLODSettings(LODLevel);
    
    // 높이 맵 크기 계산 (정사각형 가정)
    int32 MapSize = FMath::Sqrt((float)HeightMap.Num());
    if (MapSize * MapSize != HeightMap.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("HeightMap이 정사각형이 아닙니다"));
        return;
    }
    
    // 메모리 풀에서 배열 가져오기
    TArray<FVector>& Vertices = VertexPool;
    TArray<FVector>& Normals = NormalPool;
    TArray<FVector2D>& UVs = UVPool;
    TArray<int32>& Triangles = TrianglePool;
    TArray<FProcMeshTangent>& Tangents = TangentPool;
    
    // 배열 초기화
    Vertices.Reset();
    Normals.Reset();
    UVs.Reset();
    Triangles.Reset();
    Tangents.Reset();
    
    // 정점 생성
    CreateTerrainVertices(Vertices, Normals, UVs, HeightMap, ChunkSize, LODLevel);
    
    // 삼각형 생성
    int32 GridSize = MapSize / LODSettings.VertexReductionFactor;
    CreateTerrainTriangles(Triangles, GridSize, GridSize, LODLevel);
    
    // 노말 스무딩
    if (LODSettings.NormalSmoothingAngle > 0)
    {
        SmoothNormals(Normals, Vertices, Triangles, LODSettings.NormalSmoothingAngle);
    }
    
    // 탄젠트 계산
    if (LODSettings.bGenerateTangents)
    {
        CalculateTangents(Vertices, UVs, Triangles, Tangents);
    }
    
    // 메시 최적화
    OptimizeMesh(Vertices, Triangles, 0.1f);
    
    // 메시 컴포넌트에 데이터 설정
    MeshComponent->CreateMeshSection_LinearColor(
        0, // 섹션 인덱스
        Vertices,
        Triangles,
        Normals,
        UVs,
        TArray<FLinearColor>(), // 버텍스 컬러 (사용 안 함)
        Tangents,
        true // 충돌 생성
    );
    
    // 머티리얼 설정 (필요한 경우)
    // MeshComponent->SetMaterial(0, TerrainMaterial);
    
    // 성능 통계 업데이트
    PerfStats.LastGenerationTime = FPlatformTime::Seconds() - StartTime;
    PerfStats.LastVertexCount = Vertices.Num();
    PerfStats.LastTriangleCount = Triangles.Num() / 3;
    
    UE_LOG(LogTemp, Log, TEXT("지형 메시 생성 완료: %.2fms, 정점: %d, 삼각형: %d"),
        PerfStats.LastGenerationTime * 1000.0f,
        PerfStats.LastVertexCount,
        PerfStats.LastTriangleCount);
}

// 평면 메시 생성 함수
void HSProceduralMeshGenerator::GeneratePlaneMesh(
    UProceduralMeshComponent* MeshComponent,
    float Width,
    float Height,
    int32 SubdivisionsX,
    int32 SubdivisionsY)
{
    if (!MeshComponent)
        return;
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    
    // 정점 생성
    for (int32 y = 0; y <= SubdivisionsY; y++)
    {
        for (int32 x = 0; x <= SubdivisionsX; x++)
        {
            float u = (float)x / SubdivisionsX;
            float v = (float)y / SubdivisionsY;
            
            FVector Vertex(
                (u - 0.5f) * Width,
                (v - 0.5f) * Height,
                0.0f
            );
            
            Vertices.Add(Vertex);
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(u, v));
        }
    }
    
    // 삼각형 생성
    for (int32 y = 0; y < SubdivisionsY; y++)
    {
        for (int32 x = 0; x < SubdivisionsX; x++)
        {
            int32 TopLeft = y * (SubdivisionsX + 1) + x;
            int32 TopRight = TopLeft + 1;
            int32 BottomLeft = TopLeft + (SubdivisionsX + 1);
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
    
    // 메시 생성
    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        TArray<FLinearColor>(),
        TArray<FProcMeshTangent>(),
        true
    );
}

// 청크 경계 정점 블렌딩 함수
void HSProceduralMeshGenerator::BlendBorderVertices(
    TArray<float>& HeightMap,
    const TArray<float>& NeighborHeightMap,
    FIntPoint Direction)
{
    int32 MapSize = FMath::Sqrt((float)HeightMap.Num());
    
    // 블렌딩할 경계 인덱스 계산
    TArray<int32> BorderIndices;
    
    if (Direction.X > 0) // 동쪽
    {
        for (int32 y = 0; y < MapSize; y++)
        {
            BorderIndices.Add(y * MapSize + (MapSize - 1));
        }
    }
    else if (Direction.X < 0) // 서쪽
    {
        for (int32 y = 0; y < MapSize; y++)
        {
            BorderIndices.Add(y * MapSize);
        }
    }
    
    if (Direction.Y > 0) // 북쪽
    {
        for (int32 x = 0; x < MapSize; x++)
        {
            BorderIndices.Add((MapSize - 1) * MapSize + x);
        }
    }
    else if (Direction.Y < 0) // 남쪽
    {
        for (int32 x = 0; x < MapSize; x++)
        {
            BorderIndices.Add(x);
        }
    }
    
    // 경계 정점 블렌딩
    float BlendFactor = 0.5f;
    for (int32 Index : BorderIndices)
    {
        if (Index < HeightMap.Num() && Index < NeighborHeightMap.Num())
        {
            HeightMap[Index] = FMath::Lerp(HeightMap[Index], NeighborHeightMap[Index], BlendFactor);
        }
    }
}

// 높이 맵에서 노말 계산 함수
FVector HSProceduralMeshGenerator::CalculateNormalFromHeightMap(
    const TArray<float>& HeightMap,
    int32 X,
    int32 Y,
    int32 MapSize,
    float Scale)
{
    // 중심점 주변의 높이 값 가져오기
    auto GetHeight = [&](int32 dx, int32 dy) -> float
    {
        int32 nx = FMath::Clamp(X + dx, 0, MapSize - 1);
        int32 ny = FMath::Clamp(Y + dy, 0, MapSize - 1);
        return HeightMap[ny * MapSize + nx];
    };
    
    // Sobel 필터를 사용한 노말 계산
    float HeightL = GetHeight(-1, 0);
    float HeightR = GetHeight(1, 0);
    float HeightD = GetHeight(0, -1);
    float HeightU = GetHeight(0, 1);
    
    FVector Normal(
        (HeightL - HeightR) * Scale,
        (HeightD - HeightU) * Scale,
        2.0f
    );
    
    return Normal.GetSafeNormal();
}

// 메시 최적화 함수
void HSProceduralMeshGenerator::OptimizeMesh(
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    float WeldThreshold)
{
    // 정점 용접
    WeldVertices(Vertices, Triangles, WeldThreshold);
    
    // 삼각형 스트립 최적화
    OptimizeTriangleStrip(Triangles);
}

// 탄젠트 계산 함수
void HSProceduralMeshGenerator::CalculateTangents(
    const TArray<FVector>& Vertices,
    const TArray<FVector2D>& UVs,
    const TArray<int32>& Triangles,
    TArray<FProcMeshTangent>& Tangents)
{
    Tangents.SetNum(Vertices.Num());
    
    // 병렬 처리를 통한 탄젠트 계산
    ParallelFor(Triangles.Num() / 3, [&](int32 TriIndex)
    {
        int32 i0 = Triangles[TriIndex * 3 + 0];
        int32 i1 = Triangles[TriIndex * 3 + 1];
        int32 i2 = Triangles[TriIndex * 3 + 2];
        
        const FVector& v0 = Vertices[i0];
        const FVector& v1 = Vertices[i1];
        const FVector& v2 = Vertices[i2];
        
        const FVector2D& uv0 = UVs[i0];
        const FVector2D& uv1 = UVs[i1];
        const FVector2D& uv2 = UVs[i2];
        
        // 삼각형의 엣지 계산
        FVector Edge1 = v1 - v0;
        FVector Edge2 = v2 - v0;
        
        FVector2D DeltaUV1 = uv1 - uv0;
        FVector2D DeltaUV2 = uv2 - uv0;
        
        float f = 1.0f / (DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y);
        
        FVector Tangent;
        Tangent.X = f * (DeltaUV2.Y * Edge1.X - DeltaUV1.Y * Edge2.X);
        Tangent.Y = f * (DeltaUV2.Y * Edge1.Y - DeltaUV1.Y * Edge2.Y);
        Tangent.Z = f * (DeltaUV2.Y * Edge1.Z - DeltaUV1.Y * Edge2.Z);
        Tangent.Normalize();
        
        // 각 정점에 탄젠트 할당
        Tangents[i0].TangentX = Tangent;
        Tangents[i1].TangentX = Tangent;
        Tangents[i2].TangentX = Tangent;
        
        Tangents[i0].bFlipTangentY = false;
        Tangents[i1].bFlipTangentY = false;
        Tangents[i2].bFlipTangentY = false;
    });
}

// 지형 정점 생성 헬퍼 함수
void HSProceduralMeshGenerator::CreateTerrainVertices(
    TArray<FVector>& OutVertices,
    TArray<FVector>& OutNormals,
    TArray<FVector2D>& OutUVs,
    const TArray<float>& HeightMap,
    float ChunkSize,
    int32 LODLevel)
{
    FMeshLODSettings LODSettings = GetLODSettings(LODLevel);
    int32 MapSize = FMath::Sqrt((float)HeightMap.Num());
    int32 Step = LODSettings.VertexReductionFactor;
    
    float CellSize = ChunkSize / (MapSize - 1);
    
    // 정점 생성
    for (int32 y = 0; y < MapSize; y += Step)
    {
        for (int32 x = 0; x < MapSize; x += Step)
        {
            int32 Index = y * MapSize + x;
            float Height = HeightMap[Index];
            
            // 위치 계산
            FVector Position(
                x * CellSize - ChunkSize * 0.5f,
                y * CellSize - ChunkSize * 0.5f,
                Height
            );
            
            // 노말 계산
            FVector Normal = CalculateNormalFromHeightMap(HeightMap, x, y, MapSize, CellSize);
            
            // UV 계산
            FVector2D UV((float)x / (MapSize - 1), (float)y / (MapSize - 1));
            
            OutVertices.Add(Position);
            OutNormals.Add(Normal);
            OutUVs.Add(UV);
        }
    }
}

// 지형 삼각형 생성 헬퍼 함수
void HSProceduralMeshGenerator::CreateTerrainTriangles(
    TArray<int32>& OutTriangles,
    int32 GridSizeX,
    int32 GridSizeY,
    int32 LODLevel)
{
    // 삼각형 인덱스 생성
    for (int32 y = 0; y < GridSizeY - 1; y++)
    {
        for (int32 x = 0; x < GridSizeX - 1; x++)
        {
            int32 TopLeft = y * GridSizeX + x;
            int32 TopRight = TopLeft + 1;
            int32 BottomLeft = TopLeft + GridSizeX;
            int32 BottomRight = BottomLeft + 1;
            
            // 첫 번째 삼각형
            OutTriangles.Add(TopLeft);
            OutTriangles.Add(BottomLeft);
            OutTriangles.Add(TopRight);
            
            // 두 번째 삼각형
            OutTriangles.Add(TopRight);
            OutTriangles.Add(BottomLeft);
            OutTriangles.Add(BottomRight);
        }
    }
}

// 정점 용접 함수
void HSProceduralMeshGenerator::WeldVertices(
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    float Threshold)
{
    TMap<int32, int32> VertexRemap;
    TArray<FVector> WeldedVertices;
    
    // 중복 정점 찾기 및 제거
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        bool bFound = false;
        
        for (int32 j = 0; j < WeldedVertices.Num(); j++)
        {
            if (FVector::Dist(Vertices[i], WeldedVertices[j]) < Threshold)
            {
                VertexRemap.Add(i, j);
                bFound = true;
                break;
            }
        }
        
        if (!bFound)
        {
            VertexRemap.Add(i, WeldedVertices.Num());
            WeldedVertices.Add(Vertices[i]);
        }
    }
    
    // 삼각형 인덱스 업데이트
    for (int32& Index : Triangles)
    {
        Index = VertexRemap[Index];
    }
    
    // 정점 배열 업데이트
    Vertices = WeldedVertices;
}

// 삼각형 스트립 최적화
void HSProceduralMeshGenerator::OptimizeTriangleStrip(TArray<int32>& Triangles)
{
    // 간단한 캐시 최적화
    // 실제 구현은 더 복잡한 알고리즘 필요
    // 여기서는 기본적인 구현만 제공
}

// 노말 스무딩 함수
void HSProceduralMeshGenerator::SmoothNormals(
    TArray<FVector>& Normals,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    float SmoothingAngle)
{
    TArray<FVector> SmoothNormals;
    SmoothNormals.SetNum(Vertices.Num());
    
    // 각 정점에 대한 노말 평균 계산
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        int32 i0 = Triangles[i];
        int32 i1 = Triangles[i + 1];
        int32 i2 = Triangles[i + 2];
        
        // 삼각형 노말 계산
        FVector v0 = Vertices[i0];
        FVector v1 = Vertices[i1];
        FVector v2 = Vertices[i2];
        
        FVector TriNormal = FVector::CrossProduct(v1 - v0, v2 - v0).GetSafeNormal();
        
        // 각 정점에 노말 누적
        SmoothNormals[i0] += TriNormal;
        SmoothNormals[i1] += TriNormal;
        SmoothNormals[i2] += TriNormal;
    }
    
    // 노말 정규화
    for (int32 i = 0; i < SmoothNormals.Num(); i++)
    {
        SmoothNormals[i].Normalize();
        
        // 스무딩 각도에 따른 블렌딩
        float Dot = FVector::DotProduct(Normals[i], SmoothNormals[i]);
        float Angle = FMath::Acos(Dot) * (180.0f / PI);
        
        if (Angle < SmoothingAngle)
        {
            Normals[i] = SmoothNormals[i];
        }
    }
}

// LOD 설정 함수
void HSProceduralMeshGenerator::SetLODSettings(int32 LODLevel, const FMeshLODSettings& Settings)
{
    LODSettingsMap.Add(LODLevel, Settings);
}

FMeshLODSettings HSProceduralMeshGenerator::GetLODSettings(int32 LODLevel) const
{
    if (LODSettingsMap.Contains(LODLevel))
    {
        return LODSettingsMap[LODLevel];
    }
    
    // 기본 설정 반환
    return FMeshLODSettings();
}

// 메모리 풀 초기화
void HSProceduralMeshGenerator::InitializeMemoryPools()
{
    VertexPool.Reserve(MaxPoolSize);
    NormalPool.Reserve(MaxPoolSize);
    UVPool.Reserve(MaxPoolSize);
    TrianglePool.Reserve(MaxPoolSize * 3);
    TangentPool.Reserve(MaxPoolSize);
}

// 메모리 풀 정리
void HSProceduralMeshGenerator::CleanupMemoryPools()
{
    VertexPool.Empty();
    NormalPool.Empty();
    UVPool.Empty();
    TrianglePool.Empty();
    TangentPool.Empty();
}

// 메시 통계 정보 가져오기
void HSProceduralMeshGenerator::GetMeshStatistics(
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    int32& OutVertexCount,
    int32& OutTriangleCount,
    float& OutMemoryUsage)
{
    OutVertexCount = Vertices.Num();
    OutTriangleCount = Triangles.Num() / 3;
    
    // 메모리 사용량 계산 (근사치)
    OutMemoryUsage = 0.0f;
    OutMemoryUsage += Vertices.Num() * sizeof(FVector);
    OutMemoryUsage += Triangles.Num() * sizeof(int32);
    OutMemoryUsage = OutMemoryUsage / 1024.0f / 1024.0f; // MB로 변환
}

// UV 매핑 함수
void HSProceduralMeshGenerator::GenerateUVMapping(
    TArray<FVector2D>& UVs,
    const TArray<FVector>& Vertices,
    float UVScale)
{
    UVs.SetNum(Vertices.Num());
    
    // 바운딩 박스 계산
    FBox BoundingBox(Vertices);
    FVector Center = BoundingBox.GetCenter();
    FVector Extent = BoundingBox.GetExtent();
    
    // UV 생성
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        FVector RelativePos = Vertices[i] - Center;
        
        UVs[i].X = (RelativePos.X / Extent.X + 1.0f) * 0.5f * UVScale;
        UVs[i].Y = (RelativePos.Y / Extent.Y + 1.0f) * 0.5f * UVScale;
    }
}

// 박스 메시 생성 함수
void HSProceduralMeshGenerator::GenerateBoxMesh(
    UProceduralMeshComponent* MeshComponent,
    FVector BoxSize,
    int32 SubdivisionsPerFace)
{
    if (!MeshComponent)
        return;
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    
    // 박스의 절반 크기
    FVector HalfSize = BoxSize * 0.5f;
    
    // 6개 면 생성
    // 전면 (+Y)
    GeneratePlaneMeshData(Vertices, Triangles, Normals, UVs,
        FVector(0, HalfSize.Y, 0), FVector(1, 0, 0), FVector(0, 0, 1),
        BoxSize.X, BoxSize.Z, SubdivisionsPerFace, SubdivisionsPerFace);
    
    // 후면 (-Y)
    GeneratePlaneMeshData(Vertices, Triangles, Normals, UVs,
        FVector(0, -HalfSize.Y, 0), FVector(-1, 0, 0), FVector(0, 0, 1),
        BoxSize.X, BoxSize.Z, SubdivisionsPerFace, SubdivisionsPerFace);
    
    // 추가 면들도 동일한 방식으로 생성...
    
    // 메시 생성
    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        TArray<FLinearColor>(),
        TArray<FProcMeshTangent>(),
        true
    );
}

// 평면 메시 데이터 생성 헬퍼 함수
void HSProceduralMeshGenerator::GeneratePlaneMeshData(
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
    int32 SubdivisionsY)
{
    int32 VertexOffset = Vertices.Num();
    FVector Normal = FVector::CrossProduct(Right, Up).GetSafeNormal();
    
    // 정점 생성
    for (int32 y = 0; y <= SubdivisionsY; y++)
    {
        for (int32 x = 0; x <= SubdivisionsX; x++)
        {
            float u = (float)x / SubdivisionsX;
            float v = (float)y / SubdivisionsY;
            
            FVector Vertex = Origin + 
                Right * ((u - 0.5f) * Width) + 
                Up * ((v - 0.5f) * Height);
            
            Vertices.Add(Vertex);
            Normals.Add(Normal);
            UVs.Add(FVector2D(u, v));
        }
    }
    
    // 삼각형 생성
    for (int32 y = 0; y < SubdivisionsY; y++)
    {
        for (int32 x = 0; x < SubdivisionsX; x++)
        {
            int32 TopLeft = VertexOffset + y * (SubdivisionsX + 1) + x;
            int32 TopRight = TopLeft + 1;
            int32 BottomLeft = TopLeft + (SubdivisionsX + 1);
            int32 BottomRight = BottomLeft + 1;
            
            Triangles.Add(TopLeft);
            Triangles.Add(BottomLeft);
            Triangles.Add(TopRight);
            
            Triangles.Add(TopRight);
            Triangles.Add(BottomLeft);
            Triangles.Add(BottomRight);
        }
    }
}

// 구체 메시 생성 함수
void HSProceduralMeshGenerator::GenerateSphereMesh(
    UProceduralMeshComponent* MeshComponent,
    float Radius,
    int32 LatitudeSegments,
    int32 LongitudeSegments)
{
    if (!MeshComponent)
        return;
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    
    // 구체 정점 생성
    for (int32 lat = 0; lat <= LatitudeSegments; lat++)
    {
        float theta = lat * PI / LatitudeSegments;
        float sinTheta = FMath::Sin(theta);
        float cosTheta = FMath::Cos(theta);
        
        for (int32 lon = 0; lon <= LongitudeSegments; lon++)
        {
            float phi = lon * 2 * PI / LongitudeSegments;
            float sinPhi = FMath::Sin(phi);
            float cosPhi = FMath::Cos(phi);
            
            FVector Normal(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            FVector Vertex = Normal * Radius;
            
            Vertices.Add(Vertex);
            Normals.Add(Normal);
            UVs.Add(FVector2D((float)lon / LongitudeSegments, (float)lat / LatitudeSegments));
        }
    }
    
    // 구체 삼각형 생성
    for (int32 lat = 0; lat < LatitudeSegments; lat++)
    {
        for (int32 lon = 0; lon < LongitudeSegments; lon++)
        {
            int32 First = lat * (LongitudeSegments + 1) + lon;
            int32 Second = First + LongitudeSegments + 1;
            
            Triangles.Add(First);
            Triangles.Add(Second);
            Triangles.Add(First + 1);
            
            Triangles.Add(Second);
            Triangles.Add(Second + 1);
            Triangles.Add(First + 1);
        }
    }
    
    // 메시 생성
    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        TArray<FLinearColor>(),
        TArray<FProcMeshTangent>(),
        true
    );
}

// 실린더 메시 생성 함수
void HSProceduralMeshGenerator::GenerateCylinderMesh(
    UProceduralMeshComponent* MeshComponent,
    float Radius,
    float Height,
    int32 RadialSegments,
    int32 HeightSegments)
{
    if (!MeshComponent)
        return;
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    
    float HalfHeight = Height * 0.5f;
    
    // 측면 정점 생성
    for (int32 h = 0; h <= HeightSegments; h++)
    {
        float y = -HalfHeight + (float)h / HeightSegments * Height;
        
        for (int32 r = 0; r <= RadialSegments; r++)
        {
            float angle = (float)r / RadialSegments * 2 * PI;
            float x = FMath::Cos(angle) * Radius;
            float z = FMath::Sin(angle) * Radius;
            
            FVector Vertex(x, y, z);
            FVector Normal(x, 0, z);
            Normal.Normalize();
            
            Vertices.Add(Vertex);
            Normals.Add(Normal);
            UVs.Add(FVector2D((float)r / RadialSegments, (float)h / HeightSegments));
        }
    }
    
    // 측면 삼각형 생성
    for (int32 h = 0; h < HeightSegments; h++)
    {
        for (int32 r = 0; r < RadialSegments; r++)
        {
            int32 Current = h * (RadialSegments + 1) + r;
            int32 Next = Current + RadialSegments + 1;
            
            Triangles.Add(Current);
            Triangles.Add(Next);
            Triangles.Add(Current + 1);
            
            Triangles.Add(Next);
            Triangles.Add(Next + 1);
            Triangles.Add(Current + 1);
        }
    }
    
    // 상단 캡 생성
    int32 CenterTop = Vertices.Num();
    Vertices.Add(FVector(0, HalfHeight, 0));
    Normals.Add(FVector::UpVector);
    UVs.Add(FVector2D(0.5f, 0.5f));
    
    int32 TopStartIndex = HeightSegments * (RadialSegments + 1);
    for (int32 r = 0; r < RadialSegments; r++)
    {
        Triangles.Add(CenterTop);
        Triangles.Add(TopStartIndex + r);
        Triangles.Add(TopStartIndex + r + 1);
    }
    
    // 하단 캡 생성
    int32 CenterBottom = Vertices.Num();
    Vertices.Add(FVector(0, -HalfHeight, 0));
    Normals.Add(FVector::DownVector);
    UVs.Add(FVector2D(0.5f, 0.5f));
    
    for (int32 r = 0; r < RadialSegments; r++)
    {
        Triangles.Add(CenterBottom);
        Triangles.Add(r + 1);
        Triangles.Add(r);
    }
    
    // 메시 생성
    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        TArray<FLinearColor>(),
        TArray<FProcMeshTangent>(),
        true
    );
}

// 높이 맵 보간 함수
float HSProceduralMeshGenerator::InterpolateHeight(
    const TArray<float>& HeightMap,
    float X,
    float Y,
    int32 MapSize)
{
    // 바이리니어 보간
    int32 X0 = FMath::Clamp(FMath::FloorToInt(X), 0, MapSize - 2);
    int32 Y0 = FMath::Clamp(FMath::FloorToInt(Y), 0, MapSize - 2);
    int32 X1 = X0 + 1;
    int32 Y1 = Y0 + 1;
    
    float FracX = X - X0;
    float FracY = Y - Y0;
    
    float Height00 = HeightMap[Y0 * MapSize + X0];
    float Height10 = HeightMap[Y0 * MapSize + X1];
    float Height01 = HeightMap[Y1 * MapSize + X0];
    float Height11 = HeightMap[Y1 * MapSize + X1];
    
    float HeightX0 = FMath::Lerp(Height00, Height10, FracX);
    float HeightX1 = FMath::Lerp(Height01, Height11, FracX);
    
    return FMath::Lerp(HeightX0, HeightX1, FracY);
}
