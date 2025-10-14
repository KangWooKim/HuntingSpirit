#include "HSInventoryComponent.h"
#include "../../Items/HSItemBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

bool FHSInventorySlot::CanStack(UHSItemInstance* InItem) const
{
    if (!Item || !InItem || bIsEmpty || bIsLocked)
    {
        return false;
    }

    // 동일한 아이템인지 확인
    if (Item->GetClass() != InItem->GetClass())
    {
        return false;
    }

    // 스택 가능한지 확인
    return Item->CanStack() && Quantity < MaxStackSize;
}

bool FHSInventorySlot::HasSpace(int32 InQuantity) const
{
    if (bIsEmpty)
    {
        return InQuantity <= MaxStackSize;
    }

    return !bIsLocked && (Quantity + InQuantity) <= MaxStackSize;
}

void FHSInventorySlot::Clear()
{
    Item = nullptr;
    Quantity = 0;
    bIsEmpty = true;
    bIsLocked = false;
}

void FHSInventorySlotFastArray::SyncFromLegacyArray(const TArray<FHSInventorySlot>& SourceSlots, bool bMarkDirty)
{
    Items.Reset(SourceSlots.Num());
    for (int32 Index = 0; Index < SourceSlots.Num(); ++Index)
    {
        FHSInventorySlotFastArrayItem& NewItem = Items.AddDefaulted_GetRef();
        NewItem.SlotIndex = Index;
        NewItem.Slot = SourceSlots[Index];
    }

    if (bMarkDirty)
    {
        MarkArrayDirty();
    }
}

UHSInventoryComponent::UHSInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    // 기본 인벤토리 설정
    MaxSlots = 36; // 6x6 그리드
    bAutoSort = false;
    bStackSimilarItems = true;
    LastNetworkUpdate = 0.0f;

    // 슬롯 초기화
    InventorySlots.SetNum(MaxSlots);
    for (int32 i = 0; i < MaxSlots; ++i)
    {
        InventorySlots[i] = FHSInventorySlot();
    }

    ReplicatedFastSlots.SyncFromLegacyArray(InventorySlots, /*bMarkDirty=*/false);
}

void UHSInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // 캐시 초기화
    UpdateItemCache();
    UpdateEmptySlotCache();
    SyncFastArrayState();

    // 네트워크 최적화 타이머 설정
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        GetWorld()->GetTimerManager().SetTimer(
            NetworkOptimizationTimerHandle,
            this,
            &UHSInventoryComponent::OptimizeNetworkUpdates,
            NetworkUpdateInterval,
            true
        );
    }
}

void UHSInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // 조건부 복제로 네트워크 트래픽 최적화
    DOREPLIFETIME_CONDITION(UHSInventoryComponent, InventorySlots, COND_OwnerOnly);
}

bool UHSInventoryComponent::AddItem(UHSItemInstance* Item, int32 Quantity, int32& OutSlotIndex)
{
    if (!Item || Quantity <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSInventoryComponent::AddItem - 유효하지 않은 아이템 또는 수량"));
        return false;
    }

    // 서버에서만 실행
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerAddItem(Item, Quantity);
        return true; // 클라이언트에서는 일단 성공으로 처리
    }

    int32 RemainingQuantity = Quantity;
    OutSlotIndex = -1;

    // 1. 기존 스택에 추가 시도
    if (bStackSimilarItems && Item->CanStack())
    {
        for (int32 i = 0; i < InventorySlots.Num(); ++i)
        {
            FHSInventorySlot& Slot = InventorySlots[i];
            if (Slot.CanStack(Item))
            {
                int32 CanAdd = FMath::Min(RemainingQuantity, Slot.MaxStackSize - Slot.Quantity);
                Slot.Quantity += CanAdd;
                RemainingQuantity -= CanAdd;

                if (OutSlotIndex == -1)
                {
                    OutSlotIndex = i;
                }

                BroadcastInventoryChanged(i, Slot.Item);

                if (RemainingQuantity <= 0)
                {
                    break;
                }
            }
        }
    }

    // 2. 새 슬롯에 추가
    while (RemainingQuantity > 0)
    {
        int32 EmptySlotIndex = FindEmptySlot();
        if (EmptySlotIndex == -1)
        {
            // 인벤토리가 가득 참
            OnInventoryFull.Broadcast(Item);
            UE_LOG(LogTemp, Warning, TEXT("HSInventoryComponent::AddItem - 인벤토리가 가득 참"));
            return false;
        }

        FHSInventorySlot& Slot = InventorySlots[EmptySlotIndex];
        int32 CanAdd = FMath::Min(RemainingQuantity, Item->GetMaxStackSize());
        
        Slot.Item = Item;
        Slot.Quantity = CanAdd;
        Slot.MaxStackSize = Item->GetMaxStackSize();
        Slot.bIsEmpty = false;

        RemainingQuantity -= CanAdd;

        if (OutSlotIndex == -1)
        {
            OutSlotIndex = EmptySlotIndex;
        }

        BroadcastInventoryChanged(EmptySlotIndex, Item);
        OnItemAdded.Broadcast(Item, CanAdd, EmptySlotIndex);
    }

    // 캐시 업데이트
    UpdateItemCache();
    UpdateEmptySlotCache();

    return true;
}

