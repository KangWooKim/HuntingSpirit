// 사냥의 영혼(HuntingSpirit) 게임의 성능 최적화 유틸리티 클래스
// 다양한 최적화 기법들을 제공

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Engine.h"
#include "HAL/Platform.h"
#include "Math/Vector.h"
#include "Math/UnrealMathVectorCommon.h"
#include "HSPerformanceOptimizer.generated.h"

/**
 * SIMD 최적화를 위한 벡터 연산 유틸리티
 * 대량의 벡터 연산 시 사용하는 최적화 기법
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FSIMDVectorOperations
{
    GENERATED_BODY()

    /**
     * SIMD를 활용한 대량 벡터 거리 계산
     * 일반적인 루프보다 4배 빠른 처리 속도
     */
    static void CalculateDistancesBatch(
        const TArray<FVector>& SourcePositions,
        const FVector& TargetPosition,
        TArray<float>& OutDistances
    );

    /**
     * SIMD를 활용한 벡터 정규화 배치 처리
     */
    static void NormalizeVectorsBatch(TArray<FVector>& InOutVectors);

    /**
     * SIMD를 활용한 벡터 내적 배치 계산
     */
    static void DotProductBatch(
        const TArray<FVector>& VectorsA,
        const TArray<FVector>& VectorsB,
        TArray<float>& OutResults
    );
};

/**
 * 메모리 압축 및 최적화 기법
 * 메모리 사용량 최소화를 위한 기법들
 * 참고: C++ 전용 구조체로, Blueprint에서는 사용하지 않음
 */
USTRUCT()
struct HUNTINGSPIRIT_API FCompressedPlayerData
{
    GENERATED_BODY()

    // 위치 데이터를 16비트로 압축 (정밀도 vs 메모리 트레이드오프)
    uint16 CompressedX;

    uint16 CompressedY;

    uint16 CompressedZ;

    // 회전 데이터를 쿼터니언 압축
    uint32 CompressedRotation;

    // 비트 필드로 상태 플래그들 압축
    uint8 StatusFlags; // 8개의 bool 값을 1바이트로 압축

    // 압축/해제 함수들
    void CompressFromVector(const FVector& Position);
    FVector DecompressToVector() const;
    void CompressFromRotator(const FRotator& Rotation);
    FRotator DecompressToRotator() const;

    // 상태 플래그 비트 조작
    void SetStatusFlag(uint8 FlagIndex, bool bValue);
    bool GetStatusFlag(uint8 FlagIndex) const;

    FCompressedPlayerData()
    {
        CompressedX = 0;
        CompressedY = 0;
        CompressedZ = 0;
        CompressedRotation = 0;
        StatusFlags = 0;
    }
};

/**
 * 캐시 최적화를 위한 데이터 배치 구조
 * CPU 캐시 히트율 최적화를 위한 SoA(Structure of Arrays) 패턴
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FCacheOptimizedPlayerArray
{
    GENERATED_BODY()

    // 전통적인 AoS(Array of Structures) 대신 SoA(Structure of Arrays) 사용
    // 메모리 지역성 향상으로 캐시 미스 감소

    UPROPERTY(BlueprintReadOnly, Category = "Cache Optimized")
    TArray<FVector> Positions;

    UPROPERTY(BlueprintReadOnly, Category = "Cache Optimized")
    TArray<FVector> Velocities;

    UPROPERTY(BlueprintReadOnly, Category = "Cache Optimized")
    TArray<float> HealthValues;

    UPROPERTY(BlueprintReadOnly, Category = "Cache Optimized")
    TArray<int32> TeamIDs;

    UPROPERTY(BlueprintReadOnly, Category = "Cache Optimized")
    TArray<uint8> StatusFlags;

    /**
     * 특정 인덱스의 플레이어를 제거하며 배열 일관성 유지
     */
    void RemovePlayerAtIndex(int32 Index);

    /**
     * 새로운 플레이어 데이터 추가
     */
    int32 AddPlayer(const FVector& Position, const FVector& Velocity, 
                   float Health, int32 TeamID, uint8 Status);

    /**
     * 모든 배열 크기가 일치하는지 검증
     */
    bool ValidateArrayConsistency() const;

    /**
     * 배열 크기 반환
     */
    int32 GetPlayerCount() const { return Positions.Num(); }
};

