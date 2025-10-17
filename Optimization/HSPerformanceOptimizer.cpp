// 사냥의 영혼(HuntingSpirit) 게임의 성능 최적화 유틸리티 구현
// 다양한 최적화 기법들을 다룬다

#include "HSPerformanceOptimizer.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadManager.h"
#include "Misc/FileHelper.h"
#include "Async/ParallelFor.h"
#include "Math/UnrealMathVectorCommon.h"
#include "Logging/LogMacros.h"

// SIMD 최적화를 위한 헤더 추가
#if PLATFORM_WINDOWS && defined(_MSC_VER)
    #include <immintrin.h>
    #include <emmintrin.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHSPerformance, Log, All);

// 정적 멤버 초기화
TMap<FString, double> UHSPerformanceOptimizer::PerformanceCounters;
TMap<FString, FHighPerformanceObjectPool> UHSPerformanceOptimizer::ObjectPools;

// ========================================
// FSIMDVectorOperations 구현
// ========================================

void FSIMDVectorOperations::CalculateDistancesBatch(
    const TArray<FVector>& SourcePositions,
    const FVector& TargetPosition,
    TArray<float>& OutDistances)
{
    const int32 Count = SourcePositions.Num();
    OutDistances.SetNumUninitialized(Count);

    // SIMD 최적화: 4개씩 묶어서 처리
    const int32 SIMDCount = Count & ~3; // 4의 배수로 맞춤
    
    for (int32 i = 0; i < SIMDCount; i += 4)
    {
        // 루프를 벡터화할 수 있도록 4개씩 묶어서 처리
        const FVector* SourcePtr = &SourcePositions[i];
        float* DistancePtr = &OutDistances[i];

        for (int32 j = 0; j < 4; ++j)
        {
            const FVector Delta = SourcePtr[j] - TargetPosition;
            DistancePtr[j] = Delta.Size();
        }
    }

    // 나머지 요소들 처리
    for (int32 i = SIMDCount; i < Count; ++i)
    {
        const FVector Delta = SourcePositions[i] - TargetPosition;
        OutDistances[i] = Delta.Size();
    }
}

void FSIMDVectorOperations::NormalizeVectorsBatch(TArray<FVector>& InOutVectors)
{
    const int32 Count = InOutVectors.Num();
    
    // 병렬 처리로 성능 향상
    ParallelFor(Count, [&InOutVectors](int32 Index)
    {
        FVector& Vector = InOutVectors[Index];
        const float Length = Vector.Size();
        
        // 제로 디비전을 피하기 위한 보호 코드
        if (Length > SMALL_NUMBER)
        {
            const float InvLength = 1.0f / Length;
            Vector *= InvLength;
        }
        else
        {
            Vector = FVector::ZeroVector;
        }
    });
}

void FSIMDVectorOperations::DotProductBatch(
    const TArray<FVector>& VectorsA,
    const TArray<FVector>& VectorsB,
    TArray<float>& OutResults)
{
    const int32 Count = FMath::Min(VectorsA.Num(), VectorsB.Num());
    OutResults.SetNumUninitialized(Count);

    // 병렬 처리로 대량 데이터 고속 처리
    ParallelFor(Count, [&VectorsA, &VectorsB, &OutResults](int32 Index)
    {
        OutResults[Index] = FVector::DotProduct(VectorsA[Index], VectorsB[Index]);
    });
}

// ========================================
// FCompressedPlayerData 구현
// ========================================

void FCompressedPlayerData::CompressFromVector(const FVector& Position)
{
    // 월드 좌표를 압축 범위로 변환하여 정밀도와 메모리를 균형 맞춘다
    // 일반적으로 -32768 ~ 32767 범위를 -1000 ~ 1000 월드 단위로 매핑
    constexpr float CompressionScale = 32.767f; // 1000 / 32767
    
    CompressedX = static_cast<uint16>(FMath::Clamp(Position.X / CompressionScale + 32768, 0.0f, 65535.0f));
    CompressedY = static_cast<uint16>(FMath::Clamp(Position.Y / CompressionScale + 32768, 0.0f, 65535.0f));
    CompressedZ = static_cast<uint16>(FMath::Clamp(Position.Z / CompressionScale + 32768, 0.0f, 65535.0f));
}

