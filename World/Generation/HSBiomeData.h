// HSBiomeData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "HSBiomeData.generated.h"

/**
 * 지형 특성을 정의하는 열거형
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    None            UMETA(DisplayName = "None"),
    Forest          UMETA(DisplayName = "숲"),
    Desert          UMETA(DisplayName = "사막"),
    Swamp           UMETA(DisplayName = "늪지"),
    Mountain        UMETA(DisplayName = "산악"),
    Tundra          UMETA(DisplayName = "설원"),
    Volcanic        UMETA(DisplayName = "화산"),
    Corrupted       UMETA(DisplayName = "오염된 땅")
};

/**
 * 바이옴에 스폰될 수 있는 오브젝트 정보
 */
USTRUCT(BlueprintType)
struct FBiomeSpawnableObject
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    TSoftClassPtr<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnProbability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
    int32 MinSpawnCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
    int32 MaxSpawnCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    float MinDistanceBetweenObjects = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    bool bAlignToSurface = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    FVector SpawnOffset = FVector::ZeroVector;

    FBiomeSpawnableObject()
    {
        SpawnProbability = 0.5f;
        MinSpawnCount = 1;
        MaxSpawnCount = 3;
        MinDistanceBetweenObjects = 500.0f;
        bAlignToSurface = true;
    }
};

/**
 * 바이옴 환경 설정
 */
USTRUCT(BlueprintType)
struct FBiomeEnvironmentSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    FLinearColor FogColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0.0"))
    float FogDensity = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    FLinearColor AmbientLightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0.0"))
    float AmbientLightIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    class USoundBase* AmbientSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    TArray<class UMaterialInterface*> TerrainMaterials;
};

/**
 * 바이옴 데이터를 정의하는 데이터 에셋 클래스
 * 각 바이옴의 특성, 스폰 가능한 오브젝트, 환경 설정 등을 포함
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSBiomeData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UHSBiomeData();

    // 바이옴 기본 정보
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Info")
    FName BiomeName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Info")
    EBiomeType BiomeType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Info", meta = (MultiLine = true))
    FText BiomeDescription;

    // 지형 생성 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TerrainRoughness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0"))
    float TerrainHeightMultiplier = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0"))
    float NoiseScale = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "1"))
    int32 NoiseOctaves = 4;

    // 스폰 가능한 오브젝트들
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    TArray<FBiomeSpawnableObject> ResourceNodes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    TArray<FBiomeSpawnableObject> EnvironmentProps;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    TArray<FBiomeSpawnableObject> EnemySpawns;

    // 환경 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    FBiomeEnvironmentSettings EnvironmentSettings;

    // 인접 가능한 바이옴 타입
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Rules")
    TArray<EBiomeType> CompatibleBiomes;

    // 이 바이옴이 생성될 확률 가중치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Rules", meta = (ClampMin = "0.0"))
    float GenerationWeight = 1.0f;

    // 최소/최대 크기 (월드 단위)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Rules", meta = (ClampMin = "1"))
    int32 MinBiomeSize = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Rules", meta = (ClampMin = "1"))
    int32 MaxBiomeSize = 20;

public:
    /**
     * 주어진 위치에서 지형 높이를 계산
     * @param Position 월드 위치
     * @param Seed 난수 시드
     * @return 계산된 높이 값
     */
    UFUNCTION(BlueprintCallable, Category = "Biome")
    float CalculateTerrainHeightAtPosition(const FVector2D& Position, int32 Seed) const;

    /**
     * 바이옴이 다른 바이옴과 인접 가능한지 확인
     * @param OtherBiomeType 확인할 바이옴 타입
     * @return 인접 가능 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Biome")
    bool IsCompatibleWith(EBiomeType OtherBiomeType) const;

    /**
     * 스폰 가능한 오브젝트 목록을 확률에 따라 필터링
     * @param SpawnableObjects 필터링할 오브젝트 배열
     * @param RandomStream 난수 스트림
     * @return 필터링된 오브젝트 배열
     */
    UFUNCTION(BlueprintCallable, Category = "Biome")
    TArray<FBiomeSpawnableObject> FilterSpawnablesByProbability(const TArray<FBiomeSpawnableObject>& SpawnableObjects, const FRandomStream& RandomStream) const;

    // Primary Asset 인터페이스
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

protected:
    /**
     * Perlin 노이즈를 사용한 지형 높이 계산
     * @param X X 좌표
     * @param Y Y 좌표
     * @param Seed 시드 값
     * @return 노이즈 값 (-1 ~ 1)
     */
    float PerlinNoise2D(float X, float Y, int32 Seed) const;
};
