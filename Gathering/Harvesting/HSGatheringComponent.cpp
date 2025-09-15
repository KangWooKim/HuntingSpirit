// HSGatheringComponent.cpp
#include "HSGatheringComponent.h"
#include "HuntingSpirit/World/Resources/HSResourceNode.h"
#include "GameFramework/Character.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "Animation/AnimInstance.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"

UHSGatheringComponent::UHSGatheringComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UHSGatheringComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 소유자 캐릭터 캐싱
    OwnerCharacter = Cast<AHSCharacterBase>(GetOwner());
    
    // 채집 루프 오디오 컴포넌트 생성
    if (GatheringLoopSound)
    {
        GatheringLoopAudioComponent = UGameplayStatics::SpawnSoundAttached(
            GatheringLoopSound,
            GetOwner()->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            true,
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            nullptr,
            false
        );
        
        if (GatheringLoopAudioComponent)
        {
            GatheringLoopAudioComponent->Stop();
        }
    }
}

void UHSGatheringComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 주기적으로 자원 노드 스캔
    if (GetWorld()->GetTimeSeconds() - LastScanTime > ScanInterval)
    {
        ScanForResourceNodes();
        LastScanTime = GetWorld()->GetTimeSeconds();
    }

    // 채집 중인 경우 진행도 업데이트
    if (CurrentState == EGatheringState::Gathering)
    {
        UpdateGatheringProgress(DeltaTime);
    }
}

void UHSGatheringComponent::ScanForResourceNodes()
{
    if (!OwnerCharacter)
        return;

    // 이전 감지 목록 초기화
    DetectedResourceNodes.Empty();

    // 구체 오버랩으로 주변 액터 검색 - 대안적인 구현 방식
    FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    
    // TArray<AActor*>를 사용하여 범위 내 모든 액터 찾기
    TArray<AActor*> FoundActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
    
    // 스위프 구체 트레이스 대신 액터 오버랩 사용
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(GetOwner());
    
    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        OwnerLocation,
        DetectionRange,
        ObjectTypes,
        AHSResourceNode::StaticClass(),
        ActorsToIgnore,
        FoundActors
    );

    // 자원 노드만 필터링
    for (AActor* Actor : FoundActors)
    {
        if (Actor && Actor->IsA<AHSResourceNode>())
        {
            if (AHSResourceNode* ResourceNode = Cast<AHSResourceNode>(Actor))
            {
                if (ResourceNode->CanBeGathered())
                {
                    DetectedResourceNodes.Add(ResourceNode);
                    
                    // 새로 감지된 노드 이벤트 발생
                    OnResourceNodeDetected.Broadcast(ResourceNode);
                }
            }
        }
    }

    // 디버그 표시
#if WITH_EDITOR
    if (GEngine && GEngine->GetNetMode(GetWorld()) != NM_DedicatedServer)
    {
        DrawDebugSphere(
            GetWorld(),
            OwnerLocation,
            DetectionRange,
            32,
            FColor::Green,
            false,
            ScanInterval
        );
    }
#endif
}

bool UHSGatheringComponent::StartGathering(AHSResourceNode* TargetNode)
{
    if (!TargetNode || !OwnerCharacter)
        return false;

    // 이미 채집 중인 경우
    if (CurrentState == EGatheringState::Gathering)
    {
        CancelGathering();
    }

    // 채집 가능 여부 확인
    if (!TargetNode->CanBeGathered())
        return false;

    // 거리 확인
    if (!IsInGatheringRange(TargetNode))
        return false;

    // 타겟 설정
    TargetResourceNode = TargetNode;
    
    // 자원 노드에 채집 시작 알림
    if (!TargetNode->StartGathering(OwnerCharacter))
    {
        TargetResourceNode.Reset();
        return false;
    }

    // 채집 시간 계산
    CurrentGatheringTime = TargetNode->GetGatheringTimePerResource() / GatheringSpeedMultiplier;
    GatheringElapsedTime = 0.0f;

    // 상태 변경
    CurrentState = EGatheringState::Gathering;

    // 애니메이션 재생
    if (GatheringMontage && OwnerCharacter->GetMesh() && OwnerCharacter->GetMesh()->GetAnimInstance())
    {
        OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Play(GatheringMontage);
    }

    // 시작 사운드 재생
    if (GatheringStartSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            GatheringStartSound,
            OwnerCharacter->GetActorLocation()
        );
    }

    // 루프 사운드 시작
    if (GatheringLoopAudioComponent)
    {
        GatheringLoopAudioComponent->Play();
    }

    // 채집 이펙트 생성
    if (GatheringProgressEffect)
    {
        ActiveGatheringEffect = UGameplayStatics::SpawnEmitterAttached(
            GatheringProgressEffect,
            OwnerCharacter->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset
        );
    }

    return true;
}

void UHSGatheringComponent::CancelGathering()
{
    if (CurrentState != EGatheringState::Gathering)
        return;

    // 애니메이션 중지
    if (GatheringMontage && OwnerCharacter && OwnerCharacter->GetMesh() && OwnerCharacter->GetMesh()->GetAnimInstance())
    {
        OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Stop(0.25f, GatheringMontage);
    }

    // 루프 사운드 중지
    if (GatheringLoopAudioComponent)
    {
        GatheringLoopAudioComponent->Stop();
    }

    // 취소 사운드 재생
    if (GatheringCancelSound && OwnerCharacter)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            GatheringCancelSound,
            OwnerCharacter->GetActorLocation()
        );
    }

    // 채집 이펙트 제거
    if (ActiveGatheringEffect)
    {
        ActiveGatheringEffect->DestroyComponent();
        ActiveGatheringEffect = nullptr;
    }

    // 상태 초기화
    CurrentState = EGatheringState::Idle;
    TargetResourceNode.Reset();
    GatheringElapsedTime = 0.0f;
    CurrentGatheringTime = 0.0f;

    // 취소 이벤트 발생
    OnGatheringCanceled.Broadcast();
}