bool UHSInventoryComponent::RemoveItem(UHSItemInstance* Item, int32 Quantity)
{
    if (!Item || Quantity <= 0)
    {
        return false;
    }

    // 서버에서만 실행
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerRemoveItem(Item, Quantity);
        return true;
    }

    int32 RemainingQuantity = Quantity;

    // 뒤에서부터 제거 (LIFO 방식)
    for (int32 i = InventorySlots.Num() - 1; i >= 0 && RemainingQuantity > 0; --i)
    {
        FHSInventorySlot& Slot = InventorySlots[i];
        if (Slot.Item == Item && !Slot.bIsEmpty)
        {
            int32 CanRemove = FMath::Min(RemainingQuantity, Slot.Quantity);
            Slot.Quantity -= CanRemove;
            RemainingQuantity -= CanRemove;

            if (Slot.Quantity <= 0)
            {
                Slot.Clear();
            }

            BroadcastInventoryChanged(i, Slot.bIsEmpty ? nullptr : Slot.Item);
            OnItemRemoved.Broadcast(Item, CanRemove, i);
        }
    }

    // 캐시 업데이트
    UpdateItemCache();
    UpdateEmptySlotCache();

    return RemainingQuantity == 0;
}

bool UHSInventoryComponent::RemoveItemFromSlot(int32 SlotIndex, int32 Quantity)
{
    if (!IsValidSlotIndex(SlotIndex) || Quantity <= 0)
    {
        return false;
    }

    FHSInventorySlot& Slot = InventorySlots[SlotIndex];
    if (Slot.bIsEmpty || Slot.bIsLocked)
    {
        return false;
    }

    int32 CanRemove = FMath::Min(Quantity, Slot.Quantity);
    UHSItemInstance* Item = Slot.Item;
    
    Slot.Quantity -= CanRemove;
    if (Slot.Quantity <= 0)
    {
        Slot.Clear();
    }

        BroadcastInventoryChanged(SlotIndex, Slot.bIsEmpty ? nullptr : Slot.Item);
        OnItemRemoved.Broadcast(Item, CanRemove, SlotIndex);

    // 캐시 업데이트
    UpdateItemCache();
    UpdateEmptySlotCache();

    return true;
}

bool UHSInventoryComponent::MoveItem(int32 FromSlot, int32 ToSlot)
{
    if (!IsValidSlotIndex(FromSlot) || !IsValidSlotIndex(ToSlot) || FromSlot == ToSlot)
    {
        return false;
    }

    // 서버에서만 실행
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerMoveItem(FromSlot, ToSlot);
        return true;
    }

    FHSInventorySlot& FromSlotRef = InventorySlots[FromSlot];
    FHSInventorySlot& ToSlotRef = InventorySlots[ToSlot];

    if (FromSlotRef.bIsEmpty || FromSlotRef.bIsLocked || ToSlotRef.bIsLocked)
    {
        return false;
    }

    // 대상 슬롯이 비어있으면 단순 이동
    if (ToSlotRef.bIsEmpty)
    {
        ToSlotRef = FromSlotRef;
        FromSlotRef.Clear();
    }
    // 같은 아이템이고 스택 가능하면 병합
    else if (ToSlotRef.CanStack(FromSlotRef.Item))
    {
        int32 CanMove = FMath::Min(FromSlotRef.Quantity, ToSlotRef.MaxStackSize - ToSlotRef.Quantity);
        ToSlotRef.Quantity += CanMove;
        FromSlotRef.Quantity -= CanMove;

        if (FromSlotRef.Quantity <= 0)
        {
            FromSlotRef.Clear();
        }
    }
    // 그 외에는 스왑
    else
    {
        FHSInventorySlot TempSlot = FromSlotRef;
        FromSlotRef = ToSlotRef;
        ToSlotRef = TempSlot;
    }

    BroadcastInventoryChanged(FromSlot, FromSlotRef.bIsEmpty ? nullptr : FromSlotRef.Item);
    BroadcastInventoryChanged(ToSlot, ToSlotRef.bIsEmpty ? nullptr : ToSlotRef.Item);

    return true;
}

