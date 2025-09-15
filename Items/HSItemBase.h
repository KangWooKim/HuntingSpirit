// HuntingSpirit Game - Item Base Header
// 모든 아이템의 기본 클래스 (임시 구현 - 추후 확장 예정)
// 아이템 시스템의 기초를 제공하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "HSItemBase.generated.h"

// 아이템 타입 열거형
UENUM(BlueprintType)
enum class EHSItemType : uint8
{
    None        UMETA(DisplayName = "None"),
    Weapon      UMETA(DisplayName = "Weapon"),
    Armor       UMETA(DisplayName = "Armor"),
    Consumable  UMETA(DisplayName = "Consumable"),
    Resource    UMETA(DisplayName = "Resource"),
    Material    UMETA(DisplayName = "Material"),
    Quest       UMETA(DisplayName = "Quest Item"),
    Currency    UMETA(DisplayName = "Currency"),
    Misc        UMETA(DisplayName = "Miscellaneous")
};

// 아이템 레어도 열거형
UENUM(BlueprintType)
enum class EHSItemRarity : uint8
{
    Common      UMETA(DisplayName = "Common"),
    Uncommon    UMETA(DisplayName = "Uncommon"),
    Rare        UMETA(DisplayName = "Rare"),
    Epic        UMETA(DisplayName = "Epic"),
    Legendary   UMETA(DisplayName = "Legendary"),
    Mythic      UMETA(DisplayName = "Mythic")
};

// 아이템 데이터 구조체
USTRUCT(BlueprintType)
struct FHSItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ItemName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = "true"))
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHSItemType ItemType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHSItemRarity Rarity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StackSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Weight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanStack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Value;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* ItemMesh;

    FHSItemData()
    {
        ItemName = TEXT("Unknown Item");
        Description = TEXT("No description available.");
        ItemType = EHSItemType::None;
        Rarity = EHSItemRarity::Common;
        StackSize = 1;
        Weight = 1.0f;
        Value = 0;
        Icon = nullptr;
        ItemMesh = nullptr;
        bCanStack = false;
    }
};

// 인벤토리/제작 시스템 호환을 위한 UObject 기반 아이템 인스턴스 클래스
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSItemInstance : public UObject
{
    GENERATED_BODY()

public:
    // 생성자
    UHSItemInstance();

    // 아이템 데이터 접근자
    UFUNCTION(BlueprintPure, Category = "Item")
    const FHSItemData& GetItemData() const { return ItemData; }

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetItemName() const { return ItemData.ItemName; }

    UFUNCTION(BlueprintPure, Category = "Item")
    EHSItemType GetItemType() const { return ItemData.ItemType; }

    UFUNCTION(BlueprintPure, Category = "Item")
    EHSItemRarity GetItemRarity() const { return ItemData.Rarity; }

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetItemDescription() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    UTexture2D* GetItemIcon() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    float GetWeight() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetMaxStackSize() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetValue() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    bool CanStack() const;

    // 아이템 설정
    UFUNCTION(BlueprintCallable, Category = "Item")
    void SetItemData(const FHSItemData& NewData) { ItemData = NewData; }

protected:
    // 아이템 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FHSItemData ItemData;
};

// 월드에 배치 가능한 아이템 액터 클래스
UCLASS()
class HUNTINGSPIRIT_API AHSItemBase : public AActor
{
    GENERATED_BODY()

public:
    // 생성자
    AHSItemBase();

    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;

    // 아이템 정보 접근자
    UFUNCTION(BlueprintPure, Category = "Item")
    const FHSItemData& GetItemData() const { return ItemData; }

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetItemName() const { return ItemData.ItemName; }

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetItemDescription() const { return ItemData.Description; }

    UFUNCTION(BlueprintPure, Category = "Item")
    EHSItemType GetItemType() const { return ItemData.ItemType; }

    UFUNCTION(BlueprintPure, Category = "Item")
    EHSItemRarity GetItemRarity() const { return ItemData.Rarity; }

    UFUNCTION(BlueprintPure, Category = "Item")
    UTexture2D* GetItemIcon() const { return ItemData.Icon; }

    UFUNCTION(BlueprintPure, Category = "Item")
    float GetWeight() const { return ItemData.Weight; }

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetMaxStackSize() const { return ItemData.StackSize; }

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetValue() const { return ItemData.Value; }

    UFUNCTION(BlueprintPure, Category = "Item")
    bool CanStack() const { return ItemData.StackSize > 1; }

    // 아이템 상호작용
    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual void OnPickup(AActor* Picker);

    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual void OnDrop(AActor* Dropper);

    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual void OnUse(AActor* User);

    // 아이템 설정
    UFUNCTION(BlueprintCallable, Category = "Item")
    void SetItemData(const FHSItemData& NewData);

    UFUNCTION(BlueprintCallable, Category = "Item")
    void SetItemQuantity(int32 NewQuantity);

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetItemQuantity() const { return CurrentQuantity; }

    // UHSItemInstance 인스턴스 생성 및 반환
    UFUNCTION(BlueprintCallable, Category = "Item")
    UHSItemInstance* CreateItemInstance();

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 아이템 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FHSItemData ItemData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 CurrentQuantity = 1;

    // 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* ItemMeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* InteractionSphere;

    // 아이템 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item State")
    bool bIsPickedUp = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item State")
    bool bCanBePickedUp = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item State")
    bool bAutoPickup = false;

    // 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class UParticleSystem* PickupEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    class USoundBase* PickupSound;

    // 드롭 물리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bEnablePhysicsOnDrop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float DropForce = 300.0f;

protected:
    // 내부 함수
    virtual void SetupItemMesh();
    virtual void EnablePhysics();
    virtual void DisablePhysics();

    // 오버랩 이벤트
    UFUNCTION()
    virtual void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, 
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    // 아이템 회전 (드롭된 상태에서)
    float ItemRotationSpeed = 45.0f;
    bool bShouldRotate = true;
};