void UHSGatheringComponent::UpdateGatheringProgress(float DeltaTime)
{
    if (!TargetResourceNode.IsValid() || !OwnerCharacter)
    {
        CancelGathering();
        return;
    }

    // 거리 체크
    if (!IsInGatheringRange(TargetResourceNode.Get()))
    {
        CancelGathering();
        return;
    }

    // 이동 중 취소 체크
    if (bCancelOnMovement)
    {
        float MovementSpeed = OwnerCharacter->GetVelocity().Size();
        if (MovementSpeed > 10.0f) // 약간의 움직임은 허용
        {
            CancelGathering();
            return;
        }
    }

    // 진행도 업데이트
    GatheringElapsedTime += DeltaTime;
    float Progress = FMath::Clamp(GatheringElapsedTime / CurrentGatheringTime, 0.0f, 1.0f);

    // 진행도 이벤트 발생
    OnGatheringProgress.Broadcast(Progress, CurrentGatheringTime);

    // 채집 완료 체크
    if (Progress >= 1.0f)
    {
        CompleteGathering();
    }
}

void UHSGatheringComponent::CompleteGathering()
{
    if (!TargetResourceNode.IsValid())
    {
        CancelGathering();
        return;
    }

    // 자원 획득
    FResourceData GatheredResource = TargetResourceNode->CompleteGathering();

    // 애니메이션 중지
    if (GatheringMontage && OwnerCharacter && OwnerCharacter->GetMesh() && OwnerCharacter->GetMesh()->GetAnimInstance())
    {
        OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Stop(0.25f, GatheringMontage);
    }

    // 루프 사운드 중지
    if (GatheringLoopAudioComponent)
    {
        GatheringLoopAudioComponent->Stop();
    }

    // 완료 사운드 재생
    if (GatheringCompleteSound && OwnerCharacter)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            GatheringCompleteSound,
            OwnerCharacter->GetActorLocation()
        );
    }

    // 채집 이펙트 제거
    if (ActiveGatheringEffect)
    {
        ActiveGatheringEffect->DestroyComponent();
        ActiveGatheringEffect = nullptr;
    }

    // 상태 변경
    CurrentState = EGatheringState::Completed;

    // 완료 이벤트 발생
    OnGatheringCompleted.Broadcast(GatheredResource);

    // 상태 초기화 - 다음 틱에서 수행
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this]()
    {
        CurrentState = EGatheringState::Idle;
        TargetResourceNode.Reset();
        GatheringElapsedTime = 0.0f;
        CurrentGatheringTime = 0.0f;
    });
    GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDelegate);
}

bool UHSGatheringComponent::IsInGatheringRange(AHSResourceNode* ResourceNode) const
{
    if (!ResourceNode || !OwnerCharacter)
        return false;

    float Distance = FVector::Dist(
        OwnerCharacter->GetActorLocation(),
        ResourceNode->GetActorLocation()
    );

    return Distance <= GatheringRange;
}

AHSResourceNode* UHSGatheringComponent::FindNearestResourceNode(EResourceType ResourceType) const
{
    if (!OwnerCharacter)
        return nullptr;

    AHSResourceNode* NearestNode = nullptr;
    float NearestDistance = FLT_MAX;
    FVector OwnerLocation = OwnerCharacter->GetActorLocation();

    for (const auto& NodePtr : DetectedResourceNodes)
    {
        if (AHSResourceNode* Node = NodePtr.Get())
        {
            // 자원 타입 필터링
            if (ResourceType != EResourceType::None && Node->GetResourceType() != ResourceType)
                continue;

            // 채집 가능 여부 확인
            if (!Node->CanBeGathered())
                continue;

            float Distance = FVector::Dist(OwnerLocation, Node->GetActorLocation());
            if (Distance < NearestDistance)
            {
                NearestDistance = Distance;
                NearestNode = Node;
            }
        }
    }

    return NearestNode;
}

TArray<AHSResourceNode*> UHSGatheringComponent::GetDetectedResourceNodes() const
{
    TArray<AHSResourceNode*> ValidNodes;
    
    for (const auto& NodePtr : DetectedResourceNodes)
    {
        if (AHSResourceNode* Node = NodePtr.Get())
        {
            if (Node->CanBeGathered())
            {
                ValidNodes.Add(Node);
            }
        }
    }

    return ValidNodes;
}

TArray<AHSResourceNode*> UHSGatheringComponent::GetResourceNodesByType(EResourceType ResourceType) const
{
    TArray<AHSResourceNode*> FilteredNodes;
    
    if (ResourceType == EResourceType::None)
        return GetDetectedResourceNodes();

    for (const auto& NodePtr : DetectedResourceNodes)
    {
        if (AHSResourceNode* Node = NodePtr.Get())
        {
            if (Node->CanBeGathered() && Node->GetResourceType() == ResourceType)
            {
                FilteredNodes.Add(Node);
            }
        }
    }

    return FilteredNodes;
}

float UHSGatheringComponent::GetGatheringProgress() const
{
    if (CurrentState != EGatheringState::Gathering || CurrentGatheringTime <= 0.0f)
        return 0.0f;

    return FMath::Clamp(GatheringElapsedTime / CurrentGatheringTime, 0.0f, 1.0f);
}