bool UHSInventoryComponent::SwapItems(int32 SlotA, int32 SlotB)
{
    if (!IsValidSlotIndex(SlotA) || !IsValidSlotIndex(SlotB) || SlotA == SlotB)
    {
        return false;
    }

    FHSInventorySlot& SlotRefA = InventorySlots[SlotA];
    FHSInventorySlot& SlotRefB = InventorySlots[SlotB];

    if (SlotRefA.bIsLocked || SlotRefB.bIsLocked)
    {
        return false;
    }

    // 스왑
    FHSInventorySlot TempSlot = SlotRefA;
    SlotRefA = SlotRefB;
    SlotRefB = TempSlot;

    BroadcastInventoryChanged(SlotA, SlotRefA.bIsEmpty ? nullptr : SlotRefA.Item);
    BroadcastInventoryChanged(SlotB, SlotRefB.bIsEmpty ? nullptr : SlotRefB.Item);

    return true;
}

FHSInventorySlot UHSInventoryComponent::GetSlot(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return FHSInventorySlot();
    }

    return InventorySlots[SlotIndex];
}

UHSItemInstance* UHSInventoryComponent::GetItemInSlot(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return nullptr;
    }

    return InventorySlots[SlotIndex].Item;
}

int32 UHSInventoryComponent::GetItemQuantity(UHSItemInstance* Item) const
{
    if (!Item)
    {
        return 0;
    }

    // 캐시 사용으로 성능 최적화
    if (const int32* CachedQuantity = ItemQuantityCache.Find(Item))
    {
        return *CachedQuantity;
    }

    return 0;
}

bool UHSInventoryComponent::HasItem(UHSItemInstance* Item, int32 Quantity) const
{
    return GetItemQuantity(Item) >= Quantity;
}

bool UHSInventoryComponent::HasSpaceForItem(UHSItemInstance* Item, int32 Quantity) const
{
    if (!Item || Quantity <= 0)
    {
        return false;
    }

    int32 RemainingQuantity = Quantity;

    // 기존 스택에 추가 가능한 공간 확인
    if (Item->CanStack())
    {
        for (const FHSInventorySlot& Slot : InventorySlots)
        {
            if (Slot.CanStack(Item))
            {
                int32 CanAdd = Slot.MaxStackSize - Slot.Quantity;
                RemainingQuantity -= CanAdd;
                if (RemainingQuantity <= 0)
                {
                    return true;
                }
            }
        }
    }

    // 빈 슬롯 확인
    int32 EmptySlots = GetEmptySlotCount();
    int32 SlotsNeeded = FMath::CeilToInt((float)RemainingQuantity / Item->GetMaxStackSize());

    return EmptySlots >= SlotsNeeded;
}

int32 UHSInventoryComponent::GetEmptySlotCount() const
{
    return EmptySlotCache.Num();
}

TArray<FHSInventorySlot> UHSInventoryComponent::GetFilteredItems(EHSInventoryFilter Filter) const
{
    TArray<FHSInventorySlot> FilteredItems;

    for (const FHSInventorySlot& Slot : InventorySlots)
    {
        if (!Slot.bIsEmpty && Slot.Item)
        {
            bool bShouldInclude = false;
            
            switch (Filter)
            {
                case EHSInventoryFilter::None:
                    bShouldInclude = true;
                    break;
                case EHSInventoryFilter::Weapons:
                    bShouldInclude = Slot.Item->GetItemType() == EHSItemType::Weapon;
                    break;
                case EHSInventoryFilter::Armor:
                    bShouldInclude = Slot.Item->GetItemType() == EHSItemType::Armor;
                    break;
                case EHSInventoryFilter::Consumables:
                    bShouldInclude = Slot.Item->GetItemType() == EHSItemType::Consumable;
                    break;
                case EHSInventoryFilter::Materials:
                    bShouldInclude = Slot.Item->GetItemType() == EHSItemType::Material;
                    break;
                case EHSInventoryFilter::Quest:
                    bShouldInclude = Slot.Item->GetItemType() == EHSItemType::Quest;
                    break;
            }

            if (bShouldInclude)
            {
                FilteredItems.Add(Slot);
            }
        }
    }

    return FilteredItems;
}

