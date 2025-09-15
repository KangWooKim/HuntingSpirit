/**
 * @file HSAIControllerBase.h
 * @brief 모든 AI 컨트롤러의 기본 클래스 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "HSAIControllerBase.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class AHSEnemyBase;
class UHSRuntimeNavigation;
class UHSNavigationIntegration;

// 네비게이션 요청 결과
UENUM(BlueprintType)
enum class EHSNavigationRequestResult : uint8
{
    Success         UMETA(DisplayName = "성공"),
    Failed          UMETA(DisplayName = "실패"),
    Pending         UMETA(DisplayName = "대기 중"),
    Cancelled       UMETA(DisplayName = "취소됨")
};

/**
 * @brief 모든 AI 컨트롤러의 기본 클래스
 * @details AI 감지 시스템, 행동 트리, 블랙보드 등 AI의 핵심 기능을 제공합니다.
 *          네비게이션 시스템과 통합되어 고급 이동 기능도 지원합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API AHSAIControllerBase : public AAIController
{
    GENERATED_BODY()

public:
    /**
     * @brief 기본 생성자
     * @details AI 컴포넌트들과 감지 시스템을 초기화합니다
     */
    AHSAIControllerBase();

    /**
     * @brief 게임 시작 시 AI 시스템을 초기화
     * @details 네비게이션 시스템 등록과 감지 설정을 완료합니다
     */
    virtual void BeginPlay() override;

    /**
     * @brief AI를 시작합니다
     * @details 행동 트리를 실행하고 AI 로직을 활성화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "AI")
    virtual void StartAI();

    /**
     * @brief AI를 중지합니다
     * @details 행동 트리 실행을 중단하고 AI 로직을 비활성화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "AI")
    virtual void StopAI();

    // 폰 빙의 완료 시 호출
    UFUNCTION(BlueprintImplementableEvent, Category = "AI")
    void OnPawnPossessed(APawn* PossessedPawn);

    /**
     * @brief 시야 범위를 설정
     * @param Range 새로운 시야 범위 (단위: cm)
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Perception")
    void SetSightRange(float Range);

    /**
     * @brief 시야 각도를 설정
     * @param Angle 새로운 시야 각도 (단위: 도)
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Perception")
    void SetSightAngle(float Angle);

    /**
     * @brief 청각 범위를 설정
     * @param Range 새로운 청각 범위 (단위: cm)
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Perception")
    void SetHearingRange(float Range);

    /**
     * @brief 현재 타겟을 반환
     * @return 현재 추적 중인 타겟 액터 (없으면 nullptr)
     */
    UFUNCTION(BlueprintPure, Category = "AI|Target")
    AActor* GetCurrentTarget() const;

    /**
     * @brief 새로운 타겟을 설정
     * @param NewTarget 새로운 타겟 액터
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void SetCurrentTarget(AActor* NewTarget);

    /**
     * @brief 현재 타겟을 제거
     * @details 블랙보드에서 타겟 정보를 제거하고 추적을 중단합니다
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void ClearCurrentTarget();

    /**
     * @brief 블랙보드에 벡터 값을 설정
     * @param KeyName 블랙보드 키 이름
     * @param Value 설정할 벡터 값
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Blackboard")
    void SetBlackboardValueAsVector(const FName& KeyName, const FVector& Value);

    /**
     * @brief 블랙보드에 오브젝트 값을 설정
     * @param KeyName 블랙보드 키 이름
     * @param Value 설정할 오브젝트 값
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Blackboard")
    void SetBlackboardValueAsObject(const FName& KeyName, UObject* Value);

    /**
     * @brief 블랙보드에 불린 값을 설정
     * @param KeyName 블랙보드 키 이름
     * @param Value 설정할 불린 값
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Blackboard")
    void SetBlackboardValueAsBool(const FName& KeyName, bool Value);

    UFUNCTION(BlueprintCallable, Category = "AI|Blackboard")
    void SetBlackboardValueAsFloat(const FName& KeyName, float Value);

    UFUNCTION(BlueprintCallable, Category = "AI|Blackboard")
    void SetBlackboardValueAsInt(const FName& KeyName, int32 Value);

    // 블랙보드 값 가져오기 함수들
    UFUNCTION(BlueprintPure, Category = "AI|Blackboard")
    FVector GetBlackboardValueAsVector(const FName& KeyName) const;

    UFUNCTION(BlueprintPure, Category = "AI|Blackboard")
    UObject* GetBlackboardValueAsObject(const FName& KeyName) const;

    UFUNCTION(BlueprintPure, Category = "AI|Blackboard")
    bool GetBlackboardValueAsBool(const FName& KeyName) const;

    UFUNCTION(BlueprintPure, Category = "AI|Blackboard")
    float GetBlackboardValueAsFloat(const FName& KeyName) const;

    UFUNCTION(BlueprintPure, Category = "AI|Blackboard")
    int32 GetBlackboardValueAsInt(const FName& KeyName) const;

    // === 네비게이션 시스템 통합 함수들 ===
    
    /**
     * 지정된 위치로 이동합니다 (통합 네비게이션 시스템 사용)
     * @param TargetLocation 목표 위치
     * @param AcceptanceRadius 허용 오차
     * @param bStopOnOverlap 겹침 시 정지 여부
     * @param bUsePathfinding 패스파인딩 사용 여부
     * @param bCanStrafe 측면 이동 허용 여부
     * @return 네비게이션 요청 결과
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
    EHSNavigationRequestResult MoveToLocationAdvanced(const FVector& TargetLocation, 
                                                     float AcceptanceRadius = 50.0f,
                                                     bool bStopOnOverlap = false,
                                                     bool bUsePathfinding = true,
                                                     bool bCanStrafe = false);

    /**
     * 지정된 액터로 이동합니다 (통합 네비게이션 시스템 사용)
     * @param TargetActor 목표 액터
     * @param AcceptanceRadius 허용 오차
     * @param bStopOnOverlap 겹침 시 정지 여부
     * @param bUsePathfinding 패스파인딩 사용 여부
     * @param bCanStrafe 측면 이동 허용 여부
     * @return 네비게이션 요청 결과
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
    EHSNavigationRequestResult MoveToActorAdvanced(AActor* TargetActor,
                                                   float AcceptanceRadius = 50.0f,
                                                   bool bStopOnOverlap = false,
                                                   bool bUsePathfinding = true,
                                                   bool bCanStrafe = false);

    /**
     * 현재 이동을 중지합니다
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
    void StopMovementAdvanced();

    /**
     * AI가 막혀있는지 확인합니다
     * @return 막힘 상태 여부
     */
    UFUNCTION(BlueprintPure, Category = "AI|Navigation")
    bool IsStuck() const;

    /**
     * 막힌 AI를 복구합니다
     * @return 복구 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
    bool RecoverFromStuck();

    /**
     * 현재 네비게이션 상태를 반환합니다
     */
    UFUNCTION(BlueprintPure, Category = "AI|Navigation")
    FString GetNavigationStatusString() const;

    // 컴포넌트 접근 함수들
    UFUNCTION(BlueprintPure, Category = "AI|Components")
    UBehaviorTreeComponent* GetBehaviorTreeComponent() const { return BehaviorTreeComponent; }

    UFUNCTION(BlueprintPure, Category = "AI|Components")
    UBlackboardComponent* GetBlackboardComponent() const { return BlackboardComponent; }

    // GetAIPerceptionComponent는 부모 클래스 AAIController에서 이미 제공됨

    // 디버그 함수들
    UFUNCTION(BlueprintCallable, Category = "AI|Debug")
    void EnableAIDebug(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "AI|Debug")
    void DrawDebugInfo(float Duration = 1.0f);

