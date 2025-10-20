// HSRuntimeNavigation.cpp
// 런타임 네비게이션 관리자 구현

#include "HSRuntimeNavigation.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Algo/Sort.h"
#include "Navigation/PathFollowingComponent.h"

UHSRuntimeNavigation::UHSRuntimeNavigation()
{
    // 기본 설정값 초기화
    PathfindingProcessInterval = 0.05f;  // 20Hz로 처리
    AIStateUpdateInterval = 1.0f;        // 1초마다 상태 업데이트
    StuckDetectionInterval = 2.0f;       // 2초마다 막힌 AI 감지
    PerformanceUpdateInterval = 10.0f;   // 10초마다 성능 통계 업데이트
    MaxConcurrentPathfindingRequests = 5;
    StuckTimeThreshold = 5.0f;
    StuckDistanceThreshold = 50.0f;
    PathfindingTimeout = 3.0f;
    bEnableDebugLogging = true;
    bEnableDebugVisualization = false;
}

void UHSRuntimeNavigation::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Navigation System 초기화
    UWorld* World = GetWorld();
    if (World)
    {
        NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!NavigationSystem.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("HSRuntimeNavigation: Navigation System을 찾을 수 없습니다."));
            return;
        }
    }
    
    // 타이머 설정
    if (World)
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        
        // 패스파인딩 처리 타이머
        TimerManager.SetTimer(PathfindingProcessTimerHandle, 
            FTimerDelegate::CreateUObject(this, &UHSRuntimeNavigation::ProcessNextPathfindingRequest),
            PathfindingProcessInterval, true);
        
        // AI 상태 업데이트 타이머
        TimerManager.SetTimer(AIStateUpdateTimerHandle,
            FTimerDelegate::CreateUObject(this, &UHSRuntimeNavigation::UpdateAIStates),
            AIStateUpdateInterval, true);
        
        // 막힌 AI 감지 타이머
        TimerManager.SetTimer(StuckDetectionTimerHandle,
            FTimerDelegate::CreateUObject(this, &UHSRuntimeNavigation::DetectAndRecoverStuckAIs),
            StuckDetectionInterval, true);
        
        // 성능 통계 업데이트 타이머
        TimerManager.SetTimer(PerformanceUpdateTimerHandle,
            FTimerDelegate::CreateUObject(this, &UHSRuntimeNavigation::UpdatePerformanceStats),
            PerformanceUpdateInterval, true);
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 런타임 네비게이션 시스템이 초기화되었습니다."));
    }
}

void UHSRuntimeNavigation::Deinitialize()
{
    // 모든 타이머 정리
    UWorld* World = GetWorld();
    if (World)
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(PathfindingProcessTimerHandle);
        TimerManager.ClearTimer(AIStateUpdateTimerHandle);
        TimerManager.ClearTimer(StuckDetectionTimerHandle);
        TimerManager.ClearTimer(PerformanceUpdateTimerHandle);
    }
    
    // 모든 등록된 AI 정리
    {
        FScopeLock Lock(&AIRegistryCriticalSection);
        RegisteredAIs.Empty();
    }
    
    // 대기 중인 패스파인딩 요청 정리
    {
        FScopeLock Lock(&PathfindingQueueCriticalSection);
        PathfindingQueue.Empty();
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 런타임 네비게이션 시스템이 종료되었습니다."));
    }
    
    Super::Deinitialize();
}