FVector FCompressedPlayerData::DecompressToVector() const
{
    constexpr float CompressionScale = 32.767f;
    
    return FVector(
        (static_cast<float>(CompressedX) - 32768.0f) * CompressionScale,
        (static_cast<float>(CompressedY) - 32768.0f) * CompressionScale,
        (static_cast<float>(CompressedZ) - 32768.0f) * CompressionScale
    );
}

void FCompressedPlayerData::CompressFromRotator(const FRotator& Rotation)
{
    FQuat Quat = Rotation.Quaternion();
    if (!Quat.IsNormalized())
    {
        Quat.Normalize();
    }

    float Components[4] = {static_cast<float>(Quat.X), static_cast<float>(Quat.Y),
                           static_cast<float>(Quat.Z), static_cast<float>(Quat.W)};

    int32 LargestIndex = 0;
    float MaxAbsValue = FMath::Abs(Components[0]);
    for (int32 i = 1; i < 4; ++i)
    {
        const float AbsValue = FMath::Abs(Components[i]);
        if (AbsValue > MaxAbsValue)
        {
            MaxAbsValue = AbsValue;
            LargestIndex = i;
        }
    }

    if (Components[LargestIndex] < 0.0f)
    {
        for (float& Component : Components)
        {
            Component = -Component;
        }
    }

    CompressedRotation = static_cast<uint32>(LargestIndex) << 30;

    int32 BitOffset = 20;
    for (int32 i = 0; i < 4; ++i)
    {
        if (i == LargestIndex)
        {
            continue;
        }

        const float ClampedValue = FMath::Clamp(Components[i], -1.0f, 1.0f);
        const uint32 Quantized = static_cast<uint32>(FMath::RoundToInt((ClampedValue + 1.0f) * 511.5f)) & 0x3FF;
        CompressedRotation |= Quantized << BitOffset;
        BitOffset -= 10;
    }
}

