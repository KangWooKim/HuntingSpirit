// HSRuntimeNavigation.h
// 런타임에 네비게이션 메시를 관리하고 AI 캐릭터들의 이동을 지원하는 클래스
// AI 캐릭터들의 네비게이션 요청을 효율적으로 처리하고 성능을 최적화

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/World.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "AIController.h" // TWeakObjectPtr을 위해 완전한 타입 정의 필요
#include "HSRuntimeNavigation.generated.h"

class UNavigationSystemV1;
class APawn;

// 네비게이션 요청 정보
USTRUCT(BlueprintType)
struct FHSNavigationRequest
{
    GENERATED_BODY()

    // 요청을 보낸 AI 컨트롤러
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TWeakObjectPtr<AAIController> RequesterController;

    // 시작 위치
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector StartLocation;

    // 목표 위치
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector TargetLocation;

    // 요청 우선순위 (낮을수록 높은 우선순위)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority;

    // 요청 시간
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float RequestTime;

    // 요청 ID
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid RequestID;

    // 최대 검색 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSearchDistance;

    FHSNavigationRequest()
    {
        StartLocation = FVector::ZeroVector;
        TargetLocation = FVector::ZeroVector;
        Priority = 100;
        RequestTime = 0.0f;
        RequestID = FGuid::NewGuid();
        MaxSearchDistance = 5000.0f;
    }

    FHSNavigationRequest(AAIController* InController, const FVector& InStart, const FVector& InTarget, int32 InPriority = 100)
        : RequesterController(InController), StartLocation(InStart), TargetLocation(InTarget), Priority(InPriority)
    {
        RequestTime = FPlatformTime::Seconds();
        RequestID = FGuid::NewGuid();
        MaxSearchDistance = 5000.0f;
    }

    bool operator<(const FHSNavigationRequest& Other) const
    {
        return Priority < Other.Priority;
    }
};

// 네비게이션 성능 통계
USTRUCT(BlueprintType)
struct FHSNavigationPerformanceStats
{
    GENERATED_BODY()

    // 평균 패스파인딩 시간 (밀리초)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float AveragePathfindingTimeMs;

    // 성공한 패스파인딩 요청 수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 SuccessfulRequests;

    // 실패한 패스파인딩 요청 수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FailedRequests;

    // 현재 대기 중인 요청 수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 PendingRequests;

    // 네비게이션 메시 커버리지 (0.0 ~ 1.0)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float NavMeshCoverage;

    FHSNavigationPerformanceStats()
    {
        AveragePathfindingTimeMs = 0.0f;
        SuccessfulRequests = 0;
        FailedRequests = 0;
        PendingRequests = 0;
        NavMeshCoverage = 0.0f;
    }
};

// AI 네비게이션 상태
UENUM(BlueprintType)
enum class EHSAINavigationState : uint8
{
    Idle            UMETA(DisplayName = "대기"),
    Pathfinding     UMETA(DisplayName = "경로 탐색 중"),
    Moving          UMETA(DisplayName = "이동 중"),
    Stuck           UMETA(DisplayName = "막힘"),
    ReachTarget     UMETA(DisplayName = "목표 도달"),
    PathNotFound    UMETA(DisplayName = "경로 없음")
};

// AI 네비게이션 정보
USTRUCT(BlueprintType)
struct FHSAINavigationInfo
{
    GENERATED_BODY()

    // AI 컨트롤러 참조
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TWeakObjectPtr<AAIController> AIController;

    // 현재 네비게이션 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EHSAINavigationState CurrentState;

    // 현재 목표 위치
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector CurrentTarget;

    // 마지막 성공한 패스파인딩 시간
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float LastSuccessfulPathTime;

    // 연속 실패 횟수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 ConsecutiveFailures;

    FHSAINavigationInfo()
    {
        CurrentState = EHSAINavigationState::Idle;
        CurrentTarget = FVector::ZeroVector;
        LastSuccessfulPathTime = 0.0f;
        ConsecutiveFailures = 0;
    }
};

/**
 * HSRuntimeNavigation
 * 런타임에 네비게이션 메시를 관리하고 AI 캐릭터들의 이동을 지원하는 서브시스템
 * 
 * 주요 기능:
 * - AI 캐릭터들의 패스파인딩 요청 큐 관리
 * - 네비게이션 성능 모니터링 및 최적화
 * - 네비게이션 메시 업데이트 시 AI에게 알림
 * - 막힌 AI 캐릭터 자동 복구
 * - 동적 네비게이션 품질 평가
 * - 멀티스레딩을 통한 고성능 패스파인딩
 */