FGuid UHSRuntimeNavigation::RequestPathfinding(AAIController* AIController, const FVector& StartLocation, 
                                              const FVector& TargetLocation, int32 Priority)
{
    if (!AIController || !NavigationSystem.IsValid())
    {
        return FGuid();
    }
    
    // 새로운 패스파인딩 요청 생성
    FHSNavigationRequest NewRequest(AIController, StartLocation, TargetLocation, Priority);
    
    // 큐에 추가 (스레드 안전)
    {
        FScopeLock Lock(&PathfindingQueueCriticalSection);
        PathfindingQueue.Add(NewRequest);
        
        // 우선순위에 따라 정렬
        Algo::Sort(PathfindingQueue);
        
        // 통계 업데이트
        {
            FScopeLock StatsLock(&PerformanceStatsCriticalSection);
            PerformanceStats.PendingRequests = PathfindingQueue.Num();
        }
    }
    
    // AI 상태 업데이트
    {
        FScopeLock Lock(&AIRegistryCriticalSection);
        if (FHSAINavigationInfo* AIInfo = RegisteredAIs.Find(AIController))
        {
            AIInfo->CurrentState = EHSAINavigationState::Pathfinding;
            AIInfo->CurrentTarget = TargetLocation;
        }
    }
    
    if (bEnableDebugLogging)
    {
    UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 패스파인딩 요청이 추가되었습니다. AI: %s, RequestID: %s"),
    *AIController->GetName(), *NewRequest.RequestID.ToString());
    }
    
    return NewRequest.RequestID;
}

bool UHSRuntimeNavigation::CancelPathfindingRequest(const FGuid& RequestID)
{
    FScopeLock Lock(&PathfindingQueueCriticalSection);
    
    int32 RemovedCount = PathfindingQueue.RemoveAll([RequestID](const FHSNavigationRequest& Request)
    {
        return Request.RequestID == RequestID;
    });
    
    if (RemovedCount > 0)
    {
        // 통계 업데이트
        {
            FScopeLock StatsLock(&PerformanceStatsCriticalSection);
            PerformanceStats.PendingRequests = PathfindingQueue.Num();
        }
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 패스파인딩 요청이 취소되었습니다. RequestID: %s"), *RequestID.ToString());
        }
        
        return true;
    }
    
    return false;
}

int32 UHSRuntimeNavigation::CancelAllRequestsForAI(AAIController* AIController)
{
    if (!AIController)
    {
        return 0;
    }
    
    FScopeLock Lock(&PathfindingQueueCriticalSection);
    
    int32 RemovedCount = PathfindingQueue.RemoveAll([AIController](const FHSNavigationRequest& Request)
    {
        return Request.RequesterController == AIController;
    });
    
    if (RemovedCount > 0)
    {
        // 통계 업데이트
        {
            FScopeLock StatsLock(&PerformanceStatsCriticalSection);
            PerformanceStats.PendingRequests = PathfindingQueue.Num();
        }
        
        // AI 상태를 대기로 변경
        {
            FScopeLock AILock(&AIRegistryCriticalSection);
            if (FHSAINavigationInfo* AIInfo = RegisteredAIs.Find(AIController))
            {
                AIInfo->CurrentState = EHSAINavigationState::Idle;
            }
        }
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: AI의 모든 패스파인딩 요청이 취소되었습니다. AI: %s, 취소된 요청 수: %d"),
                   *AIController->GetName(), RemovedCount);
        }
    }
    
    return RemovedCount;
}

void UHSRuntimeNavigation::RegisterAIController(AAIController* AIController)
{
    if (!AIController)
    {
        return;
    }
    
    FScopeLock Lock(&AIRegistryCriticalSection);
    
    // 이미 등록된 AI인지 확인
    if (RegisteredAIs.Contains(AIController))
    {
        return;
    }
    
    // 새로운 AI 정보 생성
    FHSAINavigationInfo NewAIInfo;
    NewAIInfo.AIController = AIController;
    NewAIInfo.CurrentState = EHSAINavigationState::Idle;
    NewAIInfo.CurrentTarget = FVector::ZeroVector;
    NewAIInfo.LastSuccessfulPathTime = FPlatformTime::Seconds();
    NewAIInfo.ConsecutiveFailures = 0;
    
    RegisteredAIs.Add(AIController, NewAIInfo);
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: AI 컨트롤러가 등록되었습니다. AI: %s"), *AIController->GetName());
    }
}

void UHSRuntimeNavigation::UnregisterAIController(AAIController* AIController)
{
    if (!AIController)
    {
        return;
    }
    
    // 해당 AI의 모든 패스파인딩 요청 취소
    CancelAllRequestsForAI(AIController);
    
    // 등록 해제
    {
        FScopeLock Lock(&AIRegistryCriticalSection);
        RegisteredAIs.Remove(AIController);
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: AI 컨트롤러가 등록 해제되었습니다. AI: %s"), *AIController->GetName());
    }
}

