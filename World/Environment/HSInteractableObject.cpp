// HSInteractableObject.cpp
// 플레이어가 상호작용할 수 있는 오브젝트의 기본 클래스 구현

#include "HSInteractableObject.h"
#include "../../Characters/Base/HSCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

// 생성자
AHSInteractableObject::AHSInteractableObject()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // 루트 컴포넌트로 메시 컴포넌트 생성
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

    // 상호작용 범위 충돌체 생성
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(200.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 상호작용 위젯 컴포넌트 생성
    InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
    InteractionWidget->SetupAttachment(RootComponent);
    InteractionWidget->SetRelativeLocation(FVector(0, 0, 100));
    InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
    InteractionWidget->SetDrawSize(FVector2D(200, 50));
    InteractionWidget->SetVisibility(false);

    // 오디오 컴포넌트 생성
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->bAutoActivate = false;

    // 파티클 시스템 컴포넌트 생성
    ParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComponent"));
    ParticleComponent->SetupAttachment(RootComponent);
    ParticleComponent->bAutoActivate = false;

    // 초기값 설정
    CurrentState = EInteractionState::IS_Ready;
    CurrentInteractingCharacter = nullptr;
    InteractionProgress = 0.0f;
    InteractionStartTime = 0.0f;
    CooldownEndTime = 0.0f;
    InteractionCount = 0;
    bIsInteractionEnabled = true;
    bShowDebugInfo = false;
}

// BeginPlay
void AHSInteractableObject::BeginPlay()
{
    Super::BeginPlay();

    // 상호작용 범위 설정
    if (InteractionSphere)
    {
        InteractionSphere->SetSphereRadius(InteractionData.InteractionDistance);
    }

    // 오디오 설정
    if (AudioComponent && InteractionData.InteractionSound)
    {
        AudioComponent->SetSound(InteractionData.InteractionSound);
    }

    // 파티클 설정
    if (ParticleComponent && InteractionData.InteractionEffect)
    {
        ParticleComponent->SetTemplate(InteractionData.InteractionEffect);
    }
}

// EndPlay
void AHSInteractableObject::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 진행 중인 상호작용 취소
    if (CurrentInteractingCharacter)
    {
        EndInteraction(CurrentInteractingCharacter, false);
    }

    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);

    Super::EndPlay(EndPlayReason);
}

// 복제 속성 설정
void AHSInteractableObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AHSInteractableObject, CurrentState);
    DOREPLIFETIME(AHSInteractableObject, InteractionProgress);
}

// Tick
void AHSInteractableObject::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 상호작용 진행 중인 경우 진행률 업데이트
    if (CurrentState == EInteractionState::IS_InProgress && CurrentInteractingCharacter)
    {
        float ElapsedTime = GetWorld()->GetTimeSeconds() - InteractionStartTime;
        
        if (InteractionData.InteractionDuration > 0.0f)
        {
            InteractionProgress = FMath::Clamp(ElapsedTime / InteractionData.InteractionDuration, 0.0f, 1.0f);
            
            // 진행률 델리게이트 호출
            OnInteractionProgress.Broadcast(CurrentInteractingCharacter, InteractionProgress);

            // 상호작용 완료 확인
            if (InteractionProgress >= 1.0f)
            {
                EndInteraction(CurrentInteractingCharacter, true);
            }
        }
        else
        {
            // 즉시 완료되는 상호작용
            EndInteraction(CurrentInteractingCharacter, true);
        }
    }

    // 디버그 정보 표시
    if (bShowDebugInfo)
    {
        DrawDebugInfo();
    }
}

// 상호작용 가능 여부 확인
bool AHSInteractableObject::CanInteract(AHSCharacterBase* Character) const
{
    if (!Character || !bIsInteractionEnabled)
    {
        return false;
    }

    // 현재 상태 확인
    if (CurrentState != EInteractionState::IS_Ready)
    {
        return false;
    }

    // 거리 확인
    float Distance = FVector::Dist(GetActorLocation(), Character->GetActorLocation());
    if (Distance > InteractionData.InteractionDistance)
    {
        return false;
    }

    // 추가 조건 확인
    return CheckInteractionConditions(Character);
}