/**
 * 고성능 오브젝트 풀 관리자
 * 최적화된 풀링 시스템
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHighPerformanceObjectPool
{
    GENERATED_BODY()

private:
    // 프리 리스트를 활용한 빠른 할당/해제
    TArray<int32> FreeIndices;
    TArray<bool> ActiveFlags;
    int32 MaxPoolSize;
    int32 CurrentPoolSize;

public:
    FHighPerformanceObjectPool()
    {
        MaxPoolSize = 1000; // 기본 최대 크기
        CurrentPoolSize = 0;
        FreeIndices.Reserve(MaxPoolSize);
        ActiveFlags.Reserve(MaxPoolSize);
    }

    /**
     * 오브젝트 인덱스 할당 (O(1) 시간복잡도)
     */
    int32 AllocateIndex();

    /**
     * 오브젝트 인덱스 해제 (O(1) 시간복잡도)
     */
    void DeallocateIndex(int32 Index);

    /**
     * 인덱스가 활성 상태인지 확인
     */
    bool IsIndexActive(int32 Index) const;

    /**
     * 활성 오브젝트 수 반환
     */
    int32 GetActiveCount() const;

    /**
     * 풀 크기 조정
     */
    void ResizePool(int32 NewMaxSize);

    /**
     * 풀 상태 리셋
     */
    void ResetPool();
};

/**
 * 성능 최적화 유틸리티 클래스
 * 다양한 최적화 기법들을 제공
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSPerformanceOptimizer : public UObject
{
    GENERATED_BODY()

public:
    UHSPerformanceOptimizer();

    // === 메모리 최적화 기법 ===

    /**
     * 메모리 정렬 최적화
     * 구조체 메모리 레이아웃 최적화 시 사용
     */
    UFUNCTION(BlueprintCallable, Category = "Memory Optimization")
    static int32 GetOptimalStructAlignment(int32 StructSize);

    /**
     * 메모리 풀 사전 할당
     * 런타임 중 동적 할당을 최소화하는 기법
     */
    UFUNCTION(BlueprintCallable, Category = "Memory Optimization")
    static void PreallocateMemoryPools(int32 ExpectedObjectCount);

    // === CPU 최적화 기법 ===

    /**
     * 분기 예측 최적화 힌트
     * 조건문 성능 최적화 시 사용
     */
    UFUNCTION(BlueprintCallable, Category = "CPU Optimization")
    static FORCEINLINE bool LikelyCondition(bool Condition)
    {
        return LIKELY(Condition);
    }

    UFUNCTION(BlueprintCallable, Category = "CPU Optimization")
    static FORCEINLINE bool UnlikelyCondition(bool Condition)
    {
        return UNLIKELY(Condition);
    }

    /**
     * 캐시 프리페치 힌트
     * 메모리 접근 전에 캐시로 미리 로드
     * Blueprint에서는 지원하지 않으므로 C++에서만 사용
     */
    static void PrefetchMemory(const void* Address);

    // === 배치 처리 최적화 ===

    /**
     * 작업을 배치로 묶어서 처리
     * 함수 호출 오버헤드 최소화
     */
    UFUNCTION(BlueprintCallable, Category = "Batch Processing")
    static void ProcessPlayerUpdatesBatch(
        const TArray<FVector>& Positions,
        const TArray<FVector>& Velocities,
        float DeltaTime,
        TArray<FVector>& OutNewPositions
    );

    /**
     * 멀티스레드 배치 처리
     * 병렬 처리로 성능 향상
     */
    UFUNCTION(BlueprintCallable, Category = "Batch Processing")
    static void ProcessPlayerUpdatesParallel(
        const TArray<FVector>& Positions,
        const TArray<FVector>& Velocities,
        float DeltaTime,
        TArray<FVector>& OutNewPositions
    );

    // === 네트워크 최적화 ===

    /**
     * 델타 압축
     * 이전 상태와의 차이만 전송하여 대역폭 절약
     */
    UFUNCTION(BlueprintCallable, Category = "Network Optimization")
    static TArray<uint8> CompressDeltaData(
        const TArray<uint8>& PreviousData,
        const TArray<uint8>& CurrentData
    );

    /**
     * 비트 패킹
     * bool 값들을 비트로 압축하여 네트워크 부하 감소
     * 참고: C++ 전용 함수
     */
    static uint32 PackBoolArrayToBits(const TArray<bool>& BoolArray);

    static TArray<bool> UnpackBitsToBoolean(uint32 PackedBits, int32 BoolCount);

    /**
     * Blueprint용 비트 패킹 함수
     * int32를 사용하여 Blueprint 호환성 제공
     */
    UFUNCTION(BlueprintCallable, Category = "Network Optimization", DisplayName = "Pack Bool Array To Bits")
    static int32 PackBoolArrayToBits_BP(const TArray<bool>& BoolArray);

    UFUNCTION(BlueprintCallable, Category = "Network Optimization", DisplayName = "Unpack Bits To Boolean")
    static TArray<bool> UnpackBitsToBoolean_BP(int32 PackedBits, int32 BoolCount);

    // === 프로파일링 및 모니터링 ===

    /**
     * 성능 카운터 시작
     */
    UFUNCTION(BlueprintCallable, Category = "Performance Monitoring")
    static void StartPerformanceCounter(const FString& CounterName);

    /**
     * 성능 카운터 종료 및 결과 로깅
     */
    UFUNCTION(BlueprintCallable, Category = "Performance Monitoring")
    static float EndPerformanceCounter(const FString& CounterName);

    /**
     * 메모리 사용량 모니터링
     */
    UFUNCTION(BlueprintCallable, Category = "Performance Monitoring")
    static void LogMemoryUsage();

    /**
     * CPU 사용률 모니터링
     */
    UFUNCTION(BlueprintCallable, Category = "Performance Monitoring")
    static float GetCurrentCPUUsage();