bool UHSRuntimeNavigation::RecoverStuckAI(AAIController* AIController)
{
    if (!AIController || !AIController->GetPawn())
    {
        return false;
    }
    
    APawn* AIPawn = AIController->GetPawn();
    FVector CurrentLocation = AIPawn->GetActorLocation();
    
    // 주변의 네비게이션 가능한 위치 찾기
    FNavLocation NavLocation;
    bool bFoundNavLocation = false;
    
    if (NavigationSystem.IsValid())
    {
        // 현재 위치에서 가장 가까운 네비게이션 위치 찾기
        bFoundNavLocation = NavigationSystem->ProjectPointToNavigation(CurrentLocation, NavLocation, FVector(500.0f, 500.0f, 200.0f));
        
        if (!bFoundNavLocation)
        {
            // 더 넓은 범위에서 랜덤한 네비게이션 위치 찾기
            for (int32 i = 0; i < 10; ++i)
            {
                FVector RandomDirection = FMath::VRand();
                RandomDirection.Z = 0.0f;
                RandomDirection.Normalize();
                
                FVector TestLocation = CurrentLocation + RandomDirection * FMath::RandRange(200.0f, 800.0f);
                
                if (NavigationSystem->ProjectPointToNavigation(TestLocation, NavLocation, FVector(100.0f, 100.0f, 200.0f)))
                {
                    bFoundNavLocation = true;
                    break;
                }
            }
        }
    }
    
    if (bFoundNavLocation)
    {
        // AI를 안전한 위치로 이동
        AIPawn->SetActorLocation(NavLocation.Location);
        
        // AI 상태 업데이트
        {
            FScopeLock Lock(&AIRegistryCriticalSection);
            if (FHSAINavigationInfo* AIInfo = RegisteredAIs.Find(AIController))
            {
                AIInfo->CurrentState = EHSAINavigationState::Idle;
                AIInfo->ConsecutiveFailures = 0;
                AIInfo->LastSuccessfulPathTime = FPlatformTime::Seconds();
            }
        }
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 막힌 AI를 복구했습니다. AI: %s, 새 위치: %s"),
                   *AIController->GetName(), *NavLocation.Location.ToString());
        }
        
        return true;
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRuntimeNavigation: 막힌 AI 복구에 실패했습니다. AI: %s"), *AIController->GetName());
    }
    
    return false;
}

void UHSRuntimeNavigation::NotifyNavMeshUpdate(const FBox& UpdatedBounds)
{
    FScopeLock Lock(&AIRegistryCriticalSection);
    
    int32 AffectedAICount = 0;
    
    for (auto& AIEntry : RegisteredAIs)
    {
        AAIController* AIController = AIEntry.Key.Get();
        FHSAINavigationInfo& AIInfo = AIEntry.Value;
        
        if (!AIController || !AIController->GetPawn())
        {
            continue;
        }
        
        FVector AILocation = AIController->GetPawn()->GetActorLocation();
        
        // AI가 업데이트된 영역 내에 있는지 확인
        if (UpdatedBounds.IsInside(AILocation) || UpdatedBounds.IsInside(AIInfo.CurrentTarget))
        {
            // 현재 패스파인딩 중인 경우 재계산 요청
            if (AIInfo.CurrentState == EHSAINavigationState::Pathfinding || 
                AIInfo.CurrentState == EHSAINavigationState::Moving)
            {
                // 기존 요청 취소하고 새로운 요청 생성
                CancelAllRequestsForAI(AIController);
                
                if (AIInfo.CurrentTarget != FVector::ZeroVector)
                {
                    RequestPathfinding(AIController, AILocation, AIInfo.CurrentTarget, 75); // 중간 우선순위
                }
            }
            
            AffectedAICount++;
        }
    }
    
    if (bEnableDebugLogging && AffectedAICount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 네비게이션 메시 업데이트로 %d개의 AI가 영향을 받았습니다."), AffectedAICount);
    }
}