// 상호작용 시작
void AHSInteractableObject::StartInteraction(AHSCharacterBase* Character)
{
    if (!CanInteract(Character))
    {
        return;
    }

    // 상태 변경
    SetInteractionState(EInteractionState::IS_InProgress);
    CurrentInteractingCharacter = Character;
    InteractionStartTime = GetWorld()->GetTimeSeconds();
    InteractionProgress = 0.0f;

    // 델리게이트 호출
    OnInteractionStarted.Broadcast(Character);

    // 효과 재생
    PlayInteractionEffects();

    UE_LOG(LogTemp, Log, TEXT("상호작용 시작: %s가 %s와 상호작용"), *Character->GetName(), *GetName());
}

// 상호작용 종료
void AHSInteractableObject::EndInteraction(AHSCharacterBase* Character, bool bWasCompleted)
{
    if (CurrentInteractingCharacter != Character)
    {
        return;
    }

    if (bWasCompleted)
    {
        // 상호작용 완료 처리
        InteractionCount++;
        HandleInteractionCompleted_Implementation(Character);
        OnInteractionCompleted.Broadcast(Character);

        // 상호작용 타입에 따른 처리
        switch (InteractionData.InteractionType)
        {
        case EInteractionType::IT_OneTime:
            SetInteractionState(EInteractionState::IS_Completed);
            break;
        
        case EInteractionType::IT_Repeatable:
        case EInteractionType::IT_Conditional:
        case EInteractionType::IT_Timed:
            if (InteractionData.CooldownTime > 0.0f)
            {
                SetInteractionState(EInteractionState::IS_Cooldown);
                HandleCooldown();
            }
            else
            {
                SetInteractionState(EInteractionState::IS_Ready);
            }
            break;
        }

        UE_LOG(LogTemp, Log, TEXT("상호작용 완료: %s가 %s와의 상호작용 완료"), *Character->GetName(), *GetName());
    }
    else
    {
        // 상호작용 취소
        OnInteractionCancelled.Broadcast(Character);
        SetInteractionState(EInteractionState::IS_Ready);

        UE_LOG(LogTemp, Log, TEXT("상호작용 취소: %s가 %s와의 상호작용 취소"), *Character->GetName(), *GetName());
    }

    // 상태 초기화
    CurrentInteractingCharacter = nullptr;
    InteractionProgress = 0.0f;
    InteractionStartTime = 0.0f;

    // 효과 중지
    if (AudioComponent && AudioComponent->IsPlaying())
    {
        AudioComponent->Stop();
    }
    if (ParticleComponent && ParticleComponent->IsActive())
    {
        ParticleComponent->Deactivate();
    }
}

// 상호작용 진행률 가져오기
float AHSInteractableObject::GetInteractionProgress() const
{
    return InteractionProgress;
}

// 상호작용 활성화/비활성화
void AHSInteractableObject::SetInteractionEnabled(bool bEnabled)
{
    bIsInteractionEnabled = bEnabled;
    
    if (!bEnabled)
    {
        // 진행 중인 상호작용 취소
        if (CurrentInteractingCharacter)
        {
            EndInteraction(CurrentInteractingCharacter, false);
        }
        SetInteractionState(EInteractionState::IS_Disabled);
    }
    else
    {
        if (CurrentState == EInteractionState::IS_Disabled)
        {
            SetInteractionState(EInteractionState::IS_Ready);
        }
    }
}

// 상호작용 리셋
void AHSInteractableObject::ResetInteraction()
{
    // 진행 중인 상호작용 취소
    if (CurrentInteractingCharacter)
    {
        EndInteraction(CurrentInteractingCharacter, false);
    }

    // 상태 초기화
    SetInteractionState(EInteractionState::IS_Ready);
    InteractionCount = 0;
    CooldownEndTime = 0.0f;
    
    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
}

// 상호작용 완료 시 호출되는 함수 (서브클래스에서 오버라이드)
void AHSInteractableObject::HandleInteractionCompleted_Implementation(AHSCharacterBase* Character)
{
    // 서브클래스에서 구현
    UE_LOG(LogTemp, Log, TEXT("HandleInteractionCompleted_Implementation: %s"), *GetName());
}

// 상호작용 조건 확인 (서브클래스에서 오버라이드)
bool AHSInteractableObject::CheckInteractionConditions_Implementation(AHSCharacterBase* Character) const
{
    // 기본적으로 true 반환, 서브클래스에서 추가 조건 구현
    return true;
}

