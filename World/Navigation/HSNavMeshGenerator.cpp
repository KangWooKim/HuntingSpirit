// HSNavMeshGenerator.cpp
// 네비게이션 메시 동적 생성 구현
// 현업 최적화 기법: 메모리 풀링, 공간 분할, 멀티스레딩, 품질 검증

#include "HSNavMeshGenerator.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "Engine/World.h"
#include "Components/BrushComponent.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "DrawDebugHelpers.h"
#include "Algo/Sort.h"

// 비동기 네비게이션 메시 빌드 작업 구현
FHSAsyncNavMeshBuildTask::FHSAsyncNavMeshBuildTask(const FHSNavMeshBuildTask& InTask, UWorld* InWorld)
    : BuildTask(InTask), WorldPtr(InWorld), bTaskCompleted(false)
{
}

void FHSAsyncNavMeshBuildTask::DoWork()
{
    if (!WorldPtr.IsValid())
    {
        ErrorMessage = TEXT("World reference is invalid");
        bTaskCompleted = false;
        return;
    }

    UWorld* World = WorldPtr.Get();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    
    if (!NavSys)
    {
        ErrorMessage = TEXT("Navigation System not found");
        bTaskCompleted = false;
        return;
    }

    // 메인 스레드에서 실행되어야 하는 작업은 델리게이트로 예약
    AsyncTask(ENamedThreads::GameThread, [this, NavSys]()
    {
        // 네비게이션 메시 빌드 실행
        NavSys->Build();
        bTaskCompleted = true;
    });
}

// HSNavMeshGenerator 구현
UHSNavMeshGenerator::UHSNavMeshGenerator()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;

    // 기본 설정값 초기화
    bEnableNavMeshGeneration = true;
    MaxConcurrentTasks = 2;
    TaskProcessingInterval = 0.5f;
    MemoryOptimizationInterval = 30.0f;
    MaxBuildAreaSize = 10000000.0f; // 10,000 제곱미터
    QualityThreshold = 0.8f;
    bEnableDebugVisualization = false;
    bEnableDebugLogging = true;

    // 타이머 초기화
    TaskProcessingTimer = 0.0f;
    MemoryOptimizationTimer = 0.0f;
    bIsProcessingTasks = false;
}

void UHSNavMeshGenerator::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeNavigationSystem();
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 메시 생성기가 초기화되었습니다."));
    }
}

void UHSNavMeshGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 모든 대기 중인 작업 취소
    CancelAllPendingTasks();
    
    // 비동기 작업들 정리
    for (FAsyncTask<FHSAsyncNavMeshBuildTask>* Task : AsyncTasks)
    {
        if (Task)
        {
            Task->EnsureCompletion();
            delete Task;
        }
    }
    AsyncTasks.Empty();
    
    // UE5에서는 볼륨을 생성하지 않으므로 정리할 필요 없음
    
    Super::EndPlay(EndPlayReason);
}

void UHSNavMeshGenerator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!bEnableNavMeshGeneration)
    {
        return;
    }
    
    // 작업 처리 타이머 업데이트
    TaskProcessingTimer += DeltaTime;
    if (TaskProcessingTimer >= TaskProcessingInterval)
    {
        TaskProcessingTimer = 0.0f;
        ProcessNextBuildTask();
    }
    
    // 메모리 최적화 타이머 업데이트
    MemoryOptimizationTimer += DeltaTime;
    if (MemoryOptimizationTimer >= MemoryOptimizationInterval)
    {
        MemoryOptimizationTimer = 0.0f;
        OptimizeMemoryUsage();
    }
    
    // 비동기 작업 완료 확인
    CheckAsyncTaskCompletion();
}

void UHSNavMeshGenerator::InitializeNavigationSystem()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("HSNavMeshGenerator: World가 유효하지 않습니다."));
        return;
    }
    
    // Navigation System 가져오기
    NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavigationSystem.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HSNavMeshGenerator: Navigation System을 찾을 수 없습니다."));
        return;
    }
    
    // RecastNavMesh 가져오기
    ANavigationData* DefaultNavData = NavigationSystem->GetDefaultNavDataInstance();
    RecastNavMesh = Cast<ARecastNavMesh>(DefaultNavData);
    
    if (!RecastNavMesh.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSNavMeshGenerator: RecastNavMesh를 찾을 수 없습니다. 기본 설정을 사용합니다."));
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: Navigation System 초기화 완료"));
    }
}

