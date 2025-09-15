// HSGatheringComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HuntingSpirit/World/Resources/HSResourceNode.h"
#include "HSGatheringComponent.generated.h"

// 채집 상태 Enum
UENUM(BlueprintType)
enum class EGatheringState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Searching       UMETA(DisplayName = "Searching"),
    Approaching     UMETA(DisplayName = "Approaching"),
    Gathering       UMETA(DisplayName = "Gathering"),
    Completed       UMETA(DisplayName = "Completed")
};

// 채집 진행 상태를 알리는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGatheringProgress, float, Progress, float, TotalTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGatheringCompleted, const FResourceData&, GatheredResource);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGatheringCanceled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResourceNodeDetected, AHSResourceNode*, ResourceNode);

/**
 * 캐릭터의 자원 채집 기능을 담당하는 컴포넌트
 * 자원 노드 감지, 채집 진행, 자원 획득 처리
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSGatheringComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    UHSGatheringComponent();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 주변 자원 노드 스캔
    void ScanForResourceNodes();

    // 채집 진행 업데이트
    void UpdateGatheringProgress(float DeltaTime);

    // 채집 완료 처리
    void CompleteGathering();

    // 거리 체크
    bool IsInGatheringRange(AHSResourceNode* ResourceNode) const;

public:
    // 자원 채집 시작
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    bool StartGathering(AHSResourceNode* TargetNode);

    // 자원 채집 취소
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    void CancelGathering();

    // 현재 채집 상태 반환
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    EGatheringState GetGatheringState() const { return CurrentState; }

    // 채집 중인지 확인
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    bool IsGathering() const { return CurrentState == EGatheringState::Gathering; }

    // 현재 타겟 자원 노드 반환
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    AHSResourceNode* GetTargetResourceNode() const { return TargetResourceNode.Get(); }

    // 가장 가까운 자원 노드 찾기
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    AHSResourceNode* FindNearestResourceNode(EResourceType ResourceType = EResourceType::None) const;

    // 감지된 모든 자원 노드 반환
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    TArray<AHSResourceNode*> GetDetectedResourceNodes() const;

    // 특정 자원 타입의 노드만 반환
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    TArray<AHSResourceNode*> GetResourceNodesByType(EResourceType ResourceType) const;

    // 채집 진행도 반환 (0.0 ~ 1.0)
    UFUNCTION(BlueprintCallable, Category = "Gathering")
    float GetGatheringProgress() const;

public:
    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Gathering|Events")
    FOnGatheringProgress OnGatheringProgress;

    UPROPERTY(BlueprintAssignable, Category = "Gathering|Events")
    FOnGatheringCompleted OnGatheringCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Gathering|Events")
    FOnGatheringCanceled OnGatheringCanceled;

    UPROPERTY(BlueprintAssignable, Category = "Gathering|Events")
    FOnResourceNodeDetected OnResourceNodeDetected;

protected:
    // 현재 채집 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gathering")
    EGatheringState CurrentState = EGatheringState::Idle;

    // 타겟 자원 노드
    UPROPERTY()
    TWeakObjectPtr<AHSResourceNode> TargetResourceNode;

    // 감지된 자원 노드 목록
    UPROPERTY()
    TArray<TWeakObjectPtr<AHSResourceNode>> DetectedResourceNodes;

    // 채집 진행 시간
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gathering")
    float GatheringElapsedTime = 0.0f;

    // 현재 채집에 필요한 총 시간
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gathering")
    float CurrentGatheringTime = 0.0f;

    // 자원 노드 감지 범위
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering", meta = (ClampMin = "100.0"))
    float DetectionRange = 1000.0f;

    // 채집 가능 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering", meta = (ClampMin = "50.0"))
    float GatheringRange = 150.0f;

    // 자원 노드 스캔 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering", meta = (ClampMin = "0.1"))
    float ScanInterval = 0.5f;

    // 채집 속도 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering", meta = (ClampMin = "0.1"))
    float GatheringSpeedMultiplier = 1.0f;

    // 이동 중 채집 취소 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering")
    bool bCancelOnMovement = true;

    // 피격 시 채집 취소 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering")
    bool bCancelOnDamage = true;

    // 마지막 스캔 시간
    float LastScanTime = 0.0f;

    // 소유자 캐릭터 캐싱
    UPROPERTY()
    class AHSCharacterBase* OwnerCharacter;

    // 채집 애니메이션 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    class UAnimMontage* GatheringMontage;

    // 채집 시작 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* GatheringStartSound;

    // 채집 루프 사운드 컴포넌트
    UPROPERTY()
    class UAudioComponent* GatheringLoopAudioComponent;

    // 채집 루프 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* GatheringLoopSound;

    // 채집 완료 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* GatheringCompleteSound;

    // 채집 취소 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* GatheringCancelSound;

    // 채집 진행 파티클
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class UParticleSystem* GatheringProgressEffect;

    // 현재 재생 중인 파티클 컴포넌트
    UPROPERTY()
    class UParticleSystemComponent* ActiveGatheringEffect;
};