// 상호작용 시각 효과 재생
void AHSInteractableObject::PlayInteractionEffects()
{
    // 사운드 재생
    if (AudioComponent && InteractionData.InteractionSound)
    {
        AudioComponent->Play();
    }

    // 파티클 재생
    if (ParticleComponent && InteractionData.InteractionEffect)
    {
        ParticleComponent->Activate(true);
    }
}

// 쿨다운 처리
void AHSInteractableObject::HandleCooldown()
{
    CooldownEndTime = GetWorld()->GetTimeSeconds() + InteractionData.CooldownTime;
    
    GetWorld()->GetTimerManager().SetTimer(
        CooldownTimerHandle,
        [this]()
        {
            SetInteractionState(EInteractionState::IS_Ready);
            CooldownEndTime = 0.0f;
        },
        InteractionData.CooldownTime,
        false
    );
}

// 상태 변경
void AHSInteractableObject::SetInteractionState(EInteractionState NewState)
{
    if (CurrentState != NewState)
    {
        CurrentState = NewState;
        OnRep_CurrentState();
    }
}

// 네트워크 상태 동기화
void AHSInteractableObject::OnRep_CurrentState()
{
    // 상태에 따른 시각적 변경 처리
    switch (CurrentState)
    {
    case EInteractionState::IS_Ready:
        // 상호작용 가능 상태 시각화
        if (MeshComponent)
        {
            MeshComponent->SetRenderCustomDepth(false);
        }
        break;
        
    case EInteractionState::IS_InProgress:
        // 상호작용 진행 중 시각화
        if (MeshComponent)
        {
            MeshComponent->SetRenderCustomDepth(true);
            MeshComponent->SetCustomDepthStencilValue(1);
        }
        break;
        
    case EInteractionState::IS_Completed:
    case EInteractionState::IS_Disabled:
        // 완료/비활성화 상태 시각화
        if (MeshComponent)
        {
            MeshComponent->SetRenderCustomDepth(true);
            MeshComponent->SetCustomDepthStencilValue(2);
        }
        break;
        
    case EInteractionState::IS_Cooldown:
        // 쿨다운 상태 시각화
        if (MeshComponent)
        {
            MeshComponent->SetRenderCustomDepth(true);
            MeshComponent->SetCustomDepthStencilValue(3);
        }
        break;
    }
}

// 상호작용 진행률 동기화
void AHSInteractableObject::OnRep_InteractionProgress()
{
    // 클라이언트에서 진행률 UI 업데이트 등의 처리
}

// 디버그 정보 표시
void AHSInteractableObject::DrawDebugInfo()
{
    if (!GetWorld())
    {
        return;
    }

    // 상호작용 범위 표시
    DrawDebugSphere(
        GetWorld(),
        GetActorLocation(),
        InteractionData.InteractionDistance,
        32,
        FColor::Green,
        false,
        -1.0f,
        0,
        2.0f
    );

    // 상태 정보 표시
    FString StateString;
    switch (CurrentState)
    {
    case EInteractionState::IS_Ready:
        StateString = TEXT("Ready");
        break;
    case EInteractionState::IS_InProgress:
        StateString = FString::Printf(TEXT("In Progress: %.1f%%"), InteractionProgress * 100.0f);
        break;
    case EInteractionState::IS_Completed:
        StateString = TEXT("Completed");
        break;
    case EInteractionState::IS_Cooldown:
        StateString = FString::Printf(TEXT("Cooldown: %.1fs"), FMath::Max(0.0f, CooldownEndTime - GetWorld()->GetTimeSeconds()));
        break;
    case EInteractionState::IS_Disabled:
        StateString = TEXT("Disabled");
        break;
    }

    DrawDebugString(
        GetWorld(),
        GetActorLocation() + FVector(0, 0, 150),
        StateString,
        nullptr,
        FColor::Yellow,
        0.0f,
        true,
        1.0f
    );

    // 상호작용 정보 표시
    FString InfoString = FString::Printf(
        TEXT("Type: %s\nCount: %d\nEnabled: %s"),
        *UEnum::GetValueAsString(InteractionData.InteractionType),
        InteractionCount,
        bIsInteractionEnabled ? TEXT("Yes") : TEXT("No")
    );

    DrawDebugString(
        GetWorld(),
        GetActorLocation() + FVector(0, 0, 100),
        InfoString,
        nullptr,
        FColor::White,
        0.0f,
        true,
        0.8f
    );
}