float UHSRuntimeNavigation::EvaluateNavigationQuality(const FBox& TestArea)
{
    if (!NavigationSystem.IsValid())
    {
        return 0.0f;
    }
    
    const int32 NumTestPoints = 25;
    TArray<FVector> TestPoints;
    
    // 테스트 포인트 생성
    for (int32 i = 0; i < NumTestPoints; ++i)
    {
        FVector RandomPoint = FMath::RandPointInBox(TestArea);
        TestPoints.Add(RandomPoint);
    }
    
    int32 ValidNavigablePoints = 0;
    int32 ConnectedPoints = 0;
    
    // 각 테스트 포인트의 네비게이션 가능성 검사
    for (int32 i = 0; i < TestPoints.Num(); ++i)
    {
        FNavLocation NavLocation;
        if (NavigationSystem->ProjectPointToNavigation(TestPoints[i], NavLocation))
        {
            ValidNavigablePoints++;
            
            // 다른 포인트들과의 연결성 검사
            for (int32 j = i + 1; j < TestPoints.Num(); ++j)
            {
                FNavLocation OtherNavLocation;
                if (NavigationSystem->ProjectPointToNavigation(TestPoints[j], OtherNavLocation))
                {
                    // 두 포인트 간의 경로 존재 여부 확인
                    UNavigationPath* TestPath = NavigationSystem->FindPathToLocationSynchronously(
                        GetWorld(), NavLocation.Location, OtherNavLocation.Location);
                    
                    if (TestPath && TestPath->IsValid())
                    {
                        ConnectedPoints++;
                    }
                }
            }
        }
    }
    
    // 품질 점수 계산 (네비게이션 가능성 50% + 연결성 50%)
    float NavigabilityScore = (float)ValidNavigablePoints / (float)NumTestPoints;
    float ConnectivityScore = 0.0f;
    
    int32 MaxPossibleConnections = (NumTestPoints * (NumTestPoints - 1)) / 2;
    if (MaxPossibleConnections > 0)
    {
        ConnectivityScore = (float)ConnectedPoints / (float)MaxPossibleConnections;
    }
    
    float QualityScore = (NavigabilityScore * 0.5f) + (ConnectivityScore * 0.5f);
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 네비게이션 품질 평가 완료. 점수: %.2f (네비게이션 가능: %.2f, 연결성: %.2f)"),
               QualityScore, NavigabilityScore, ConnectivityScore);
    }
    
    return QualityScore;
}

FHSAINavigationInfo UHSRuntimeNavigation::GetAINavigationInfo(AAIController* AIController)
{
    FScopeLock Lock(&AIRegistryCriticalSection);
    
    if (FHSAINavigationInfo* AIInfo = RegisteredAIs.Find(AIController))
    {
        return *AIInfo;
    }
    
    return FHSAINavigationInfo();
}

TArray<FHSAINavigationInfo> UHSRuntimeNavigation::GetAllAINavigationInfos()
{
    FScopeLock Lock(&AIRegistryCriticalSection);
    
    TArray<FHSAINavigationInfo> AllInfos;
    
    for (const auto& AIEntry : RegisteredAIs)
    {
        AllInfos.Add(AIEntry.Value);
    }
    
    return AllInfos;
}

void UHSRuntimeNavigation::OptimizeNavigationSystem()
{
    // 만료된 AI 컨트롤러 정리
    {
        FScopeLock Lock(&AIRegistryCriticalSection);
        
        TArray<TWeakObjectPtr<AAIController>> ToRemove;
        
        for (const auto& AIEntry : RegisteredAIs)
        {
            if (!AIEntry.Key.IsValid())
            {
                ToRemove.Add(AIEntry.Key);
            }
        }
        
        for (const auto& InvalidAI : ToRemove)
        {
            RegisteredAIs.Remove(InvalidAI);
        }
        
        if (bEnableDebugLogging && ToRemove.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: %d개의 무효한 AI 참조를 정리했습니다."), ToRemove.Num());
        }
    }
    
    // 만료된 패스파인딩 요청 정리
    {
        FScopeLock Lock(&PathfindingQueueCriticalSection);
        
        float CurrentTime = FPlatformTime::Seconds();
        int32 RemovedCount = PathfindingQueue.RemoveAll([CurrentTime, this](const FHSNavigationRequest& Request)
        {
            return !Request.RequesterController.IsValid() || 
                   (CurrentTime - Request.RequestTime) > PathfindingTimeout;
        });
        
        if (bEnableDebugLogging && RemovedCount > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: %d개의 만료된 패스파인딩 요청을 정리했습니다."), RemovedCount);
        }
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 네비게이션 시스템 최적화를 완료했습니다."));
    }
}

