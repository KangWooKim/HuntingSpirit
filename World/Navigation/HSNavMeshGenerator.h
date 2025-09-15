// HSNavMeshGenerator.h
// 네비게이션 메시 동적 생성을 담당하는 메인 클래스
// UE5 Navigation System을 활용한 절차적 월드 네비게이션 메시 생성

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/AsyncWork.h"
#include "HSNavMeshGenerator.generated.h"

class UNavigationSystemV1;
class ANavMeshBoundsVolume;
class ARecastNavMesh;

// 네비게이션 메시 생성 작업 정보
USTRUCT(BlueprintType)
struct FHSNavMeshBuildTask
{
    GENERATED_BODY()

    // 빌드할 영역의 경계
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FBox BuildBounds;

    // 작업 우선순위 (낮을수록 높은 우선순위)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority;

    // 작업 유형 (전체 빌드, 부분 업데이트 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TaskType;

    // 작업 식별자
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid TaskID;

    FHSNavMeshBuildTask()
    {
        BuildBounds = FBox(ForceInit);
        Priority = 100;
        TaskType = 0;
        TaskID = FGuid::NewGuid();
    }

    FHSNavMeshBuildTask(const FBox& InBounds, int32 InPriority = 100, int32 InTaskType = 0)
        : BuildBounds(InBounds), Priority(InPriority), TaskType(InTaskType)
    {
        TaskID = FGuid::NewGuid();
    }

    bool operator<(const FHSNavMeshBuildTask& Other) const
    {
        return Priority < Other.Priority;
    }
};

// 네비게이션 메시 생성 통계
USTRUCT(BlueprintType)
struct FHSNavMeshBuildStats
{
    GENERATED_BODY()

    // 총 빌드 시간 (밀리초)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalBuildTimeMs;

    // 완료된 작업 수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CompletedTasks;

    // 실패한 작업 수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FailedTasks;

    // 생성된 네비게이션 메시 크기 (제곱미터)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float GeneratedAreaSize;

    FHSNavMeshBuildStats()
    {
        TotalBuildTimeMs = 0.0f;
        CompletedTasks = 0;
        FailedTasks = 0;
        GeneratedAreaSize = 0.0f;
    }
};

// 비동기 네비게이션 메시 빌드 작업
class FHSAsyncNavMeshBuildTask : public FNonAbandonableTask
{
    friend class FAutoDeleteAsyncTask<FHSAsyncNavMeshBuildTask>;

public:
    FHSAsyncNavMeshBuildTask(const FHSNavMeshBuildTask& InTask, UWorld* InWorld);

    // 작업 실행
    void DoWork();

    // 작업 식별
    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FHSAsyncNavMeshBuildTask, STATGROUP_ThreadPoolAsyncTasks);
    }

private:
    FHSNavMeshBuildTask BuildTask;
    TWeakObjectPtr<UWorld> WorldPtr;
    bool bTaskCompleted;
    FString ErrorMessage;
};