UCLASS()
class HUNTINGSPIRIT_API UHSRuntimeNavigation : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSRuntimeNavigation();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * AI 컨트롤러의 패스파인딩 요청을 큐에 추가합니다
     * @param AIController 요청하는 AI 컨트롤러
     * @param StartLocation 시작 위치
     * @param TargetLocation 목표 위치
     * @param Priority 요청 우선순위
     * @return 요청 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    FGuid RequestPathfinding(AAIController* AIController, const FVector& StartLocation, 
                            const FVector& TargetLocation, int32 Priority = 100);

    /**
     * 특정 패스파인딩 요청을 취소합니다
     * @param RequestID 취소할 요청 ID
     * @return 취소 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    bool CancelPathfindingRequest(const FGuid& RequestID);

    /**
     * 특정 AI 컨트롤러의 모든 요청을 취소합니다
     * @param AIController AI 컨트롤러
     * @return 취소된 요청 수
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    int32 CancelAllRequestsForAI(AAIController* AIController);

    /**
     * AI 컨트롤러를 네비게이션 시스템에 등록합니다
     * @param AIController 등록할 AI 컨트롤러
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    void RegisterAIController(AAIController* AIController);

    /**
     * AI 컨트롤러를 네비게이션 시스템에서 제거합니다
     * @param AIController 제거할 AI 컨트롤러
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    void UnregisterAIController(AAIController* AIController);

    /**
     * 막힌 AI 캐릭터를 자동으로 복구합니다
     * @param AIController 복구할 AI 컨트롤러
     * @return 복구 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    bool RecoverStuckAI(AAIController* AIController);

    /**
     * 네비게이션 메시 업데이트를 모든 AI에게 알립니다
     * @param UpdatedBounds 업데이트된 영역
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    void NotifyNavMeshUpdate(const FBox& UpdatedBounds);

    /**
     * 특정 영역의 네비게이션 품질을 평가합니다
     * @param TestArea 평가할 영역
     * @return 품질 점수 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    float EvaluateNavigationQuality(const FBox& TestArea);

    /**
     * 네비게이션 성능 통계를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Runtime Navigation")
    FHSNavigationPerformanceStats GetPerformanceStats() const { return PerformanceStats; }

    /**
     * 특정 AI의 네비게이션 정보를 반환합니다
     * @param AIController AI 컨트롤러
     * @return 네비게이션 정보
     */
    UFUNCTION(BlueprintPure, Category = "Runtime Navigation")
    FHSAINavigationInfo GetAINavigationInfo(AAIController* AIController);

    /**
     * 등록된 모든 AI의 네비게이션 정보를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Runtime Navigation")
    TArray<FHSAINavigationInfo> GetAllAINavigationInfos();

    /**
     * 네비게이션 시스템을 최적화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Runtime Navigation")
    void OptimizeNavigationSystem();

protected:
    /**
     * 다음 패스파인딩 요청을 처리합니다
     */
    void ProcessNextPathfindingRequest();

    /**
     * AI 컨트롤러들의 상태를 업데이트합니다
     */
    void UpdateAIStates();

    /**
     * 막힌 AI를 감지하고 복구합니다
     */
    void DetectAndRecoverStuckAIs();

    /**
     * 성능 통계를 업데이트합니다
     */
    void UpdatePerformanceStats();

    /**
     * 패스파인딩 요청을 실제로 처리합니다
     * @param Request 처리할 요청
     * @return 성공 여부
     */
    bool ProcessPathfindingRequest(const FHSNavigationRequest& Request);

private:
    // Navigation System 참조
    UPROPERTY()
    TWeakObjectPtr<UNavigationSystemV1> NavigationSystem;

    // 패스파인딩 요청 큐 (우선순위 큐)
    TArray<FHSNavigationRequest> PathfindingQueue;

    // 등록된 AI 컨트롤러들과 그들의 네비게이션 정보
    UPROPERTY()
    TMap<TWeakObjectPtr<AAIController>, FHSAINavigationInfo> RegisteredAIs;

    // 성능 통계
    FHSNavigationPerformanceStats PerformanceStats;

    // 타이머 핸들들
    FTimerHandle PathfindingProcessTimerHandle;
    FTimerHandle AIStateUpdateTimerHandle;
    FTimerHandle StuckDetectionTimerHandle;
    FTimerHandle PerformanceUpdateTimerHandle;

public:
    // === 설정 가능한 속성들 ===

    // 패스파인딩 처리 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float PathfindingProcessInterval;

    // AI 상태 업데이트 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float AIStateUpdateInterval;

    // 막힌 AI 감지 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float StuckDetectionInterval;

    // 성능 통계 업데이트 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "5.0", ClampMax = "60.0"))
    float PerformanceUpdateInterval;

    // 최대 동시 패스파인딩 요청 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxConcurrentPathfindingRequests;

    // AI가 막혔다고 판단하는 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "2.0", ClampMax = "30.0"))
    float StuckTimeThreshold;

    // AI가 같은 위치에 있다고 판단하는 거리 (cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "10.0", ClampMax = "200.0"))
    float StuckDistanceThreshold;

    // 패스파인딩 타임아웃 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Navigation", meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float PathfindingTimeout;

    // 디버그 로그 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging;

    // 디버그 시각화 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugVisualization;

private:
    // 스레드 안전성을 위한 크리티컬 섹션들
    FCriticalSection PathfindingQueueCriticalSection;
    FCriticalSection AIRegistryCriticalSection;
    FCriticalSection PerformanceStatsCriticalSection;

    // 현재 처리 중인 요청 수 추적
    FThreadSafeCounter ActivePathfindingRequests;
};