void UHSInventoryComponent::SortInventory()
{
    // 정렬 알고리즘: 아이템 타입별, 이름별 정렬
    TArray<FHSInventorySlot> NonEmptySlots;
    
    for (FHSInventorySlot& Slot : InventorySlots)
    {
        if (!Slot.bIsEmpty)
        {
            NonEmptySlots.Add(Slot);
            Slot.Clear();
        }
    }

    // 정렬
    NonEmptySlots.Sort([](const FHSInventorySlot& A, const FHSInventorySlot& B)
    {
        if (A.Item->GetItemType() != B.Item->GetItemType())
        {
            return static_cast<int32>(A.Item->GetItemType()) < static_cast<int32>(B.Item->GetItemType());
        }
        return A.Item->GetItemName().Compare(B.Item->GetItemName()) < 0;
    });

    // 정렬된 아이템들을 다시 배치
    for (int32 i = 0; i < NonEmptySlots.Num() && i < MaxSlots; ++i)
    {
        InventorySlots[i] = NonEmptySlots[i];
        BroadcastInventoryChanged(i, InventorySlots[i].Item);
    }

    UpdateItemCache();
    UpdateEmptySlotCache();
}

void UHSInventoryComponent::ClearInventory()
{
    for (int32 i = 0; i < InventorySlots.Num(); ++i)
    {
        if (!InventorySlots[i].bIsEmpty)
        {
            InventorySlots[i].Clear();
            BroadcastInventoryChanged(i, nullptr);
        }
    }

    UpdateItemCache();
    UpdateEmptySlotCache();
}

void UHSInventoryComponent::ResizeInventory(int32 NewSize)
{
    NewSize = FMath::Clamp(NewSize, 1, 100); // 최대 100슬롯

    if (NewSize > MaxSlots)
    {
        // 슬롯 확장
        int32 SlotsToAdd = NewSize - MaxSlots;
        for (int32 i = 0; i < SlotsToAdd; ++i)
        {
            InventorySlots.Add(FHSInventorySlot());
        }
    }
    else if (NewSize < MaxSlots)
    {
        // 슬롯 축소 - 아이템이 있는 슬롯은 보호
        TArray<FHSInventorySlot> ItemsToKeep;
        for (int32 i = NewSize; i < MaxSlots; ++i)
        {
            if (!InventorySlots[i].bIsEmpty)
            {
                ItemsToKeep.Add(InventorySlots[i]);
            }
        }

        InventorySlots.SetNum(NewSize);

        // 제거된 아이템들을 다시 추가 시도
        for (FHSInventorySlot& Item : ItemsToKeep)
        {
            int32 OutSlotIndex;
            if (!AddItem(Item.Item, Item.Quantity, OutSlotIndex))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSInventoryComponent::ResizeInventory - 아이템 손실: %s"), *Item.Item->GetItemName());
            }
        }
    }

    MaxSlots = NewSize;
    UpdateEmptySlotCache();
}

// 네트워크 함수들
void UHSInventoryComponent::ServerAddItem_Implementation(UHSItemInstance* Item, int32 Quantity)
{
    int32 OutSlotIndex;
    AddItem(Item, Quantity, OutSlotIndex);
}

void UHSInventoryComponent::ServerRemoveItem_Implementation(UHSItemInstance* Item, int32 Quantity)
{
    RemoveItem(Item, Quantity);
}

void UHSInventoryComponent::ServerMoveItem_Implementation(int32 FromSlot, int32 ToSlot)
{
    MoveItem(FromSlot, ToSlot);
}

