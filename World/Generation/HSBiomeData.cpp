// HSBiomeData.cpp
#include "HSBiomeData.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/AssetManager.h"

UHSBiomeData::UHSBiomeData()
{
    // 기본값 설정
    BiomeName = TEXT("Default Biome");
    BiomeType = EBiomeType::Forest;
    TerrainRoughness = 0.5f;
    TerrainHeightMultiplier = 1000.0f;
    NoiseScale = 0.001f;
    NoiseOctaves = 4;
    GenerationWeight = 1.0f;
    MinBiomeSize = 5;
    MaxBiomeSize = 20;
}

float UHSBiomeData::CalculateTerrainHeightAtPosition(const FVector2D& Position, int32 Seed) const
{
    float Height = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = NoiseScale;
    
    // Octave 기반 Perlin 노이즈를 사용한 지형 생성
    for (int32 i = 0; i < NoiseOctaves; i++)
    {
        float NoiseValue = PerlinNoise2D(Position.X * Frequency, Position.Y * Frequency, Seed + i);
        Height += NoiseValue * Amplitude;
        
        // 다음 Octave를 위한 설정
        Amplitude *= TerrainRoughness;
        Frequency *= 2.0f;
    }
    
    // 높이 정규화 및 스케일링
    Height = (Height + 1.0f) * 0.5f; // 0~1 범위로 정규화
    Height *= TerrainHeightMultiplier;
    
    return Height;
}

bool UHSBiomeData::IsCompatibleWith(EBiomeType OtherBiomeType) const
{
    return CompatibleBiomes.Contains(OtherBiomeType);
}

TArray<FBiomeSpawnableObject> UHSBiomeData::FilterSpawnablesByProbability(
    const TArray<FBiomeSpawnableObject>& SpawnableObjects, 
    const FRandomStream& RandomStream) const
{
    TArray<FBiomeSpawnableObject> FilteredObjects;
    
    // 확률에 따라 오브젝트 필터링
    for (const FBiomeSpawnableObject& SpawnableObject : SpawnableObjects)
    {
        if (RandomStream.FRand() <= SpawnableObject.SpawnProbability)
        {
            FilteredObjects.Add(SpawnableObject);
        }
    }
    
    return FilteredObjects;
}

FPrimaryAssetId UHSBiomeData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId("BiomeData", GetFName());
}

float UHSBiomeData::PerlinNoise2D(float X, float Y, int32 Seed) const
{
    // Perlin 노이즈 구현

    // 격자 좌표 계산
    int32 X0 = FMath::FloorToInt(X);
    int32 X1 = X0 + 1;
    int32 Y0 = FMath::FloorToInt(Y);
    int32 Y1 = Y0 + 1;
    
    // 보간 가중치
    float Sx = X - (float)X0;
    float Sy = Y - (float)Y0;
    
    // 각 격자점에서의 그래디언트와 거리 벡터의 내적 계산
    auto DotGridGradient = [Seed](int32 IX, int32 IY, float X, float Y) -> float
    {
        // 의사 난수 그래디언트 벡터 생성
        uint32 Hash = (uint32)(IX * 73856093) ^ (uint32)(IY * 19349663) ^ (uint32)(Seed * 83492791);
        Hash = (Hash * Hash * Hash) % 2147483647;
        
        float Angle = (Hash % 360) * (PI / 180.0f);
        FVector2D Gradient(FMath::Cos(Angle), FMath::Sin(Angle));
        
        // 거리 벡터
        FVector2D Distance(X - (float)IX, Y - (float)IY);
        
        // 내적 반환
        return FVector2D::DotProduct(Gradient, Distance);
    };
    
    float N0 = DotGridGradient(X0, Y0, X, Y);
    float N1 = DotGridGradient(X1, Y0, X, Y);
    float IX0 = FMath::Lerp(N0, N1, Sx);
    
    N0 = DotGridGradient(X0, Y1, X, Y);
    N1 = DotGridGradient(X1, Y1, X, Y);
    float IX1 = FMath::Lerp(N0, N1, Sx);
    
    // 부드러운 보간을 위한 Fade 함수 적용
    float U = Sx * Sx * (3.0f - 2.0f * Sx);
    float V = Sy * Sy * (3.0f - 2.0f * Sy);
    
    return FMath::Lerp(IX0, IX1, V);
}
