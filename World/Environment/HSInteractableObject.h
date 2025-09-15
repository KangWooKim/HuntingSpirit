// HSInteractableObject.h
// 플레이어가 상호작용할 수 있는 오브젝트의 기본 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "HSInteractableObject.generated.h"

// 상호작용 타입 정의
UENUM(BlueprintType)
enum class EInteractionType : uint8
{
    IT_OneTime      UMETA(DisplayName = "One Time"),      // 일회성 상호작용
    IT_Repeatable   UMETA(DisplayName = "Repeatable"),    // 반복 가능
    IT_Conditional  UMETA(DisplayName = "Conditional"),   // 조건부
    IT_Timed        UMETA(DisplayName = "Timed")          // 시간 제한
};

// 상호작용 상태
UENUM(BlueprintType)
enum class EInteractionState : uint8
{
    IS_Ready        UMETA(DisplayName = "Ready"),         // 상호작용 가능
    IS_InProgress   UMETA(DisplayName = "In Progress"),   // 상호작용 진행 중
    IS_Completed    UMETA(DisplayName = "Completed"),     // 상호작용 완료
    IS_Cooldown     UMETA(DisplayName = "Cooldown"),      // 쿨다운 중
    IS_Disabled     UMETA(DisplayName = "Disabled")       // 비활성화
};

// 상호작용 데이터 구조체
USTRUCT(BlueprintType)
struct FInteractionData : public FTableRowBase
{
    GENERATED_BODY()

    // 상호작용 프롬프트 텍스트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    FText InteractionPrompt;

    // 상호작용에 필요한 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
    float InteractionDuration;

    // 상호작용 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
    float InteractionDistance;

    // 쿨다운 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
    float CooldownTime;

    // 상호작용 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    EInteractionType InteractionType;

    // 상호작용 시 재생될 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    class USoundBase* InteractionSound;

    // 상호작용 시 재생될 파티클
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class UParticleSystem* InteractionEffect;

    FInteractionData()
    {
        InteractionPrompt = FText::FromString("상호작용");
        InteractionDuration = 0.0f;
        InteractionDistance = 200.0f;
        CooldownTime = 0.0f;
        InteractionType = EInteractionType::IT_Repeatable;
        InteractionSound = nullptr;
        InteractionEffect = nullptr;
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionStarted, class AHSCharacterBase*, InteractingCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionCompleted, class AHSCharacterBase*, InteractingCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionCancelled, class AHSCharacterBase*, InteractingCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionProgress, class AHSCharacterBase*, InteractingCharacter, float, Progress);

UCLASS()
class HUNTINGSPIRIT_API AHSInteractableObject : public AActor
{
    GENERATED_BODY()

public:
    AHSInteractableObject();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    virtual void Tick(float DeltaTime) override;

    // 상호작용 가능 여부 확인
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual bool CanInteract(class AHSCharacterBase* Character) const;

    // 상호작용 시작
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual void StartInteraction(class AHSCharacterBase* Character);

    // 상호작용 종료
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual void EndInteraction(class AHSCharacterBase* Character, bool bWasCompleted);

    // 상호작용 진행률 가져오기
    UFUNCTION(BlueprintPure, Category = "Interaction")
    float GetInteractionProgress() const;

    // 상호작용 프롬프트 텍스트 가져오기
    UFUNCTION(BlueprintPure, Category = "Interaction")
    FText GetInteractionPrompt() const { return InteractionData.InteractionPrompt; }

    // 상호작용 상태 가져오기
    UFUNCTION(BlueprintPure, Category = "Interaction")
    EInteractionState GetInteractionState() const { return CurrentState; }

    // 상호작용 활성화/비활성화
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual void SetInteractionEnabled(bool bEnabled);

    // 상호작용 리셋
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    virtual void ResetInteraction();

protected:
    // 상호작용 완료 시 호출되는 함수 (서브클래스에서 오버라이드)
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void HandleInteractionCompleted(class AHSCharacterBase* Character);
    virtual void HandleInteractionCompleted_Implementation(class AHSCharacterBase* Character);

    // 상호작용 조건 확인 (서브클래스에서 오버라이드)
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    bool CheckInteractionConditions(class AHSCharacterBase* Character) const;
    virtual bool CheckInteractionConditions_Implementation(class AHSCharacterBase* Character) const;

    // 상호작용 시각 효과 재생
    UFUNCTION(BlueprintCallable, Category = "Effects")
    void PlayInteractionEffects();

    // 쿨다운 처리
    void HandleCooldown();

    // 상태 변경
    void SetInteractionState(EInteractionState NewState);

    // 네트워크 상태 동기화
    UFUNCTION()
    void OnRep_CurrentState();

    UFUNCTION()
    void OnRep_InteractionProgress();

protected:
    // 상호작용 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    FInteractionData InteractionData;

    // 현재 상호작용 상태
    UPROPERTY(ReplicatedUsing = OnRep_CurrentState, BlueprintReadOnly, Category = "Interaction")
    EInteractionState CurrentState;

    // 현재 상호작용 중인 캐릭터
    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    class AHSCharacterBase* CurrentInteractingCharacter;

    // 상호작용 진행률 (0.0 ~ 1.0)
    UPROPERTY(ReplicatedUsing = OnRep_InteractionProgress, BlueprintReadOnly, Category = "Interaction")
    float InteractionProgress;

    // 상호작용 시작 시간
    float InteractionStartTime;

    // 쿨다운 종료 시간
    float CooldownEndTime;

    // 상호작용 완료 횟수
    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    int32 InteractionCount;

    // 상호작용 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    bool bIsInteractionEnabled;

    // 메시 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* MeshComponent;

    // 상호작용 범위를 표시하는 충돌체
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* InteractionSphere;

    // 상호작용 아이콘을 표시하는 위젯 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UWidgetComponent* InteractionWidget;

    // 오디오 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UAudioComponent* AudioComponent;

    // 파티클 시스템 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UParticleSystemComponent* ParticleComponent;

public:
    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionStarted OnInteractionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionCompleted OnInteractionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionCancelled OnInteractionCancelled;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionProgress OnInteractionProgress;

    // 디버그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugInfo;

protected:
    // 디버그 정보 표시
    void DrawDebugInfo();

    // 타이머 핸들
    FTimerHandle CooldownTimerHandle;
    FTimerHandle InteractionTimerHandle;
};