FRotator FCompressedPlayerData::DecompressToRotator() const
{
    const int32 LargestIndex = (CompressedRotation >> 30) & 0x3;

    float Components[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    int32 BitOffset = 20;
    float SumOfSquares = 0.0f;
    for (int32 i = 0; i < 4; ++i)
    {
        if (i == LargestIndex)
        {
            continue;
        }

        const uint32 QuantizedComponent = (CompressedRotation >> BitOffset) & 0x3FF;
        const float Value = (static_cast<float>(QuantizedComponent) / 511.5f) - 1.0f;
        Components[i] = Value;
        SumOfSquares += Value * Value;
        BitOffset -= 10;
    }

    SumOfSquares = FMath::Min(SumOfSquares, 1.0f);
    Components[LargestIndex] = FMath::Sqrt(FMath::Max(0.0f, 1.0f - SumOfSquares));

    FQuat Quat(Components[0], Components[1], Components[2], Components[3]);
    if (!Quat.IsNormalized())
    {
        Quat.Normalize();
    }
    return Quat.Rotator();
}

void FCompressedPlayerData::SetStatusFlag(uint8 FlagIndex, bool bValue)
{
    if (FlagIndex < 8)
    {
        if (bValue)
        {
            StatusFlags |= (1 << FlagIndex);
        }
        else
        {
            StatusFlags &= ~(1 << FlagIndex);
        }
    }
}

bool FCompressedPlayerData::GetStatusFlag(uint8 FlagIndex) const
{
    return FlagIndex < 8 ? (StatusFlags & (1 << FlagIndex)) != 0 : false;
}

// ========================================
// FCacheOptimizedPlayerArray 구현
// ========================================

void FCacheOptimizedPlayerArray::RemovePlayerAtIndex(int32 Index)
{
    if (Index >= 0 && Index < GetPlayerCount())
    {
        // 스왑-앤-팝으로 O(1) 시간에 제거
        const int32 LastIndex = GetPlayerCount() - 1;
        
        if (Index != LastIndex)
        {
            Positions[Index] = Positions[LastIndex];
            Velocities[Index] = Velocities[LastIndex];
            HealthValues[Index] = HealthValues[LastIndex];
            TeamIDs[Index] = TeamIDs[LastIndex];
            StatusFlags[Index] = StatusFlags[LastIndex];
        }
        
        Positions.RemoveAt(LastIndex);
        Velocities.RemoveAt(LastIndex);
        HealthValues.RemoveAt(LastIndex);
        TeamIDs.RemoveAt(LastIndex);
        StatusFlags.RemoveAt(LastIndex);
    }
}

int32 FCacheOptimizedPlayerArray::AddPlayer(const FVector& Position, const FVector& Velocity, 
                                          float Health, int32 TeamID, uint8 Status)
{
    const int32 NewIndex = GetPlayerCount();
    
    Positions.Add(Position);
    Velocities.Add(Velocity);
    HealthValues.Add(Health);
    TeamIDs.Add(TeamID);
    StatusFlags.Add(Status);
    
    return NewIndex;
}

bool FCacheOptimizedPlayerArray::ValidateArrayConsistency() const
{
    const int32 Count = Positions.Num();
    return (Velocities.Num() == Count && 
            HealthValues.Num() == Count && 
            TeamIDs.Num() == Count && 
            StatusFlags.Num() == Count);
}

// ========================================
// FHighPerformanceObjectPool 구현
// ========================================

int32 FHighPerformanceObjectPool::AllocateIndex()
{
    if (FreeIndices.Num() > 0)
    {
        // 프리 리스트를 활용해 O(1) 시간에 할당
        const int32 Index = FreeIndices.Pop();
        ActiveFlags[Index] = true;
        return Index;
    }
    else if (CurrentPoolSize < MaxPoolSize)
    {
        // 풀 확장
        const int32 NewIndex = CurrentPoolSize++;
        ActiveFlags.SetNum(CurrentPoolSize);
        ActiveFlags[NewIndex] = true;
        return NewIndex;
    }
    
    return INDEX_NONE; // 풀이 가득 참
}

void FHighPerformanceObjectPool::DeallocateIndex(int32 Index)
{
    if (Index >= 0 && Index < CurrentPoolSize && ActiveFlags[Index])
    {
        ActiveFlags[Index] = false;
        FreeIndices.Push(Index);
    }
}

bool FHighPerformanceObjectPool::IsIndexActive(int32 Index) const
{
    return Index >= 0 && Index < CurrentPoolSize && ActiveFlags[Index];
}

int32 FHighPerformanceObjectPool::GetActiveCount() const
{
    return CurrentPoolSize - FreeIndices.Num();
}

void FHighPerformanceObjectPool::ResizePool(int32 NewMaxSize)
{
    MaxPoolSize = NewMaxSize;
    ActiveFlags.SetNum(FMath::Min(CurrentPoolSize, MaxPoolSize));
    
    // 범위를 벗어난 프리 인덱스들 제거
    FreeIndices.RemoveAll([this](int32 Index) 
    { 
        return Index >= MaxPoolSize; 
    });
}

void FHighPerformanceObjectPool::ResetPool()
{
    FreeIndices.Empty();
    ActiveFlags.Empty();
    CurrentPoolSize = 0;
    
    // 미리 메모리 예약
    FreeIndices.Reserve(MaxPoolSize);
    ActiveFlags.Reserve(MaxPoolSize);
}

// ========================================
// UHSPerformanceOptimizer 구현
// ========================================

UHSPerformanceOptimizer::UHSPerformanceOptimizer()
{
    // 생성자에서 초기화 작업 수행
}

int32 UHSPerformanceOptimizer::GetOptimalStructAlignment(int32 StructSize)
{
    // 2의 거듭제곱 정렬로 캐시 라인 효율을 확보
    if (StructSize <= 8) return 8;
    if (StructSize <= 16) return 16;
    if (StructSize <= 32) return 32;
    if (StructSize <= 64) return 64;
    return 128; // 대부분의 현대 CPU 캐시 라인 크기
}

void UHSPerformanceOptimizer::PreallocateMemoryPools(int32 ExpectedObjectCount)
{
    // 런타임 할당을 줄이기 위해 메모리를 미리 예약한다
    const int32 PoolSize = FMath::RoundUpToPowerOfTwo(ExpectedObjectCount * 2); // 버퍼 여유 확보
    
    // 각 타입별 풀 초기화
    for (auto& PoolPair : ObjectPools)
    {
        PoolPair.Value.ResizePool(PoolSize);
    }
    
    UE_LOG(LogHSPerformance, Log, TEXT("메모리 풀 사전 할당 완료: %d 오브젝트"), PoolSize);
}

void UHSPerformanceOptimizer::PrefetchMemory(const void* Address)
{
    // CPU 캐시 프리페치로 메모리 접근 지연을 줄인다
#if PLATFORM_WINDOWS && defined(_MSC_VER)
    _mm_prefetch(static_cast<const char*>(Address), _MM_HINT_T0);
#elif defined(__GNUC__)
    __builtin_prefetch(Address, 0, 3);
#endif
}

void UHSPerformanceOptimizer::ProcessPlayerUpdatesBatch(
    const TArray<FVector>& Positions,
    const TArray<FVector>& Velocities,
    float DeltaTime,
    TArray<FVector>& OutNewPositions)
{
    const int32 Count = FMath::Min(Positions.Num(), Velocities.Num());
    OutNewPositions.SetNumUninitialized(Count);
    
    // 배치 처리로 함수 호출 오버헤드를 최소화
    for (int32 i = 0; i < Count; ++i)
    {
        OutNewPositions[i] = Positions[i] + Velocities[i] * DeltaTime;
    }
}

void UHSPerformanceOptimizer::ProcessPlayerUpdatesParallel(
    const TArray<FVector>& Positions,
    const TArray<FVector>& Velocities,
    float DeltaTime,
    TArray<FVector>& OutNewPositions)
{
    const int32 Count = FMath::Min(Positions.Num(), Velocities.Num());
    OutNewPositions.SetNumUninitialized(Count);
    
    // 멀티스레드 병렬 처리로 성능을 높인다
    ParallelFor(Count, [&Positions, &Velocities, DeltaTime, &OutNewPositions](int32 Index)
    {
        OutNewPositions[Index] = Positions[Index] + Velocities[Index] * DeltaTime;
    });
}

TArray<uint8> UHSPerformanceOptimizer::CompressDeltaData(
    const TArray<uint8>& PreviousData,
    const TArray<uint8>& CurrentData)
{
    TArray<uint8> DeltaData;
    const int32 Count = FMath::Min(PreviousData.Num(), CurrentData.Num());
    
    DeltaData.Reserve(Count / 2); // 평균적으로 50% 압축률 예상
    
    // XOR 델타 인코딩으로 네트워크 대역폭을 절약
    for (int32 i = 0; i < Count; ++i)
    {
        const uint8 Delta = CurrentData[i] ^ PreviousData[i];
        if (Delta != 0)
        {
            DeltaData.Add(static_cast<uint8>(i & 0xFF)); // 인덱스 하위 바이트
            DeltaData.Add(static_cast<uint8>((i >> 8) & 0xFF)); // 인덱스 상위 바이트
            DeltaData.Add(Delta); // 델타 값
        }
    }
    
    return DeltaData;
}

uint32 UHSPerformanceOptimizer::PackBoolArrayToBits(const TArray<bool>& BoolArray)
{
    uint32 PackedBits = 0;
    const int32 Count = FMath::Min(BoolArray.Num(), 32);
    
    // 비트 패킹으로 메모리와 네트워크 효율을 높인다
    for (int32 i = 0; i < Count; ++i)
    {
        if (BoolArray[i])
        {
            PackedBits |= (1U << i);
        }
    }
    
    return PackedBits;
}

TArray<bool> UHSPerformanceOptimizer::UnpackBitsToBoolean(uint32 PackedBits, int32 BoolCount)
{
    TArray<bool> BoolArray;
    BoolArray.SetNumUninitialized(BoolCount);
    
    for (int32 i = 0; i < BoolCount && i < 32; ++i)
    {
        BoolArray[i] = (PackedBits & (1U << i)) != 0;
    }
    
    return BoolArray;
}

void UHSPerformanceOptimizer::StartPerformanceCounter(const FString& CounterName)
{
    PerformanceCounters.Add(CounterName, FPlatformTime::Seconds());
}

float UHSPerformanceOptimizer::EndPerformanceCounter(const FString& CounterName)
{
    if (const double* StartTime = PerformanceCounters.Find(CounterName))
    {
        const double ElapsedTime = FPlatformTime::Seconds() - *StartTime;
        PerformanceCounters.Remove(CounterName);
        
        UE_LOG(LogHSPerformance, Log, TEXT("성능 카운터 [%s]: %.4f ms"), 
               *CounterName, ElapsedTime * 1000.0);
        
        return static_cast<float>(ElapsedTime);
    }
    
    return 0.0f;
}

void UHSPerformanceOptimizer::LogMemoryUsage()
{
    // 메모리 사용량을 기록해 최적화 포인트를 식별한다
    const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    
    UE_LOG(LogHSPerformance, Log, TEXT("=== 메모리 사용량 현황 ==="));
    UE_LOG(LogHSPerformance, Log, TEXT("물리 메모리 사용: %.2f MB"), 
           MemStats.UsedPhysical / (1024.0f * 1024.0f));
    UE_LOG(LogHSPerformance, Log, TEXT("가상 메모리 사용: %.2f MB"), 
           MemStats.UsedVirtual / (1024.0f * 1024.0f));
    UE_LOG(LogHSPerformance, Log, TEXT("사용 가능한 물리 메모리: %.2f MB"), 
           MemStats.AvailablePhysical / (1024.0f * 1024.0f));
}

float UHSPerformanceOptimizer::GetCurrentCPUUsage()
{
    static double LastUpdateTime = 0.0;
    static float CachedUsage = 0.0f;

    const double Now = FPlatformTime::Seconds();
    if (LastUpdateTime > 0.0 && (Now - LastUpdateTime) < 0.25)
    {
        return CachedUsage;
    }

    const auto StoreResult = [&](float Value) -> float
    {
        CachedUsage = FMath::Clamp(Value, 0.0f, 100.0f);
        LastUpdateTime = Now;
        return CachedUsage;
    };

    const float DeltaSeconds = (LastUpdateTime > 0.0) ? static_cast<float>(Now - LastUpdateTime) : 0.0f;
    FPlatformTime::UpdateCPUTime(DeltaSeconds);

    const FCPUTime CpuSample = FPlatformTime::GetCPUTime();
    float CpuUsage = CpuSample.CPUTimePct;
    if (CpuUsage <= 0.0f && CpuSample.CPUTimePctRelative > 0.0f)
    {
        CpuUsage = CpuSample.CPUTimePctRelative;
    }

    return StoreResult(CpuUsage);
}

int32 UHSPerformanceOptimizer::PackBoolArrayToBits_BP(const TArray<bool>& BoolArray)
{
    // uint32 함수를 호출하고 int32로 캐스팅
    const uint32 Result = PackBoolArrayToBits(BoolArray);
    return static_cast<int32>(Result);
}

TArray<bool> UHSPerformanceOptimizer::UnpackBitsToBoolean_BP(int32 PackedBits, int32 BoolCount)
{
    // int32를 uint32로 캐스팅하여 원래 함수 호출
    const uint32 UnsignedBits = static_cast<uint32>(PackedBits);
    return UnpackBitsToBoolean(UnsignedBits, BoolCount);
}

// ========================================
// UHSAdvancedMemoryManager 구현
// ========================================

// 정적 멤버 초기화
TMap<FString, FHighPerformanceObjectPool> UHSAdvancedMemoryManager::TypedPools;

UHSAdvancedMemoryManager::UHSAdvancedMemoryManager()
{
    // 생성자에서 초기화 작업 수행
}

void UHSAdvancedMemoryManager::CreateTypedPool(const FString& PoolName, int32 ObjectSize, int32 MaxObjects)
{
    if (PoolName.IsEmpty() || ObjectSize <= 0 || MaxObjects <= 0)
    {
        UE_LOG(LogHSPerformance, Warning, TEXT("유효하지 않은 풀 생성 파라미터: %s, Size: %d, Max: %d"), 
               *PoolName, ObjectSize, MaxObjects);
        return;
    }

    // 기존 풀이 있으면 경고 후 교체
    if (TypedPools.Contains(PoolName))
    {
        UE_LOG(LogHSPerformance, Warning, TEXT("기존 풀 [%s]을 새 풀로 교체합니다"), *PoolName);
    }

    // 새로운 풀 생성
    FHighPerformanceObjectPool NewPool;
    NewPool.ResizePool(MaxObjects);
    
    TypedPools.Add(PoolName, NewPool);
    
    UE_LOG(LogHSPerformance, Log, TEXT("메모리 풀 생성 완료: [%s] - 최대 %d개 오브젝트 (크기: %d 바이트)"), 
           *PoolName, MaxObjects, ObjectSize);
}

bool UHSAdvancedMemoryManager::AllocateFromPool(const FString& PoolName, int32& OutIndex)
{
    OutIndex = INDEX_NONE;

    if (PoolName.IsEmpty())
    {
        return false;
    }

    FHighPerformanceObjectPool* Pool = TypedPools.Find(PoolName);
    if (!Pool)
    {
        UE_LOG(LogHSPerformance, Warning, TEXT("존재하지 않는 풀: %s"), *PoolName);
        return false;
    }

    OutIndex = Pool->AllocateIndex();
    
    if (OutIndex == INDEX_NONE)
    {
        UE_LOG(LogHSPerformance, Warning, TEXT("풀 [%s]에서 할당 실패 - 풀이 가득 참"), *PoolName);
        return false;
    }

    return true;
}

bool UHSAdvancedMemoryManager::DeallocateToPool(const FString& PoolName, int32 Index)
{
    if (PoolName.IsEmpty() || Index == INDEX_NONE)
    {
        return false;
    }

    FHighPerformanceObjectPool* Pool = TypedPools.Find(PoolName);
    if (!Pool)
    {
        UE_LOG(LogHSPerformance, Warning, TEXT("존재하지 않는 풀: %s"), *PoolName);
        return false;
    }

    if (!Pool->IsIndexActive(Index))
    {
        UE_LOG(LogHSPerformance, Warning, TEXT("풀 [%s]에서 비활성 인덱스 해제 시도: %d"), *PoolName, Index);
        return false;
    }

    Pool->DeallocateIndex(Index);
    return true;
}

float UHSAdvancedMemoryManager::GetPoolUsageRatio(const FString& PoolName)
{
    if (PoolName.IsEmpty())
    {
        return 0.0f;
    }

    const FHighPerformanceObjectPool* Pool = TypedPools.Find(PoolName);
    if (!Pool)
    {
        return 0.0f;
    }

    const int32 ActiveCount = Pool->GetActiveCount();
    const int32 TotalCapacity = Pool->GetMaxPoolSize();
    
    if (TotalCapacity <= 0)
    {
        return 0.0f;
    }

    const float UsageRatio = static_cast<float>(ActiveCount) / static_cast<float>(TotalCapacity);
    return FMath::Clamp(UsageRatio, 0.0f, 1.0f);
}

void UHSAdvancedMemoryManager::CleanupAllPools()
{
    int32 PoolCount = TypedPools.Num();
    int32 TotalActiveObjects = 0;

    // 모든 풀의 통계 수집
    for (auto& PoolPair : TypedPools)
    {
        TotalActiveObjects += PoolPair.Value.GetActiveCount();
        PoolPair.Value.ResetPool();
    }

    // 풀 제거
    TypedPools.Empty();
    
    UE_LOG(LogHSPerformance, Log, TEXT("모든 메모리 풀 정리 완료 - %d개 풀, %d개 활성 오브젝트 해제"), 
           PoolCount, TotalActiveObjects);
}

// ========================================
// FHSSmartPoolHandle 구현
// ========================================

void FHSSmartPoolHandle::Release()
{
    if (IsValid())
    {
        UHSAdvancedMemoryManager::DeallocateToPool(PoolName, ObjectIndex);
        Reset();
    }
}
