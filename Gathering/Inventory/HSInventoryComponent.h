#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "../../Items/HSItemBase.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "HSInventoryComponent.generated.h"

class UHSItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryChanged, int32, SlotIndex, UHSItemInstance*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemAdded, UHSItemInstance*, Item, int32, Quantity, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemRemoved, UHSItemInstance*, Item, int32, Quantity, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryFull, UHSItemInstance*, FailedItem);

/**
 * 인벤토리 슬롯 정보 구조체
 * 네트워크 복제 지원 및 최적화된 데이터 구조
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UHSItemInstance> Item;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxStackSize;

    // 성능 최적화를 위한 슬롯 상태 플래그
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsLocked;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEmpty;

    FHSInventorySlot()
        : Item(nullptr)
        , Quantity(0)
        , MaxStackSize(99)
        , bIsLocked(false)
        , bIsEmpty(true)
    {
    }

    bool IsValid() const
    {
        return Item != nullptr && Quantity > 0 && !bIsEmpty;
    }

    bool CanStack(UHSItemInstance* InItem) const;
    bool HasSpace(int32 InQuantity) const;
    void Clear();
};

/**
 * FastArraySerializer 기반 슬롯 래퍼
 * TODO: FFastArraySerializer를 통한 슬롯 델타 복제로 전환 시 본 구조를 사용
 */
USTRUCT()
struct FHSInventorySlotFastArrayItem : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY()
    int32 SlotIndex;

    UPROPERTY()
    FHSInventorySlot Slot;

    FHSInventorySlotFastArrayItem()
        : SlotIndex(INDEX_NONE)
    {
    }
};

USTRUCT()
struct FHSInventorySlotFastArray : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FHSInventorySlotFastArrayItem> Items;

    void SyncFromLegacyArray(const TArray<FHSInventorySlot>& SourceSlots, bool bMarkDirty);

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize(Items, DeltaParms, *this);
    }
};

template<>
struct TStructOpsTypeTraits<FHSInventorySlotFastArray> : public TStructOpsTypeTraitsBase2<FHSInventorySlotFastArray>
{
    enum
    {
        WithNetDeltaSerializer = true
    };
};

/**
 * 인벤토리 필터 타입
 */
UENUM(BlueprintType)
enum class EHSInventoryFilter : uint8
{
    None        UMETA(DisplayName = "모든 아이템"),
    Weapons     UMETA(DisplayName = "무기"),
    Armor       UMETA(DisplayName = "방어구"),
    Consumables UMETA(DisplayName = "소모품"),
    Materials   UMETA(DisplayName = "재료"),
    Quest       UMETA(DisplayName = "퀘스트 아이템")
};

/**
 * 인벤토리 컴포넌트
 * 멀티플레이어 환경에서 동기화되는 인벤토리 시스템
 * 오브젝트 풀링, 메모리 캐싱, 조건부 네트워크 복제 적용
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHSInventoryComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 인벤토리 슬롯 배열 (네트워크 복제)
    UPROPERTY(ReplicatedUsing = OnRep_InventorySlots, BlueprintReadOnly, Category = "Inventory")
    TArray<FHSInventorySlot> InventorySlots;

public:
    // 인벤토리 설정 (UI 접근을 위해 public으로 변경)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
    int32 MaxSlots;

protected:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
    bool bAutoSort;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
    bool bStackSimilarItems;

    // 성능 최적화를 위한 캐시 (네트워크 복제 불필요)
    TMap<TObjectPtr<UHSItemInstance>, int32> ItemQuantityCache;
    TArray<int32> EmptySlotCache;

    // 마지막 업데이트 시간 (네트워크 최적화)
    UPROPERTY()
    float LastNetworkUpdate;

    // 타이머 핸들 (네트워크 최적화용)
    FTimerHandle NetworkOptimizationTimerHandle;

public:
    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
    FOnInventoryChanged OnInventoryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
    FOnItemAdded OnItemAdded;

    UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
    FOnItemRemoved OnItemRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
    FOnInventoryFull OnInventoryFull;

    // 기본 인벤토리 기능
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(UHSItemInstance* Item, int32 Quantity, int32& OutSlotIndex);

    // 편의용 오버로드 함수 (UFUNCTION 아님)
    bool AddItemSimple(UHSItemInstance* Item, int32& OutSlotIndex) { return AddItem(Item, 1, OutSlotIndex); }

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(UHSItemInstance* Item, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItemFromSlot(int32 SlotIndex, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool MoveItem(int32 FromSlot, int32 ToSlot);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool SwapItems(int32 SlotA, int32 SlotB);

    // 아이템 검색 및 정보
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    FHSInventorySlot GetSlot(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    UHSItemInstance* GetItemInSlot(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    int32 GetItemQuantity(UHSItemInstance* Item) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool HasItem(UHSItemInstance* Item, int32 Quantity = 1) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool HasSpaceForItem(UHSItemInstance* Item, int32 Quantity = 1) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    int32 GetEmptySlotCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    TArray<FHSInventorySlot> GetFilteredItems(EHSInventoryFilter Filter) const;

    // 인벤토리 관리
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SortInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ClearInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ResizeInventory(int32 NewSize);

    // 네트워크 함수
    UFUNCTION(Server, Reliable, Category = "Inventory Network")
    void ServerAddItem(UHSItemInstance* Item, int32 Quantity);

    UFUNCTION(Server, Reliable, Category = "Inventory Network")
    void ServerRemoveItem(UHSItemInstance* Item, int32 Quantity);

    UFUNCTION(Server, Reliable, Category = "Inventory Network")
    void ServerMoveItem(int32 FromSlot, int32 ToSlot);

    UFUNCTION(NetMulticast, Reliable, Category = "Inventory Network")
    void MulticastInventoryUpdate(int32 SlotIndex, UHSItemInstance* Item, int32 Quantity);

protected:
    // 네트워크 복제 콜백
    UFUNCTION()
    void OnRep_InventorySlots();

    // 내부 헬퍼 함수
    int32 FindEmptySlot();
    int32 FindSlotWithItem(UHSItemInstance* Item) const;
    int32 FindSlotWithSpace(UHSItemInstance* Item) const;
    void UpdateItemCache();
    void UpdateEmptySlotCache();
    bool IsValidSlotIndex(int32 SlotIndex) const;
    void BroadcastInventoryChanged(int32 SlotIndex, UHSItemInstance* Item);
    void SyncFastArrayState();

    // 성능 최적화 함수
    void OptimizeNetworkUpdates();
    void CacheFrequentlyUsedData();

private:
    // 네트워크 최적화를 위한 변수
    static constexpr float NetworkUpdateInterval = 0.1f;

    // FFastArraySerializer 전환 준비용 내부 상태
    UPROPERTY()
    FHSInventorySlotFastArray ReplicatedFastSlots;
};
