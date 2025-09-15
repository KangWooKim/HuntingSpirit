// HuntingSpirit Game - Item Base Implementation
// 모든 아이템의 기본 클래스 구현 (임시 구현 - 추후 확장 예정)

#include "HSItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// UHSItemInstance 구현 (인벤토리/제작 시스템용)
UHSItemInstance::UHSItemInstance()
{
    // 기본 아이템 데이터 초기화
    ItemData = FHSItemData();
}

// 추가 UHSItemInstance 메서드들 구현
FString UHSItemInstance::GetItemDescription() const
{
    return ItemData.Description;
}

UTexture2D* UHSItemInstance::GetItemIcon() const
{
    return ItemData.Icon;
}

float UHSItemInstance::GetWeight() const
{
    return ItemData.Weight;
}

int32 UHSItemInstance::GetMaxStackSize() const
{
    return ItemData.StackSize;
}

int32 UHSItemInstance::GetValue() const
{
    return ItemData.Value;
}

bool UHSItemInstance::CanStack() const
{
    return ItemData.bCanStack && ItemData.StackSize > 1;
}

// 생성자
AHSItemBase::AHSItemBase()
{
    // 틱 비활성화 (필요시 활성화)
    PrimaryActorTick.bCanEverTick = false;

    // 루트 컴포넌트 설정
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    // 아이템 메시 컴포넌트 생성
    ItemMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    ItemMeshComponent->SetupAttachment(RootComponent);
    ItemMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ItemMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    ItemMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    ItemMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    // 상호작용 구체 생성
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(100.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 기본 아이템 설정
    ItemData = FHSItemData();
    CurrentQuantity = 1;
    bCanBePickedUp = true;
    bAutoPickup = false;
    bEnablePhysicsOnDrop = true;
    DropForce = 300.0f;

    // 네트워크 복제 설정
    bReplicates = true;
    SetReplicateMovement(true);
}

// 게임 시작 시 호출
void AHSItemBase::BeginPlay()
{
    Super::BeginPlay();

    // 아이템 메시 설정
    SetupItemMesh();

    // 오버랩 이벤트 바인딩
    if (InteractionSphere)
    {
        InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AHSItemBase::OnInteractionSphereBeginOverlap);
        InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AHSItemBase::OnInteractionSphereEndOverlap);
    }
}

// 매 프레임 호출
void AHSItemBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 아이템 회전 (드롭된 상태에서)
    if (bShouldRotate && !bIsPickedUp)
    {
        FRotator NewRotation = GetActorRotation();
        NewRotation.Yaw += ItemRotationSpeed * DeltaTime;
        SetActorRotation(NewRotation);
    }
}

// 아이템 픽업 처리
void AHSItemBase::OnPickup(AActor* Picker)
{
    if (!bCanBePickedUp || bIsPickedUp || !Picker)
    {
        return;
    }

    bIsPickedUp = true;

    // 픽업 효과 재생
    if (PickupEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PickupEffect, GetActorLocation());
    }

    // 픽업 사운드 재생
    if (PickupSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), PickupSound, GetActorLocation());
    }

    // 물리 비활성화
    DisablePhysics();

    // 아이템 숨기기 (인벤토리 시스템에서 처리)
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);

    UE_LOG(LogTemp, Log, TEXT("Item [%s] picked up by %s"), *ItemData.ItemName, *Picker->GetName());
}

// 아이템 드롭 처리
void AHSItemBase::OnDrop(AActor* Dropper)
{
    if (!Dropper)
    {
        return;
    }

    bIsPickedUp = false;

    // 드롭 위치 설정 (드롭하는 액터 앞쪽)
    FVector DropLocation = Dropper->GetActorLocation() + Dropper->GetActorForwardVector() * 100.0f;
    DropLocation.Z += 50.0f; // 약간 위로 올림
    SetActorLocation(DropLocation);

    // 아이템 표시
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);

    // 물리 활성화
    if (bEnablePhysicsOnDrop)
    {
        EnablePhysics();

        // 드롭 방향으로 힘 적용
        FVector DropDirection = Dropper->GetActorForwardVector() + FVector(0, 0, 0.5f);
        DropDirection.Normalize();
        ItemMeshComponent->AddImpulse(DropDirection * DropForce);
    }

    UE_LOG(LogTemp, Log, TEXT("Item [%s] dropped by %s"), *ItemData.ItemName, *Dropper->GetName());
}

// 아이템 사용 처리
void AHSItemBase::OnUse(AActor* User)
{
    if (!User)
    {
        return;
    }

    // 기본 클래스에서는 로그만 출력
    // 실제 사용 효과는 파생 클래스에서 구현
    UE_LOG(LogTemp, Log, TEXT("Item [%s] used by %s"), *ItemData.ItemName, *User->GetName());
}

// 아이템 데이터 설정
void AHSItemBase::SetItemData(const FHSItemData& NewData)
{
    ItemData = NewData;
    SetupItemMesh();
}

// 아이템 수량 설정
void AHSItemBase::SetItemQuantity(int32 NewQuantity)
{
    CurrentQuantity = FMath::Max(0, NewQuantity);

    // 수량이 0이 되면 아이템 제거
    if (CurrentQuantity <= 0)
    {
        Destroy();
    }
}

// UHSItemInstance 인스턴스 생성
UHSItemInstance* AHSItemBase::CreateItemInstance()
{
    UHSItemInstance* ItemInstance = NewObject<UHSItemInstance>(this);
    if (ItemInstance)
    {
        ItemInstance->SetItemData(ItemData);
    }
    return ItemInstance;
}

// 아이템 메시 설정
void AHSItemBase::SetupItemMesh()
{
    if (ItemMeshComponent && ItemData.ItemMesh)
    {
        ItemMeshComponent->SetStaticMesh(ItemData.ItemMesh);
    }
}

// 물리 활성화
void AHSItemBase::EnablePhysics()
{
    if (ItemMeshComponent)
    {
        ItemMeshComponent->SetSimulatePhysics(true);
        ItemMeshComponent->SetEnableGravity(true);
        ItemMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ItemMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
        ItemMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        ItemMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }
}

// 물리 비활성화
void AHSItemBase::DisablePhysics()
{
    if (ItemMeshComponent)
    {
        ItemMeshComponent->SetSimulatePhysics(false);
        ItemMeshComponent->SetEnableGravity(false);
        ItemMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

// 상호작용 구체 오버랩 시작
void AHSItemBase::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this || bIsPickedUp)
    {
        return;
    }

    // 자동 픽업이 활성화된 경우
    if (bAutoPickup && bCanBePickedUp)
    {
        OnPickup(OtherActor);
    }
}

// 상호작용 구체 오버랩 종료
void AHSItemBase::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, 
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 필요시 구현
}