void UHSRuntimeNavigation::ProcessNextPathfindingRequest()
{
    // 동시 요청 수 제한 확인
    if (ActivePathfindingRequests.GetValue() >= MaxConcurrentPathfindingRequests)
    {
        return;
    }
    
    FHSNavigationRequest CurrentRequest;
    bool bHasRequest = false;
    
    // 다음 요청 가져오기
    {
        FScopeLock Lock(&PathfindingQueueCriticalSection);
        if (PathfindingQueue.Num() > 0)
        {
            CurrentRequest = PathfindingQueue[0];
            PathfindingQueue.RemoveAt(0);
            bHasRequest = true;
        }
    }
    
    if (!bHasRequest)
    {
        return;
    }
    
    // 요청 처리
    ActivePathfindingRequests.Increment();
    
    bool bSuccess = ProcessPathfindingRequest(CurrentRequest);
    
    // 통계 업데이트
    {
        FScopeLock Lock(&PerformanceStatsCriticalSection);
        if (bSuccess)
        {
            PerformanceStats.SuccessfulRequests++;
        }
        else
        {
            PerformanceStats.FailedRequests++;
        }
        PerformanceStats.PendingRequests = PathfindingQueue.Num();
    }
    
    ActivePathfindingRequests.Decrement();
}

void UHSRuntimeNavigation::UpdateAIStates()
{
    FScopeLock Lock(&AIRegistryCriticalSection);
    
    float CurrentTime = FPlatformTime::Seconds();
    
    for (auto& AIEntry : RegisteredAIs)
    {
        AAIController* AIController = AIEntry.Key.Get();
        FHSAINavigationInfo& AIInfo = AIEntry.Value;
        
        if (!AIController || !AIController->GetPawn())
        {
            continue;
        }
        
        // 패스 팔로잉 컴포넌트 상태 확인
        UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent();
        if (PathFollowingComp)
        {
            switch (PathFollowingComp->GetStatus())
            {
                case EPathFollowingStatus::Idle:
                    AIInfo.CurrentState = EHSAINavigationState::Idle;
                    break;
                    
                case EPathFollowingStatus::Moving:
                    AIInfo.CurrentState = EHSAINavigationState::Moving;
                    break;
                    
                case EPathFollowingStatus::Paused:
                    AIInfo.CurrentState = EHSAINavigationState::Stuck;
                    break;
                    
                default:
                    break;
            }
        }
        
        // 목표 도달 확인
        if (AIInfo.CurrentTarget != FVector::ZeroVector)
        {
            FVector CurrentLocation = AIController->GetPawn()->GetActorLocation();
            float DistanceToTarget = FVector::Dist(CurrentLocation, AIInfo.CurrentTarget);
            
            if (DistanceToTarget < 100.0f) // 1미터 이내면 도달한 것으로 간주
            {
                AIInfo.CurrentState = EHSAINavigationState::ReachTarget;
                AIInfo.CurrentTarget = FVector::ZeroVector;
                AIInfo.LastSuccessfulPathTime = CurrentTime;
                AIInfo.ConsecutiveFailures = 0;
            }
        }
    }
}

