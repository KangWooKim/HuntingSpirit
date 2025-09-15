// HSResourceNode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSResourceNode.generated.h"

// 자원 타입 Enum
UENUM(BlueprintType)
enum class EResourceType : uint8
{
    None        UMETA(DisplayName = "None"),
    Wood        UMETA(DisplayName = "Wood"),
    Stone       UMETA(DisplayName = "Stone"),
    Iron        UMETA(DisplayName = "Iron"),
    Gold        UMETA(DisplayName = "Gold"),
    Crystal     UMETA(DisplayName = "Crystal"),
    Herb        UMETA(DisplayName = "Herb"),
    Energy      UMETA(DisplayName = "Energy")
};

// 자원 데이터 구조체
USTRUCT(BlueprintType)
struct FResourceData
{
    GENERATED_BODY()

    // 자원 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EResourceType ResourceType = EResourceType::None;

    // 자원 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity = 1;

    // 자원 품질 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Quality = 1.0f;

    FResourceData()
    {
        ResourceType = EResourceType::None;
        Quantity = 1;
        Quality = 1.0f;
    }
};

/**
 * 채집 가능한 자원 노드 클래스
 * 월드에 배치되어 플레이어가 상호작용하여 자원을 획득할 수 있음
 */
UCLASS()
class HUNTINGSPIRIT_API AHSResourceNode : public AActor
{
    GENERATED_BODY()
    
public:    
    AHSResourceNode();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 자원 재생성 타이머 핸들러
    void HandleResourceRespawn();

    // 채집 가능 상태로 전환
    void EnableGathering();

    // 채집 불가능 상태로 전환
    void DisableGathering();

    // 자원 노드 시각적 업데이트
    void UpdateNodeVisuals();

public:
    // 자원 채집 시작
    UFUNCTION(BlueprintCallable, Category = "Resource")
    bool StartGathering(class AHSCharacterBase* Gatherer);

    // 자원 채집 완료
    UFUNCTION(BlueprintCallable, Category = "Resource")
    FResourceData CompleteGathering();

    // 채집 가능 여부 확인
    UFUNCTION(BlueprintCallable, Category = "Resource")
    bool CanBeGathered() const { return bCanBeGathered && CurrentResources > 0; }

    // 현재 자원 수량 반환
    UFUNCTION(BlueprintCallable, Category = "Resource")
    int32 GetCurrentResources() const { return CurrentResources; }

    // 자원 타입 반환
    UFUNCTION(BlueprintCallable, Category = "Resource")
    EResourceType GetResourceType() const { return ResourceData.ResourceType; }

    // 자원 즉시 재생성 (디버그용)
    UFUNCTION(CallInEditor, Category = "Resource|Debug")
    void ForceRespawn();

    // 자원당 채집 시간 반환
    UFUNCTION(BlueprintCallable, Category = "Resource")
    float GetGatheringTimePerResource() const { return GatheringTimePerResource; }

protected:
    // 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* InteractionRange;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UWidgetComponent* ResourceInfoWidget;

    // 자원 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    FResourceData ResourceData;

    // 최대 자원 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "1"))
    int32 MaxResources = 5;

    // 현재 자원 수량
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    int32 CurrentResources;

    // 자원당 채집 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "0.1"))
    float GatheringTimePerResource = 2.0f;

    // 자원 재생성 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "1.0"))
    float RespawnTime = 300.0f;

    // 채집 가능 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    bool bCanBeGathered = true;

    // 자동 재생성 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    bool bAutoRespawn = true;

    // 채집 시 파괴 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    bool bDestroyOnDepletion = false;

    // 현재 채집 중인 캐릭터
    UPROPERTY()
    TWeakObjectPtr<class AHSCharacterBase> CurrentGatherer;

    // 자원 재생성 타이머 핸들
    FTimerHandle RespawnTimerHandle;

    // 시각 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class UParticleSystem* GatheringEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class UParticleSystem* DepletionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class UParticleSystem* RespawnEffect;

    // 사운드 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* GatheringSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* DepletionSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* RespawnSound;

    // 고갈된 상태의 메시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    class UStaticMesh* DepletedMesh;

    // 정상 상태의 메시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    class UStaticMesh* NormalMesh;
};
