// HSNavigationIntegration.h
// 월드 생성 시스템과 네비게이션 시스템을 통합하여 관리하는 클래스
// 절차적 월드 생성 시 자동으로 네비게이션 메시를 생성하고 AI들과 연동

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "HSNavigationIntegration.generated.h"

class UHSNavMeshGenerator;
class UHSRuntimeNavigation;
class AHSWorldGenerator;
class AAIController;

// 네비게이션 통합 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavigationReadyDelegate, const FBox&, GeneratedArea);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavigationUpdateDelegate, const FBox&, UpdatedArea);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNavigationErrorDelegate, const FString&, ErrorMessage, const FBox&, FailedArea);

// 네비게이션 통합 상태
UENUM(BlueprintType)
enum class EHSNavigationIntegrationState : uint8
{
    Uninitialized    UMETA(DisplayName = "초기화되지 않음"),
    Initializing     UMETA(DisplayName = "초기화 중"),
    Ready           UMETA(DisplayName = "준비 완료"),
    Generating      UMETA(DisplayName = "생성 중"),
    Error           UMETA(DisplayName = "오류")
};

/**
 * HSNavigationIntegration
 * 월드 생성 시스템과 네비게이션 시스템을 통합하여 관리하는 컴포넌트
 * 
 * 주요 기능:
 * - 월드 생성과 네비게이션 메시 생성의 자동 연동
 * - AI 시스템과의 통합 관리
 * - 네비게이션 이벤트 관리 및 알림
 * - 성능 최적화된 통합 워크플로우
 * - 오류 처리 및 복구 시스템
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSNavigationIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UHSNavigationIntegration();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * 네비게이션 통합 시스템을 초기화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void InitializeNavigationIntegration();

    /**
     * 월드 생성 완료 시 호출되는 함수
     * @param GeneratedBounds 생성된 월드 영역
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void OnWorldGenerationComplete(const FBox& GeneratedBounds);

    /**
     * 월드 업데이트 시 호출되는 함수
     * @param UpdatedBounds 업데이트된 월드 영역
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void OnWorldUpdated(const FBox& UpdatedBounds);

    /**
     * AI 컨트롤러를 통합 시스템에 등록합니다
     * @param AIController 등록할 AI 컨트롤러
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void RegisterAIController(AAIController* AIController);

    /**
     * AI 컨트롤러를 통합 시스템에서 해제합니다
     * @param AIController 해제할 AI 컨트롤러
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void UnregisterAIController(AAIController* AIController);

    /**
     * 특정 영역의 네비게이션을 강제로 업데이트합니다
     * @param UpdateArea 업데이트할 영역
     * @param bHighPriority 높은 우선순위 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void ForceNavigationUpdate(const FBox& UpdateArea, bool bHighPriority = true);

    /**
     * 모든 AI의 네비게이션을 재초기화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation Integration")
    void ReinitializeAllAINavigation();

    /**
     * 네비게이션 통합 상태를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Navigation Integration")
    EHSNavigationIntegrationState GetIntegrationState() const { return CurrentState; }

    /**
     * 네비게이션 시스템이 준비되었는지 확인합니다
     */
    UFUNCTION(BlueprintPure, Category = "Navigation Integration")
    bool IsNavigationReady() const { return CurrentState == EHSNavigationIntegrationState::Ready; }

    /**
     * 등록된 AI 수를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "Navigation Integration")
    int32 GetRegisteredAICount() const { return RegisteredAIControllers.Num(); }

protected:
    /**
     * 네비게이션 컴포넌트들을 초기화합니다
     */
    void InitializeNavigationComponents();

    /**
     * 월드 생성기와의 연동을 설정합니다
     */
    void SetupWorldGeneratorIntegration();

    /**
     * 네비게이션 이벤트 핸들러들을 설정합니다
     */
    void SetupNavigationEventHandlers();

    /**
     * 네비게이션 생성 완료 시 호출됩니다
     * @param GeneratedArea 생성된 영역
     */
    UFUNCTION()
    void OnNavigationGenerationComplete(const FBox& GeneratedArea);

    /**
     * 네비게이션 생성 실패 시 호출됩니다
     * @param ErrorMessage 오류 메시지
     * @param FailedArea 실패한 영역
     */
    UFUNCTION()
    void OnNavigationGenerationFailed(const FString& ErrorMessage, const FBox& FailedArea);

    /**
     * AI 등록 상태를 검증합니다
     */
    void ValidateAIRegistrations();

    /**
     * 오류 복구를 시도합니다
     */
    void AttemptErrorRecovery();

private:
    // 네비게이션 메시 생성기 참조
    UPROPERTY()
    TObjectPtr<UHSNavMeshGenerator> NavMeshGenerator;

    // 런타임 네비게이션 관리자 참조
    UPROPERTY()
    TWeakObjectPtr<UHSRuntimeNavigation> RuntimeNavigation;

    // 월드 생성기 참조
    UPROPERTY()
    TWeakObjectPtr<AHSWorldGenerator> WorldGenerator;

    // 등록된 AI 컨트롤러들
    UPROPERTY()
    TArray<TWeakObjectPtr<AAIController>> RegisteredAIControllers;

    // 현재 통합 상태
    EHSNavigationIntegrationState CurrentState;

    // 마지막 오류 메시지
    FString LastErrorMessage;

    // 오류 복구 시도 횟수
    int32 ErrorRecoveryAttempts;

public:
    // === 이벤트 델리게이트들 ===

    // 네비게이션 준비 완료 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Navigation Integration|Events")
    FOnNavigationReadyDelegate OnNavigationReady;

    // 네비게이션 업데이트 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Navigation Integration|Events")
    FOnNavigationUpdateDelegate OnNavigationUpdate;

    // 네비게이션 오류 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Navigation Integration|Events")
    FOnNavigationErrorDelegate OnNavigationError;

    // === 설정 가능한 속성들 ===

    // 자동 초기화 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation Integration")
    bool bAutoInitialize;

    // 월드 생성 시 자동 네비게이션 생성
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation Integration")
    bool bAutoGenerateNavigation;

    // AI 자동 등록 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation Integration")
    bool bAutoRegisterAI;

    // 네비게이션 생성 우선순위
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation Integration", meta = (ClampMin = "1", ClampMax = "100"))
    int32 NavigationGenerationPriority;

    // 오류 자동 복구 시도 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation Integration", meta = (ClampMin = "0", ClampMax = "10"))
    int32 MaxErrorRecoveryAttempts;

    // AI 등록 검증 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation Integration", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float AIValidationInterval;

    // 디버그 로그 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging;

private:
    // 타이머 핸들들
    FTimerHandle AIValidationTimerHandle;
    FTimerHandle ErrorRecoveryTimerHandle;

    // 통합 초기화 완료 여부
    bool bIntegrationInitialized;
};