/**
 * HSNavMeshGenerator
 * 절차적으로 생성된 월드에서 동적으로 네비게이션 메시를 생성하고 관리하는 컴포넌트
 * 
 * 주요 기능:
 * - 실시간 네비게이션 메시 생성 및 업데이트
 * - 부분적 네비게이션 메시 재구축으로 성능 최적화
 * - 멀티스레딩을 통한 비동기 네비게이션 메시 생성
 * - 공간 분할을 통한 효율적인 관리
 * - 메모리 풀링을 통한 메모리 최적화
 * - 네트워크 동기화 지원
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSNavMeshGenerator : public UActorComponent
{
    GENERATED_BODY()

public:
    UHSNavMeshGenerator();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * 지정된 영역에 네비게이션 메시를 생성합니다
     * @param BuildBounds 생성할 영역의 경계
     * @param Priority 작업 우선순위 (낮을수록 높은 우선순위)
     * @param bAsyncBuild 비동기 빌드 여부
     * @return 빌드 작업 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    FGuid GenerateNavMeshInBounds(const FBox& BuildBounds, int32 Priority = 100, bool bAsyncBuild = true);

    /**
     * 특정 위치 주변에 네비게이션 메시를 생성합니다
     * @param Location 중심 위치
     * @param Radius 생성 반경
     * @param Priority 작업 우선순위
     * @param bAsyncBuild 비동기 빌드 여부
     * @return 빌드 작업 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    FGuid GenerateNavMeshAroundLocation(const FVector& Location, float Radius = 1000.0f, int32 Priority = 100, bool bAsyncBuild = true);

    /**
     * 네비게이션 메시를 부분적으로 업데이트합니다
     * @param UpdateBounds 업데이트할 영역
     * @param bForceRebuild 강제 재구축 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    void UpdateNavMeshInBounds(const FBox& UpdateBounds, bool bForceRebuild = false);

    /**
     * 모든 네비게이션 메시를 다시 빌드합니다
     * @param bClearExisting 기존 메시 삭제 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    void RebuildAllNavMesh(bool bClearExisting = true);

    /**
     * 지정된 작업을 취소합니다
     * @param TaskID 취소할 작업 ID
     * @return 취소 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    bool CancelBuildTask(const FGuid& TaskID);

    /**
     * 모든 대기 중인 작업을 취소합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    void CancelAllPendingTasks();

    /**
     * 네비게이션 메시 생성 통계를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Navigation|NavMesh Generation")
    FHSNavMeshBuildStats GetBuildStats() const { return BuildStats; }

    /**
     * 현재 대기 중인 작업 수를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Navigation|NavMesh Generation")
    int32 GetPendingTaskCount() const { return PendingTasks.Num(); }

    /**
     * 네비게이션 메시 생성이 활성화되어 있는지 확인합니다
     */
    UFUNCTION(BlueprintPure, Category = "Navigation|NavMesh Generation")
    bool IsNavMeshGenerationEnabled() const { return bEnableNavMeshGeneration; }

    /**
     * 네비게이션 메시 생성을 활성화/비활성화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|NavMesh Generation")
    void SetNavMeshGenerationEnabled(bool bEnabled);

protected:
    /**
     * Navigation System을 초기화합니다
     */
    UFUNCTION(CallInEditor)
    void InitializeNavigationSystem();

    /**
     * 다음 빌드 작업을 처리합니다
     */
    void ProcessNextBuildTask();

    /**
     * 비동기 작업 완료를 확인합니다
     */
    void CheckAsyncTaskCompletion();

    /**
     * 네비게이션 메시 경계를 설정합니다 (UE5 호환)
     * @param Bounds 경계 영역
     * @return 설정 성공 시 nullptr 반환 (더 이상 볼륨 생성하지 않음)
     */
    AActor* CreateNavMeshBoundsVolume(const FBox& Bounds);

    /**
     * 공간 분할을 통한 최적화된 빌드 영역 계산
     * @param OriginalBounds 원본 경계
     * @return 최적화된 빌드 영역 배열
     */
    TArray<FBox> CalculateOptimalBuildRegions(const FBox& OriginalBounds);

    /**
     * 메모리 사용량을 모니터링하고 최적화합니다
     */
    void OptimizeMemoryUsage();

    /**
     * 네비게이션 메시 품질을 검증합니다
     * @param TestBounds 검증할 영역
     * @return 품질 점수 (0.0 ~ 1.0)
     */
    float ValidateNavMeshQuality(const FBox& TestBounds);

private:
    // Navigation System 참조
    UPROPERTY()
    TWeakObjectPtr<UNavigationSystemV1> NavigationSystem;

    // RecastNavMesh 참조
    UPROPERTY()
    TWeakObjectPtr<ARecastNavMesh> RecastNavMesh;

    // 대기 중인 빌드 작업 큐 (우선순위 큐)
    TArray<FHSNavMeshBuildTask> PendingTasks;

    // 현재 실행 중인 비동기 작업들
    TArray<FAsyncTask<FHSAsyncNavMeshBuildTask>*> AsyncTasks;

    // 더 이상 사용하지 않음 (UE5 호환성)
    // UPROPERTY()
    // TArray<TWeakObjectPtr<ANavMeshBoundsVolume>> CreatedBoundsVolumes;

    // 빌드 통계
    FHSNavMeshBuildStats BuildStats;

public:
    // === 설정 가능한 속성들 ===

    // 네비게이션 메시 생성 활성화 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMesh Generation")
    bool bEnableNavMeshGeneration;

    // 최대 동시 빌드 작업 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMesh Generation", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxConcurrentTasks;

    // 빌드 작업 처리 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMesh Generation", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float TaskProcessingInterval;

    // 자동 메모리 최적화 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMesh Generation", meta = (ClampMin = "10.0", ClampMax = "300.0"))
    float MemoryOptimizationInterval;

    // 최대 빌드 영역 크기 (제곱미터)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMesh Generation", meta = (ClampMin = "1000.0"))
    float MaxBuildAreaSize;

    // 네비게이션 메시 품질 임계값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMesh Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float QualityThreshold;

    // 디버그 시각화 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugVisualization;

    // 디버그 로그 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging;

private:
    // 내부 타이머들
    float TaskProcessingTimer;
    float MemoryOptimizationTimer;

    // 스레드 안전성을 위한 변수들
    FThreadSafeBool bIsProcessingTasks;
    FCriticalSection TaskQueueCriticalSection;
    FCriticalSection StatsUpdateCriticalSection;
};