void UHSRuntimeNavigation::DetectAndRecoverStuckAIs()
{
    FScopeLock Lock(&AIRegistryCriticalSection);
    
    float CurrentTime = FPlatformTime::Seconds();
    int32 RecoveredAICount = 0;
    
    for (auto& AIEntry : RegisteredAIs)
    {
        AAIController* AIController = AIEntry.Key.Get();
        FHSAINavigationInfo& AIInfo = AIEntry.Value;
        
        if (!AIController || !AIController->GetPawn())
        {
            continue;
        }
        
        // 막힌 AI 감지
        bool bIsStuck = false;
        
        if (AIInfo.CurrentState == EHSAINavigationState::Moving)
        {
            // 마지막 성공적인 패스파인딩으로부터 너무 많은 시간이 지났는지 확인
            if ((CurrentTime - AIInfo.LastSuccessfulPathTime) > StuckTimeThreshold)
            {
                bIsStuck = true;
            }
            
            // 연속 실패 횟수가 너무 많은지 확인
            if (AIInfo.ConsecutiveFailures > 3)
            {
                bIsStuck = true;
            }
        }
        
        if (bIsStuck)
        {
            AIInfo.CurrentState = EHSAINavigationState::Stuck;
            
            // 자동 복구 시도
            if (RecoverStuckAI(AIController))
            {
                RecoveredAICount++;
            }
        }
    }
    
    if (bEnableDebugLogging && RecoveredAICount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: %d개의 막힌 AI를 복구했습니다."), RecoveredAICount);
    }
}

void UHSRuntimeNavigation::UpdatePerformanceStats()
{
    FScopeLock Lock(&PerformanceStatsCriticalSection);

    // 네비게이션 메시 커버리지 계산
    if (NavigationSystem.IsValid())
    {
        UWorld* World = GetWorld();
        if (World)
        {
            FVector WorldOrigin = FVector::ZeroVector;
            FVector WorldExtent = FVector(10000.0f, 10000.0f, 1000.0f);
            FBox WorldBounds = FBox(WorldOrigin - WorldExtent, WorldOrigin + WorldExtent);
            float Coverage = EvaluateNavigationQuality(WorldBounds);
            PerformanceStats.NavMeshCoverage = Coverage;
        }
    }

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 성능 통계 업데이트 완료. 성공: %d, 실패: %d, 대기: %d, 평균 시간: %.1fms, 커버리지: %.2f"),
               PerformanceStats.SuccessfulRequests, PerformanceStats.FailedRequests,
               PerformanceStats.PendingRequests, PerformanceStats.AveragePathfindingTimeMs,
               PerformanceStats.NavMeshCoverage);
    }
}

bool UHSRuntimeNavigation::ProcessPathfindingRequest(const FHSNavigationRequest& Request)
{
    if (!Request.RequesterController.IsValid() || !NavigationSystem.IsValid())
    {
        return false;
    }

    AAIController* AIController = Request.RequesterController.Get();

    const double StartTime = FPlatformTime::Seconds();
    UNavigationPath* Path = NavigationSystem->FindPathToLocationSynchronously(
        GetWorld(), Request.StartLocation, Request.TargetLocation);
    const double ElapsedTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    bool bSuccess = (Path && Path->IsValid());

    {
        FScopeLock Lock(&PerformanceStatsCriticalSection);
        if (bSuccess)
        {
            const float NewAverage = (PerformanceStats.AveragePathfindingTimeMs * PerformanceStats.SuccessfulRequests + ElapsedTimeMs) / (PerformanceStats.SuccessfulRequests + 1);
            PerformanceStats.AveragePathfindingTimeMs = NewAverage;
        }
    }

    {
        FScopeLock Lock(&AIRegistryCriticalSection);
        if (FHSAINavigationInfo* AIInfo = RegisteredAIs.Find(AIController))
        {
            if (bSuccess)
            {
                AIInfo->CurrentState = EHSAINavigationState::Moving;
                AIInfo->LastSuccessfulPathTime = FPlatformTime::Seconds();
                AIInfo->ConsecutiveFailures = 0;

                AIController->MoveToLocation(Request.TargetLocation);
            }
            else
            {
                AIInfo->CurrentState = EHSAINavigationState::PathNotFound;
                AIInfo->ConsecutiveFailures++;
            }
        }
    }

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSRuntimeNavigation: 패스파인딩 요청 처리 완료. AI: %s, 성공: %s, 소요 시간: %.2fms"),
               *AIController->GetName(), bSuccess ? TEXT("예") : TEXT("아니오"), ElapsedTimeMs);
    }

    if (bEnableDebugVisualization && bSuccess)
    {
        DrawDebugLine(GetWorld(), Request.StartLocation, Request.TargetLocation,
                     FColor::Green, false, 3.0f, 0, 2.0f);
    }

    return bSuccess;
}