void UHSInventoryComponent::MulticastInventoryUpdate_Implementation(int32 SlotIndex, UHSItemInstance* Item, int32 Quantity)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // 서버는 이미 최신 상태를 반영했으므로 추가 처리 불필요
        return;
    }

    FHSInventorySlot& Slot = InventorySlots[SlotIndex];

    if (!Item || Quantity <= 0)
    {
        Slot.Clear();
    }
    else
    {
        Slot.Item = Item;
        Slot.Quantity = Quantity;
        Slot.MaxStackSize = Item->GetMaxStackSize();
        Slot.bIsEmpty = false;
    }

    UpdateItemCache();
    UpdateEmptySlotCache();
    SyncFastArrayState();

    OnInventoryChanged.Broadcast(SlotIndex, Slot.bIsEmpty ? nullptr : Slot.Item);
}

void UHSInventoryComponent::OnRep_InventorySlots()
{
    // 클라이언트에서 인벤토리 변경 시 처리
    UpdateItemCache();
    UpdateEmptySlotCache();
    
    // UI 업데이트를 위한 델리게이트 호출
    for (int32 i = 0; i < InventorySlots.Num(); ++i)
    {
        OnInventoryChanged.Broadcast(i, InventorySlots[i].Item);
    }

    SyncFastArrayState();
}

// 내부 헬퍼 함수들
int32 UHSInventoryComponent::FindEmptySlot()
{
    if (EmptySlotCache.Num() == 0)
    {
        UpdateEmptySlotCache();
    }

    if (EmptySlotCache.Num() == 0)
    {
        return -1;
    }

    const int32 SlotIndex = EmptySlotCache[0];
    EmptySlotCache.RemoveAt(0, 1, false);
    return SlotIndex;
}

int32 UHSInventoryComponent::FindSlotWithItem(UHSItemInstance* Item) const
{
    for (int32 i = 0; i < InventorySlots.Num(); ++i)
    {
        if (InventorySlots[i].Item == Item && !InventorySlots[i].bIsEmpty)
        {
            return i;
        }
    }
    return -1;
}

int32 UHSInventoryComponent::FindSlotWithSpace(UHSItemInstance* Item) const
{
    for (int32 i = 0; i < InventorySlots.Num(); ++i)
    {
        if (InventorySlots[i].CanStack(Item))
        {
            return i;
        }
    }
    return -1;
}

void UHSInventoryComponent::UpdateItemCache()
{
    ItemQuantityCache.Empty();
    
    for (const FHSInventorySlot& Slot : InventorySlots)
    {
        if (!Slot.bIsEmpty && Slot.Item)
        {
            int32& TotalQuantity = ItemQuantityCache.FindOrAdd(Slot.Item);
            TotalQuantity += Slot.Quantity;
        }
    }
}

void UHSInventoryComponent::UpdateEmptySlotCache()
{
    EmptySlotCache.Empty();
    
    for (int32 i = 0; i < InventorySlots.Num(); ++i)
    {
        if (InventorySlots[i].bIsEmpty)
        {
            EmptySlotCache.Add(i);
        }
    }
}

bool UHSInventoryComponent::IsValidSlotIndex(int32 SlotIndex) const
{
    return SlotIndex >= 0 && SlotIndex < InventorySlots.Num();
}

void UHSInventoryComponent::BroadcastInventoryChanged(int32 SlotIndex, UHSItemInstance* Item)
{
    OnInventoryChanged.Broadcast(SlotIndex, Item);
    
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        const FHSInventorySlot& Slot = InventorySlots[SlotIndex];
        MulticastInventoryUpdate(SlotIndex, Item, Slot.Quantity);
        SyncFastArrayState();
    }
}

void UHSInventoryComponent::OptimizeNetworkUpdates()
{
    // 주기적으로 네트워크 최적화 수행
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastNetworkUpdate > NetworkUpdateInterval)
    {
        LastNetworkUpdate = CurrentTime;
        // 필요시 추가 최적화 로직
    }
}

void UHSInventoryComponent::CacheFrequentlyUsedData()
{
    UpdateItemCache();
    UpdateEmptySlotCache();
}

void UHSInventoryComponent::SyncFastArrayState()
{
    const bool bShouldMarkDirty = GetOwner() && GetOwner()->HasAuthority();
    ReplicatedFastSlots.SyncFromLegacyArray(InventorySlots, bShouldMarkDirty);
}