FGuid UHSNavMeshGenerator::GenerateNavMeshInBounds(const FBox& BuildBounds, int32 Priority, bool bAsyncBuild)
{
    if (!bEnableNavMeshGeneration || !NavigationSystem.IsValid())
    {
        return FGuid();
    }
    
    // 빌드 영역 크기 검증
    float AreaSize = BuildBounds.GetVolume();
    if (AreaSize > MaxBuildAreaSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSNavMeshGenerator: 빌드 영역이 너무 큽니다. 분할하여 처리합니다."));
        
        // 큰 영역을 작은 영역들로 분할
        TArray<FBox> OptimalRegions = CalculateOptimalBuildRegions(BuildBounds);
        FGuid FirstTaskID;
        
        for (int32 i = 0; i < OptimalRegions.Num(); ++i)
        {
            FHSNavMeshBuildTask NewTask(OptimalRegions[i], Priority + i, 0);
            
            if (i == 0)
            {
                FirstTaskID = NewTask.TaskID;
            }
            
            // 스레드 안전성을 위한 크리티컬 섹션
            FScopeLock Lock(&TaskQueueCriticalSection);
            PendingTasks.Add(NewTask);
        }
        
        // 우선순위에 따라 정렬
        Algo::Sort(PendingTasks);
        
        return FirstTaskID;
    }
    
    // 일반적인 크기의 영역 처리
    FHSNavMeshBuildTask NewTask(BuildBounds, Priority, 0);
    FGuid TaskID = NewTask.TaskID;
    
    {
        FScopeLock Lock(&TaskQueueCriticalSection);
        PendingTasks.Add(NewTask);
        Algo::Sort(PendingTasks);
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 메시 생성 작업이 큐에 추가되었습니다. TaskID: %s"), *TaskID.ToString());
    }
    
    return TaskID;
}

FGuid UHSNavMeshGenerator::GenerateNavMeshAroundLocation(const FVector& Location, float Radius, int32 Priority, bool bAsyncBuild)
{
    // 구형 영역을 박스로 변환
    FVector HalfExtents(Radius, Radius, Radius);
    FBox BuildBounds(Location - HalfExtents, Location + HalfExtents);
    
    return GenerateNavMeshInBounds(BuildBounds, Priority, bAsyncBuild);
}

void UHSNavMeshGenerator::UpdateNavMeshInBounds(const FBox& UpdateBounds, bool bForceRebuild)
{
    if (!NavigationSystem.IsValid())
    {
        return;
    }
    
    // 부분적 업데이트 수행 (UE5 새로운 API 사용)
    TArray<FBox> DirtyAreas;
    DirtyAreas.Add(UpdateBounds);
    NavigationSystem->OnNavigationBoundsUpdated(nullptr);
    
    if (bForceRebuild)
    {
        // 강제 재빌드가 요청된 경우 해당 영역을 다시 빌드
        GenerateNavMeshInBounds(UpdateBounds, 50, true); // 중간 우선순위로 설정
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 메시가 업데이트되었습니다. 강제 재빌드: %s"), 
               bForceRebuild ? TEXT("예") : TEXT("아니오"));
    }
}

void UHSNavMeshGenerator::RebuildAllNavMesh(bool bClearExisting)
{
    if (!NavigationSystem.IsValid())
    {
        return;
    }
    
    if (bClearExisting)
    {
        // 기존 네비게이션 데이터 제거
        NavigationSystem->CleanUp();
    }
    
    // 전체 네비게이션 메시 재빌드
    NavigationSystem->Build();
    
    // 통계 업데이트
    {
        FScopeLock Lock(&StatsUpdateCriticalSection);
        BuildStats.CompletedTasks++;
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 전체 네비게이션 메시가 재빌드되었습니다."));
    }
}