protected:
    // AI 컴포넌트들
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBehaviorTreeComponent* BehaviorTreeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBlackboardComponent* BlackboardComponent;

    // AI 감지 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAIPerceptionComponent* AIPerceptionComponent;

    // 시각 감지 설정
    UPROPERTY()
    UAISenseConfig_Sight* SightConfig;

    // 청각 감지 설정
    UPROPERTY()
    UAISenseConfig_Hearing* HearingConfig;

    // 데미지 감지 설정
    UPROPERTY()
    UAISenseConfig_Damage* DamageConfig;

    // 감지 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
    float SightRadius = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
    float SightAngleDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
    float HearingRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
    float MaxAge = 5.0f; // 감지 정보 유지 시간

    // 오너 적 캐릭터 캐시
    UPROPERTY()
    AHSEnemyBase* OwnerEnemy;

    // === 네비게이션 시스템 통합 ===
    
    // 런타임 네비게이션 서브시스템 참조
    UPROPERTY()
    TWeakObjectPtr<UHSRuntimeNavigation> RuntimeNavigation;
    
    // 네비게이션 통합 컴포넌트 참조
    UPROPERTY()
    TWeakObjectPtr<UHSNavigationIntegration> NavigationIntegration;
    
    // 현재 네비게이션 요청 ID
    FGuid CurrentNavigationRequestID;
    
    // 마지막 성공한 이동 시간
    float LastSuccessfulMoveTime;
    
    // 마지막 위치 (막힘 감지용)
    FVector LastKnownPosition;
    
    // 위치 체크 시간
    float LastPositionCheckTime;
    
    // 네비게이션 시스템 자동 등록 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Navigation")
    bool bAutoRegisterWithNavigationSystem = true;
    
    // 막힘 감지 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Navigation")
    bool bEnableStuckDetection = true;
    
    // 막힘 감지 거리 임계값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Navigation", meta = (ClampMin = "10.0", ClampMax = "200.0"))
    float StuckDistanceThreshold = 50.0f;
    
    // 막힘 감지 시간 임계값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Navigation", meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float StuckTimeThreshold = 3.0f;

    // 디버그 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Debug")
    bool bShowDebugInfo = false;
    
    // 네비게이션 디버그 표시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Debug")
    bool bShowNavigationDebug = false;

private:
    // 내부 함수들
    void SetupAIPerception();
    void SetupSightSense();
    void SetupHearingSense();
    void SetupDamageSense();
    
    // === 네비게이션 시스템 통합 내부 함수들 ===
    
    /**
     * 네비게이션 시스템을 초기화합니다
     */
    void InitializeNavigationSystem();
    
    /**
     * 네비게이션 시스템에 자동 등록합니다
     */
    void RegisterWithNavigationSystem();
    
    /**
     * 네비게이션 시스템에서 등록 해제합니다
     */
    void UnregisterFromNavigationSystem();
    
    /**
     * 막힘 상태를 업데이트합니다
     */
    void UpdateStuckDetection();
    
    /**
     * 네비게이션 디버그 정보를 그립니다
     */
    void DrawNavigationDebug();

    // 감지 이벤트 함수들
    UFUNCTION()
    void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    UFUNCTION()
    void OnTargetPerceptionForgotten(AActor* Actor);

    // 헬퍼 함수들
    bool IsValidTarget(AActor* Actor) const;
    void HandleSightStimulus(AActor* Actor, const FAIStimulus& Stimulus);
    void HandleHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus);
    void HandleDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus);
    
    // 네비게이션 헬퍼 함수들
    bool IsNavigationSystemReady() const;
    FVector GetSafeLocationNearby(const FVector& OriginalLocation, float SearchRadius = 500.0f) const;
};