private:
    // 성능 카운터 저장소
    static TMap<FString, double> PerformanceCounters;

    // 메모리 풀 관리
    static TMap<FString, FHighPerformanceObjectPool> ObjectPools;
};

/**
 * UE5 호환 고성능 메모리 관리자
 * 최적화 기법을 UE5 시스템과 안전하게 통합
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSAdvancedMemoryManager : public UObject
{
    GENERATED_BODY()

public:
    UHSAdvancedMemoryManager();

    /**
     * 타입별 메모리 풀 생성 (최적화 기법)
     * Blueprint에서 안전하게 사용 가능
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced Memory")
    static void CreateTypedPool(const FString& PoolName, int32 ObjectSize, int32 MaxObjects);

    /**
     * 메모리 풀에서 오브젝트 할당
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced Memory")
    static bool AllocateFromPool(const FString& PoolName, int32& OutIndex);

    /**
     * 메모리 풀에 오브젝트 반환
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced Memory")
    static bool DeallocateToPool(const FString& PoolName, int32 Index);

    /**
     * 풀 사용 통계 조회
     */
    UFUNCTION(BlueprintPure, Category = "Advanced Memory")
    static float GetPoolUsageRatio(const FString& PoolName);

    /**
     * 모든 풀 정리 (레벨 전환 시 사용)
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced Memory")
    static void CleanupAllPools();

private:
    // UE5 스타일 타입 안전 풀 관리
    static TMap<FString, FHighPerformanceObjectPool> TypedPools;
};

/**
 * RAII 패턴을 활용한 자동 리소스 관리
 * UE5 환경에서 안전한 스마트 포인터 스타일 구현
 * 주의: 이 클래스는 C++에서만 사용하며 Blueprint 노출 안함
 */