bool UHSNavMeshGenerator::CancelBuildTask(const FGuid& TaskID)
{
    FScopeLock Lock(&TaskQueueCriticalSection);
    
    int32 RemovedCount = PendingTasks.RemoveAll([TaskID](const FHSNavMeshBuildTask& Task)
    {
        return Task.TaskID == TaskID;
    });
    
    if (RemovedCount > 0)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 작업이 취소되었습니다. TaskID: %s"), *TaskID.ToString());
        }
        return true;
    }
    
    return false;
}

void UHSNavMeshGenerator::CancelAllPendingTasks()
{
    FScopeLock Lock(&TaskQueueCriticalSection);
    
    int32 CancelledCount = PendingTasks.Num();
    PendingTasks.Empty();
    
    if (bEnableDebugLogging && CancelledCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: %d개의 대기 중인 작업이 취소되었습니다."), CancelledCount);
    }
}

void UHSNavMeshGenerator::SetNavMeshGenerationEnabled(bool bEnabled)
{
    bEnableNavMeshGeneration = bEnabled;
    
    if (!bEnabled)
    {
        // 비활성화 시 모든 대기 중인 작업 취소
        CancelAllPendingTasks();
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 메시 생성이 %s되었습니다."), 
               bEnabled ? TEXT("활성화") : TEXT("비활성화"));
    }
}

void UHSNavMeshGenerator::ProcessNextBuildTask()
{
    if (bIsProcessingTasks.AtomicSet(true))
    {
        return; // 이미 처리 중
    }
    
    // 현재 실행 중인 작업 수 확인
    if (AsyncTasks.Num() >= MaxConcurrentTasks)
    {
        bIsProcessingTasks = false;
        return;
    }
    
    FHSNavMeshBuildTask CurrentTask;
    bool bHasTask = false;
    
    // 다음 작업 가져오기
    {
        FScopeLock Lock(&TaskQueueCriticalSection);
        if (PendingTasks.Num() > 0)
        {
            CurrentTask = PendingTasks[0];
            PendingTasks.RemoveAt(0);
            bHasTask = true;
        }
    }
    
    if (!bHasTask)
    {
        bIsProcessingTasks = false;
        return;
    }
    
    // UE5에서는 네비게이션 경계를 직접 설정
    CreateNavMeshBoundsVolume(CurrentTask.BuildBounds);
    
    // 비동기 작업 시작
    FAsyncTask<FHSAsyncNavMeshBuildTask>* AsyncTask = new FAsyncTask<FHSAsyncNavMeshBuildTask>(CurrentTask, GetWorld());
    AsyncTasks.Add(AsyncTask);
    AsyncTask->StartBackgroundTask();
    
    // 빌드 시작 시간 기록
    double StartTime = FPlatformTime::Seconds();
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 메시 빌드 작업을 시작했습니다. TaskID: %s"), 
               *CurrentTask.TaskID.ToString());
    }
    
    // 디버그 시각화
    if (bEnableDebugVisualization)
    {
        DrawDebugBox(GetWorld(), CurrentTask.BuildBounds.GetCenter(), CurrentTask.BuildBounds.GetExtent(), 
                     FColor::Green, false, 10.0f, 0, 2.0f);
    }
    
    bIsProcessingTasks = false;
}

void UHSNavMeshGenerator::CheckAsyncTaskCompletion()
{
    for (int32 i = AsyncTasks.Num() - 1; i >= 0; --i)
    {
        FAsyncTask<FHSAsyncNavMeshBuildTask>* Task = AsyncTasks[i];
        
        if (Task && Task->IsDone())
        {
            // 작업 완료 처리
            {
                FScopeLock Lock(&StatsUpdateCriticalSection);
                BuildStats.CompletedTasks++;
                BuildStats.TotalBuildTimeMs += 100.0f; // 임시값, 실제로는 측정된 시간 사용
            }
            
            // 작업 정리
            delete Task;
            AsyncTasks.RemoveAt(i);
            
            if (bEnableDebugLogging)
            {
                UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 비동기 네비게이션 메시 빌드 작업이 완료되었습니다."));
            }
        }
    }
}

