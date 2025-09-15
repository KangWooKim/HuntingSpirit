// HSResourceNode.cpp
#include "HSResourceNode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"

AHSResourceNode::AHSResourceNode()
{
    PrimaryActorTick.bCanEverTick = false;

    // 루트 컴포넌트로 메시 컴포넌트 생성
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    // 상호작용 범위 컴포넌트
    InteractionRange = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionRange"));
    InteractionRange->SetupAttachment(RootComponent);
    InteractionRange->SetSphereRadius(200.0f);
    InteractionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 자원 정보 위젯 컴포넌트
    ResourceInfoWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResourceInfoWidget"));
    ResourceInfoWidget->SetupAttachment(RootComponent);
    ResourceInfoWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
    ResourceInfoWidget->SetWidgetSpace(EWidgetSpace::Screen);
    ResourceInfoWidget->SetDrawSize(FVector2D(200.0f, 50.0f));
}

void AHSResourceNode::BeginPlay()
{
    Super::BeginPlay();
    
    // 초기 자원 수량 설정
    CurrentResources = MaxResources;
    
    // 초기 시각적 상태 업데이트
    UpdateNodeVisuals();
}

void AHSResourceNode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

bool AHSResourceNode::StartGathering(AHSCharacterBase* Gatherer)
{
    if (!CanBeGathered() || !Gatherer)
    {
        return false;
    }

    // 이미 다른 플레이어가 채집 중인지 확인
    if (CurrentGatherer.IsValid() && CurrentGatherer.Get() != Gatherer)
    {
        return false;
    }

    CurrentGatherer = Gatherer;

    // 채집 시작 효과 재생
    if (GatheringEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            GatheringEffect,
            GetActorLocation(),
            GetActorRotation()
        );
    }

    if (GatheringSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            GatheringSound,
            GetActorLocation()
        );
    }

    return true;
}

FResourceData AHSResourceNode::CompleteGathering()
{
    FResourceData GatheredResource = ResourceData;
    
    // 자원 수량 감소
    CurrentResources--;

    // 채집 완료 후 현재 채집자 초기화
    CurrentGatherer.Reset();

    // 자원이 고갈된 경우
    if (CurrentResources <= 0)
    {
        // 고갈 효과 재생
        if (DepletionEffect)
        {
            UGameplayStatics::SpawnEmitterAtLocation(
                GetWorld(),
                DepletionEffect,
                GetActorLocation(),
                GetActorRotation()
            );
        }

        if (DepletionSound)
        {
            UGameplayStatics::PlaySoundAtLocation(
                this,
                DepletionSound,
                GetActorLocation()
            );
        }

        // 채집 불가능 상태로 전환
        DisableGathering();

        // 자동 재생성이 활성화된 경우
        if (bAutoRespawn)
        {
            GetWorld()->GetTimerManager().SetTimer(
                RespawnTimerHandle,
                this,
                &AHSResourceNode::HandleResourceRespawn,
                RespawnTime,
                false
            );
        }
        // 고갈 시 파괴가 활성화된 경우
        else if (bDestroyOnDepletion)
        {
            Destroy();
        }
    }

    // 시각적 상태 업데이트
    UpdateNodeVisuals();

    return GatheredResource;
}

void AHSResourceNode::HandleResourceRespawn()
{
    // 자원 수량 복구
    CurrentResources = MaxResources;

    // 재생성 효과 재생
    if (RespawnEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            RespawnEffect,
            GetActorLocation(),
            GetActorRotation()
        );
    }

    if (RespawnSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            RespawnSound,
            GetActorLocation()
        );
    }

    // 채집 가능 상태로 전환
    EnableGathering();

    // 시각적 상태 업데이트
    UpdateNodeVisuals();
}

void AHSResourceNode::EnableGathering()
{
    bCanBeGathered = true;
    
    // 콜리전 활성화
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    // 위젯 표시
    if (ResourceInfoWidget)
    {
        ResourceInfoWidget->SetVisibility(true);
    }
}

void AHSResourceNode::DisableGathering()
{
    bCanBeGathered = false;
    
    // 콜리전을 쿼리만 가능하도록 변경 (물리는 비활성화)
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    
    // 위젯 숨기기
    if (ResourceInfoWidget)
    {
        ResourceInfoWidget->SetVisibility(false);
    }
}

void AHSResourceNode::UpdateNodeVisuals()
{
    // 자원 고갈 상태에 따른 메시 변경
    if (CurrentResources <= 0 && DepletedMesh)
    {
        MeshComponent->SetStaticMesh(DepletedMesh);
    }
    else if (NormalMesh)
    {
        MeshComponent->SetStaticMesh(NormalMesh);
    }
    
    // 자원 수량에 따른 스케일 조정 (선택적)
    float ScaleRatio = static_cast<float>(CurrentResources) / static_cast<float>(MaxResources);
    float MinScale = 0.7f;
    float NewScale = MinScale + (1.0f - MinScale) * ScaleRatio;
    
    // 고갈되지 않은 경우에만 스케일 적용
    if (CurrentResources > 0)
    {
        SetActorScale3D(FVector(NewScale));
    }
}

void AHSResourceNode::ForceRespawn()
{
    // 타이머 취소
    if (RespawnTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(RespawnTimerHandle);
    }
    
    // 즉시 재생성
    HandleResourceRespawn();
}