class HUNTINGSPIRIT_API FHSSmartPoolHandle
{
private:
    FString PoolName;
    int32 ObjectIndex;
    bool bIsValid;

public:
    FHSSmartPoolHandle()
        : PoolName(TEXT(""))
        , ObjectIndex(INDEX_NONE)
        , bIsValid(false)
    {
    }

    FHSSmartPoolHandle(const FString& InPoolName, int32 InIndex)
        : PoolName(InPoolName)
        , ObjectIndex(InIndex)
        , bIsValid(true)
    {
    }

    // 복사 생성자 (리소스 공유 방지)
    FHSSmartPoolHandle(const FHSSmartPoolHandle& Other) = delete;
    FHSSmartPoolHandle& operator=(const FHSSmartPoolHandle& Other) = delete;

    // 이동 생성자 (최적화)
    FHSSmartPoolHandle(FHSSmartPoolHandle&& Other) noexcept
        : PoolName(MoveTemp(Other.PoolName))
        , ObjectIndex(Other.ObjectIndex)
        , bIsValid(Other.bIsValid)
    {
        Other.Reset();
    }

    FHSSmartPoolHandle& operator=(FHSSmartPoolHandle&& Other) noexcept
    {
        if (this != &Other)
        {
            Release();
            
            PoolName = MoveTemp(Other.PoolName);
            ObjectIndex = Other.ObjectIndex;
            bIsValid = Other.bIsValid;
            
            Other.Reset();
        }
        return *this;
    }

    // 소멸자에서 자동 정리 (RAII 패턴)
    ~FHSSmartPoolHandle()
    {
        Release();
    }

    /**
     * 핸들이 유효한지 확인
     */
    FORCEINLINE bool IsValid() const { return bIsValid && ObjectIndex != INDEX_NONE; }

    /**
     * 오브젝트 인덱스 반환
     */
    FORCEINLINE int32 GetIndex() const { return IsValid() ? ObjectIndex : INDEX_NONE; }

    /**
     * 풀 이름 반환
     */
    FORCEINLINE FString GetPoolName() const { return PoolName; }

    /**
     * 리소스 해제
     */
    void Release();

private:
    void Reset()
    {
        PoolName = TEXT("");
        ObjectIndex = INDEX_NONE;
        bIsValid = false;
    }
};

/**
 * Blueprint 호환 스마트 핸들 래퍼
 * Blueprint와 C++ 간 안전한 인터페이스 제공
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSBlueprintPoolHandle
{
    GENERATED_BODY()

private:
    UPROPERTY()
    FString PoolName;

    UPROPERTY()
    int32 ObjectIndex;

    UPROPERTY()
    bool bIsValid;

public:
    FHSBlueprintPoolHandle()
        : PoolName(TEXT(""))
        , ObjectIndex(INDEX_NONE)
        , bIsValid(false)
    {
    }

    FHSBlueprintPoolHandle(const FString& InPoolName, int32 InIndex)
        : PoolName(InPoolName)
        , ObjectIndex(InIndex)
        , bIsValid(true)
    {
    }

    /**
     * 핸들이 유효한지 확인
     */
    FORCEINLINE bool IsValid() const { return bIsValid && ObjectIndex != INDEX_NONE; }

    /**
     * 오브젝트 인덱스 반환
     */
    FORCEINLINE int32 GetIndex() const { return IsValid() ? ObjectIndex : INDEX_NONE; }

    /**
     * 풀 이름 반환
     */
    FORCEINLINE FString GetPoolName() const { return PoolName; }

    /**
     * 수동 리소스 해제 (Blueprint용)
     */
    void Release()
    {
        if (IsValid())
        {
            UHSAdvancedMemoryManager::DeallocateToPool(PoolName, ObjectIndex);
            Reset();
        }
    }

private:
    void Reset()
    {
        PoolName = TEXT("");
        ObjectIndex = INDEX_NONE;
        bIsValid = false;
    }
};