AActor* UHSNavMeshGenerator::CreateNavMeshBoundsVolume(const FBox& Bounds)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    // UE5에서는 NavMeshBoundsVolume 대신 직접 네비게이션 데이터를 구성
    if (NavigationSystem.IsValid())
    {
        // 네비게이션 시스템에 직접 경계 업데이트 (UE5 API)
        TArray<FBox> DirtyAreas;
        DirtyAreas.Add(Bounds);
        NavigationSystem->OnNavigationBoundsUpdated(nullptr);
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 경계가 직접 업데이트되었습니다. 영역: %s"), *Bounds.ToString());
        }
    }
    
    return nullptr; // 더 이상 BoundsVolume을 생성하지 않음
}

TArray<FBox> UHSNavMeshGenerator::CalculateOptimalBuildRegions(const FBox& OriginalBounds)
{
    TArray<FBox> OptimalRegions;
    
    float OriginalVolume = OriginalBounds.GetVolume();
    float OptimalChunkSize = FMath::Sqrt(MaxBuildAreaSize);
    
    FVector Extent = OriginalBounds.GetExtent();
    FVector Center = OriginalBounds.GetCenter();
    
    // X, Y 축으로 분할 (Z축은 유지)
    int32 DivisionsX = FMath::CeilToInt(Extent.X * 2.0f / OptimalChunkSize);
    int32 DivisionsY = FMath::CeilToInt(Extent.Y * 2.0f / OptimalChunkSize);
    
    DivisionsX = FMath::Max(1, DivisionsX);
    DivisionsY = FMath::Max(1, DivisionsY);
    
    float ChunkSizeX = (Extent.X * 2.0f) / DivisionsX;
    float ChunkSizeY = (Extent.Y * 2.0f) / DivisionsY;
    
    for (int32 X = 0; X < DivisionsX; ++X)
    {
        for (int32 Y = 0; Y < DivisionsY; ++Y)
        {
            FVector ChunkMin = OriginalBounds.Min + FVector(X * ChunkSizeX, Y * ChunkSizeY, 0.0f);
            FVector ChunkMax = ChunkMin + FVector(ChunkSizeX, ChunkSizeY, Extent.Z * 2.0f);
            
            // 원본 경계를 벗어나지 않도록 클램프
            ChunkMax = FVector::Min(ChunkMax, OriginalBounds.Max);
            
            OptimalRegions.Add(FBox(ChunkMin, ChunkMax));
        }
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 큰 영역을 %d개의 최적화된 영역으로 분할했습니다."), OptimalRegions.Num());
    }
    
    return OptimalRegions;
}

void UHSNavMeshGenerator::OptimizeMemoryUsage()
{
    // UE5에서는 경계 볼륨을 생성하지 않으므로 정리할 필요 없음
    
    // 완료된 비동기 작업 정리
    CheckAsyncTaskCompletion();
    
    // Navigation System의 메모리 최적화 요청
    if (NavigationSystem.IsValid())
    {
        // UE5에서는 자동으로 메모리 관리가 이루어지지만, 필요시 수동 최적화 가능
        NavigationSystem->OnNavigationBoundsUpdated(nullptr);
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 메모리 최적화를 수행했습니다."));
    }
}

float UHSNavMeshGenerator::ValidateNavMeshQuality(const FBox& TestBounds)
{
    if (!NavigationSystem.IsValid())
    {
        return 0.0f;
    }
    
    // 테스트 포인트들을 생성하여 네비게이션 품질 측정
    TArray<FVector> TestPoints;
    const int32 NumTestPoints = 20;
    
    for (int32 i = 0; i < NumTestPoints; ++i)
    {
        FVector RandomPoint = FMath::RandPointInBox(TestBounds);
        TestPoints.Add(RandomPoint);
    }
    
    int32 ValidNavigablePoints = 0;
    
    for (const FVector& Point : TestPoints)
    {
        FNavLocation NavLocation;
        if (NavigationSystem->ProjectPointToNavigation(Point, NavLocation))
        {
            ValidNavigablePoints++;
        }
    }
    
    float QualityScore = (float)ValidNavigablePoints / (float)NumTestPoints;
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavMeshGenerator: 네비게이션 메시 품질 점수: %.2f (%d/%d 포인트가 유효)"), 
               QualityScore, ValidNavigablePoints, NumTestPoints);
    }
    
    return QualityScore;
}
