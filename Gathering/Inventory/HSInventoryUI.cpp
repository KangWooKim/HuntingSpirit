#include "HSInventoryUI.h"
#include "../../Items/HSItemBase.h"
#include "Components/GridSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/DragDropOperation.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// HSInventorySlotWidget 구현
UHSInventorySlotWidget::UHSInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , SlotIndex(-1)
    , OwnerInventory(nullptr)
{
}

void UHSInventorySlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 버튼 이벤트 바인딩
    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UHSInventorySlotWidget::OnSlotButtonClicked);
    }
}

FReply UHSInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && !SlotData.bIsEmpty)
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UHSInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!SlotData.bIsEmpty && SlotData.Item)
    {
        // 드래그 앤 드롭 오퍼레이션 생성
        UDragDropOperation* DragOperation = NewObject<UDragDropOperation>();
        
        // 드래그되는 아이템 정보 설정
        DragOperation->Payload = SlotData.Item;
        DragOperation->DefaultDragVisual = this;
        
        // 시각적 피드백
        UpdateDragVisual(true);
        
        OutOperation = DragOperation;

        UE_LOG(LogTemp, Log, TEXT("HSInventorySlotWidget::NativeOnDragDetected - 슬롯 %d 드래그 시작"), SlotIndex);
    }
}

bool UHSInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (InOperation && InOperation->Payload)
    {
        UHSItemInstance* DroppedItem = Cast<UHSItemInstance>(InOperation->Payload);
        if (DroppedItem && OwnerInventory)
        {
            // 드롭 로직 처리는 인벤토리 컴포넌트에서 수행
            UE_LOG(LogTemp, Log, TEXT("HSInventorySlotWidget::NativeOnDrop - 슬롯 %d에 아이템 드롭"), SlotIndex);
            return true;
        }
    }

    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UHSInventorySlotWidget::SetSlotData(int32 InSlotIndex, const FHSInventorySlot& InSlotData, UHSInventoryComponent* InInventory)
{
    SlotIndex = InSlotIndex;
    SlotData = InSlotData;
    OwnerInventory = InInventory;
    
    UpdateSlotDisplay();
}

void UHSInventorySlotWidget::UpdateSlotDisplay()
{
    if (!SlotData.bIsEmpty && SlotData.Item)
    {
        // 아이템 아이콘 설정
        if (ItemIcon)
        {
            ItemIcon->SetBrushFromTexture(SlotData.Item->GetItemIcon());
            ItemIcon->SetVisibility(ESlateVisibility::Visible);
        }

        // 수량 텍스트 설정
        if (QuantityText)
        {
            if (SlotData.Quantity > 1)
            {
                QuantityText->SetText(FText::FromString(FString::FromInt(SlotData.Quantity)));
                QuantityText->SetVisibility(ESlateVisibility::Visible);
            }
            else
            {
                QuantityText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        // 슬롯 배경 색상 설정 (품질별)
        if (SlotBackground)
        {
            FLinearColor BackgroundColor = FLinearColor::White;
            switch (SlotData.Item->GetItemRarity())
            {
                case EHSItemRarity::Common:
                    BackgroundColor = FLinearColor::Gray;
                    break;
                case EHSItemRarity::Uncommon:
                    BackgroundColor = FLinearColor::Green;
                    break;
                case EHSItemRarity::Rare:
                    BackgroundColor = FLinearColor::Blue;
                    break;
                case EHSItemRarity::Epic:
                    BackgroundColor = FLinearColor(0.5f, 0.0f, 1.0f, 1.0f); // 보라색
                    break;
                case EHSItemRarity::Legendary:
                    BackgroundColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); // 주황색
                    break;
            }
            SlotBackground->SetColorAndOpacity(BackgroundColor);
        }
    }
    else
    {
        // 빈 슬롯 표시
        if (ItemIcon)
        {
            ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
        
        if (QuantityText)
        {
            QuantityText->SetVisibility(ESlateVisibility::Collapsed);
        }
        
        if (SlotBackground)
        {
            SlotBackground->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f));
        }
    }

    // 블루프린트 이벤트 호출
    OnSlotDataChanged();
}

void UHSInventorySlotWidget::SetHighlighted(bool bHighlighted)
{
    if (SlotBackground)
    {
        if (bHighlighted)
        {
            SlotBackground->SetColorAndOpacity(FLinearColor::Yellow);
        }
        else
        {
            UpdateSlotDisplay(); // 원래 색상으로 복원
        }
    }
}

void UHSInventorySlotWidget::OnSlotButtonClicked()
{
    UE_LOG(LogTemp, Log, TEXT("HSInventorySlotWidget::OnSlotButtonClicked - 슬롯 %d 클릭"), SlotIndex);
    
    // 부모 UI에 이벤트 전달 (슬롯 → 그리드 패널 → 인벤토리 UI)
    if (UWidget* GridParent = GetParent()) // GridPanel
    {
        if (UWidget* InventoryParent = GridParent->GetParent()) // HSInventoryUI
        {
            if (UHSInventoryUI* ParentUI = Cast<UHSInventoryUI>(InventoryParent))
            {
                ParentUI->HandleSlotClicked(SlotIndex);
            }
        }
    }
}

void UHSInventorySlotWidget::UpdateDragVisual(bool bIsDragging)
{
    if (bIsDragging)
    {
        SetRenderOpacity(0.5f);
    }
    else
    {
        SetRenderOpacity(1.0f);
    }
}

// HSInventoryUI 구현
UHSInventoryUI::UHSInventoryUI(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , GridColumns(6)
    , GridRows(6)
    , SlotSize(FVector2D(64.0f, 64.0f))
    , InventoryComponent(nullptr)
    , CurrentFilter(EHSInventoryFilter::None)
    , bNeedsRefresh(false)
    , LastUpdateTime(0.0f)
    , VisibleSlotStart(0)
    , VisibleSlotEnd(0)
    , DraggedSlotIndex(-1)
    , bIsDragging(false)
{
    // 기본 슬롯 위젯 클래스 설정
    SlotWidgetClass = UHSInventorySlotWidget::StaticClass();
}

void UHSInventoryUI::NativeConstruct()
{
    Super::NativeConstruct();

    // UI 컴포넌트 이벤트 바인딩
    if (SortButton)
    {
        SortButton->OnClicked.AddDynamic(this, &UHSInventoryUI::OnSortButtonClicked);
    }

    if (ClearButton)
    {
        ClearButton->OnClicked.AddDynamic(this, &UHSInventoryUI::OnClearButtonClicked);
    }

    if (FilterComboBox)
    {
        FilterComboBox->OnSelectionChanged.AddDynamic(this, &UHSInventoryUI::OnFilterChanged);
        
        // 필터 옵션 추가
        FilterComboBox->AddOption(FilterToString(EHSInventoryFilter::None));
        FilterComboBox->AddOption(FilterToString(EHSInventoryFilter::Weapons));
        FilterComboBox->AddOption(FilterToString(EHSInventoryFilter::Armor));
        FilterComboBox->AddOption(FilterToString(EHSInventoryFilter::Consumables));
        FilterComboBox->AddOption(FilterToString(EHSInventoryFilter::Materials));
        FilterComboBox->AddOption(FilterToString(EHSInventoryFilter::Quest));
        
        FilterComboBox->SetSelectedOption(FilterToString(EHSInventoryFilter::None));
    }

    // 슬롯 위젯 생성
    CreateSlotWidgets();
}

void UHSInventoryUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    float CurrentTime = GetWorld()->GetTimeSeconds();
    
    // 주기적 업데이트 (성능 최적화)
    if (CurrentTime - LastUpdateTime > UpdateInterval)
    {
        LastUpdateTime = CurrentTime;
        
        if (bNeedsRefresh)
        {
            RefreshInventoryDisplay();
            bNeedsRefresh = false;
        }

        // 가시 영역 최적화
        OptimizeVisibleSlots(MyGeometry);
    }
}

void UHSInventoryUI::InitializeInventoryUI(UHSInventoryComponent* InInventoryComponent)
{
    if (!InInventoryComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("HSInventoryUI::InitializeInventoryUI - 유효하지 않은 인벤토리 컴포넌트"));
        return;
    }

    InventoryComponent = InInventoryComponent;

    // 인벤토리 이벤트 바인딩
    InventoryComponent->OnInventoryChanged.AddDynamic(this, &UHSInventoryUI::OnInventoryChanged);
    InventoryComponent->OnItemAdded.AddDynamic(this, &UHSInventoryUI::OnItemAdded);
    InventoryComponent->OnItemRemoved.AddDynamic(this, &UHSInventoryUI::OnItemRemoved);
    InventoryComponent->OnInventoryFull.AddDynamic(this, &UHSInventoryUI::OnInventoryFull);

    // 초기 디스플레이 업데이트
    RefreshInventoryDisplay();
    UpdateSlotCount();
    UpdateWeight();

    UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::InitializeInventoryUI - 인벤토리 UI 초기화 완료"));
}

void UHSInventoryUI::RefreshInventoryDisplay()
{
    if (!InventoryComponent)
    {
        return;
    }

    // 필터 적용
    ApplyCurrentFilter();

    // 모든 슬롯 위젯 업데이트
    for (int32 i = 0; i < SlotWidgets.Num(); ++i)
    {
        UpdateSlotWidget(i);
    }

    // UI 정보 업데이트
    UpdateSlotCount();
    UpdateWeight();
}

void UHSInventoryUI::UpdateSlotCount()
{
    if (!InventoryComponent || !SlotCountText)
    {
        return;
    }

    int32 UsedSlots = InventoryComponent->MaxSlots - InventoryComponent->GetEmptySlotCount();
    FString SlotCountString = FString::Printf(TEXT("%d / %d"), UsedSlots, InventoryComponent->MaxSlots);
    SlotCountText->SetText(FText::FromString(SlotCountString));
}

void UHSInventoryUI::UpdateWeight()
{
    if (!InventoryComponent || !WeightText)
    {
        return;
    }

    // 총 무게 계산
    float TotalWeight = 0.0f;
    for (int32 i = 0; i < InventoryComponent->MaxSlots; ++i)
    {
        FHSInventorySlot InventorySlotData = InventoryComponent->GetSlot(i);
        if (!InventorySlotData.bIsEmpty && InventorySlotData.Item)
        {
            TotalWeight += InventorySlotData.Item->GetWeight() * InventorySlotData.Quantity;
        }
    }

    FString WeightString = FString::Printf(TEXT("무게: %.1f kg"), TotalWeight);
    WeightText->SetText(FText::FromString(WeightString));
}

void UHSInventoryUI::SetFilter(EHSInventoryFilter Filter)
{
    if (CurrentFilter != Filter)
    {
        CurrentFilter = Filter;
        ApplyCurrentFilter();
        RefreshInventoryDisplay();
    }
}

void UHSInventoryUI::ApplyCurrentFilter()
{
    if (!InventoryComponent)
    {
        return;
    }

    FilteredSlots = InventoryComponent->GetFilteredItems(CurrentFilter);
    CacheFilteredResults();
}

UHSInventorySlotWidget* UHSInventoryUI::GetSlotWidget(int32 SlotIndex)
{
    if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num())
    {
        return SlotWidgets[SlotIndex];
    }
    return nullptr;
}

void UHSInventoryUI::HighlightSlot(int32 SlotIndex, bool bHighlight)
{
    UHSInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
    if (SlotWidget)
    {
        SlotWidget->SetHighlighted(bHighlight);
    }
}

void UHSInventoryUI::ClearAllHighlights()
{
    for (UHSInventorySlotWidget* SlotWidget : SlotWidgets)
    {
        if (SlotWidget)
        {
            SlotWidget->SetHighlighted(false);
        }
    }
}

TArray<int32> UHSInventoryUI::SearchItems(const FString& SearchTerm)
{
    TArray<int32> Results;
    
    if (!InventoryComponent || SearchTerm.IsEmpty())
    {
        return Results;
    }

    for (int32 i = 0; i < InventoryComponent->MaxSlots; ++i)
    {
        FHSInventorySlot InventorySlotData = InventoryComponent->GetSlot(i);
        if (!InventorySlotData.bIsEmpty && InventorySlotData.Item)
        {
            if (InventorySlotData.Item->GetItemName().Contains(SearchTerm) || 
                InventorySlotData.Item->GetItemDescription().Contains(SearchTerm))
            {
                Results.Add(i);
            }
        }
    }

    return Results;
}

void UHSInventoryUI::HighlightSearchResults(const TArray<int32>& ResultSlots)
{
    ClearAllHighlights();
    
    for (int32 SlotIndex : ResultSlots)
    {
        HighlightSlot(SlotIndex, true);
    }
}

// UI 이벤트 처리 함수들
void UHSInventoryUI::OnSortButtonClicked()
{
    if (InventoryComponent)
    {
        InventoryComponent->SortInventory();
        UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::OnSortButtonClicked - 인벤토리 정렬"));
    }
}

void UHSInventoryUI::OnClearButtonClicked()
{
    if (InventoryComponent)
    {
        // 확인 다이얼로그 후 실행하는 것이 좋음
        InventoryComponent->ClearInventory();
        UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::OnClearButtonClicked - 인벤토리 초기화"));
    }
}

void UHSInventoryUI::OnFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    EHSInventoryFilter NewFilter = StringToFilter(SelectedItem);
    SetFilter(NewFilter);
    UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::OnFilterChanged - 필터 변경: %s"), *SelectedItem);
}

// 인벤토리 이벤트 콜백들
void UHSInventoryUI::OnInventoryChanged(int32 SlotIndex, UHSItemInstance* Item)
{
    UpdateSlotWidget(SlotIndex);
    UpdateSlotCount();
    UpdateWeight();
}

void UHSInventoryUI::OnItemAdded(UHSItemInstance* Item, int32 Quantity, int32 SlotIndex)
{
    if (Item)
    {
        PlaySlotAddAnimation(SlotIndex);
        UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::OnItemAdded - %s 추가됨 (수량: %d, 슬롯: %d)"), 
               *Item->GetItemName(), Quantity, SlotIndex);
    }
}

void UHSInventoryUI::OnItemRemoved(UHSItemInstance* Item, int32 Quantity, int32 SlotIndex)
{
    if (Item)
    {
        PlaySlotRemoveAnimation(SlotIndex);
        UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::OnItemRemoved - %s 제거됨 (수량: %d, 슬롯: %d)"), 
               *Item->GetItemName(), Quantity, SlotIndex);
    }
}

void UHSInventoryUI::OnInventoryFull(UHSItemInstance* FailedItem)
{
    PlayInventoryFullAnimation();
    if (FailedItem)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSInventoryUI::OnInventoryFull - 인벤토리 가득참: %s 추가 실패"), 
               *FailedItem->GetItemName());
    }
}

// 슬롯 이벤트 처리
void UHSInventoryUI::HandleSlotClicked(int32 SlotIndex)
{
    OnSlotClicked.Broadcast(SlotIndex);
    UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::HandleSlotClicked - 슬롯 %d 클릭됨"), SlotIndex);
}

void UHSInventoryUI::HandleSlotDragStarted(int32 SlotIndex, UHSItemInstance* Item)
{
    DraggedSlotIndex = SlotIndex;
    bIsDragging = true;
    OnSlotDragStarted.Broadcast(SlotIndex, Item);
    UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::HandleSlotDragStarted - 슬롯 %d 드래그 시작"), SlotIndex);
}

void UHSInventoryUI::HandleSlotDropped(int32 FromSlot, int32 ToSlot)
{
    if (InventoryComponent && FromSlot != ToSlot)
    {
        InventoryComponent->MoveItem(FromSlot, ToSlot);
        OnSlotDropped.Broadcast(FromSlot, ToSlot);
        UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::HandleSlotDropped - 슬롯 %d에서 %d로 이동"), FromSlot, ToSlot);
    }
    
    bIsDragging = false;
    DraggedSlotIndex = -1;
}

// 내부 헬퍼 함수들
void UHSInventoryUI::CreateSlotWidgets()
{
    if (!InventoryGrid || !SlotWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("HSInventoryUI::CreateSlotWidgets - 그리드 패널 또는 슬롯 위젯 클래스가 없음"));
        return;
    }

    // 기존 위젯들 제거
    InventoryGrid->ClearChildren();
    SlotWidgets.Empty();

    // 새 슬롯 위젯들 생성
    int32 TotalSlots = GridColumns * GridRows;
    SlotWidgets.Reserve(TotalSlots);

    for (int32 i = 0; i < TotalSlots; ++i)
    {
        UHSInventorySlotWidget* SlotWidget = CreateWidget<UHSInventorySlotWidget>(this, SlotWidgetClass);
        if (SlotWidget)
        {
            // 그리드에 추가
            int32 Row = i / GridColumns;
            int32 Column = i % GridColumns;
            
            UGridSlot* GridSlotPtr = InventoryGrid->AddChildToGrid(SlotWidget, Row, Column);
            if (GridSlotPtr)
            {
                // UE5에서 SetSize() 메서드 제거됨 - SetHorizontalAlignment와 SetVerticalAlignment로 대체
                GridSlotPtr->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
                GridSlotPtr->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
                GridSlotPtr->SetPadding(FMargin(2.0f));
            }

            SlotWidgets.Add(SlotWidget);
            
            // 초기 슬롯 데이터 설정
            if (InventoryComponent)
            {
                FHSInventorySlot InventorySlotData = InventoryComponent->GetSlot(i);
                SlotWidget->SetSlotData(i, InventorySlotData, InventoryComponent);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSInventoryUI::CreateSlotWidgets - %d개 슬롯 위젯 생성 완료"), TotalSlots);
}

void UHSInventoryUI::UpdateSlotWidget(int32 SlotIndex)
{
    if (!InventoryComponent || SlotIndex < 0 || SlotIndex >= SlotWidgets.Num())
    {
        return;
    }

    UHSInventorySlotWidget* SlotWidget = SlotWidgets[SlotIndex];
    if (SlotWidget)
    {
        FHSInventorySlot InventorySlotData = InventoryComponent->GetSlot(SlotIndex);
        SlotWidget->SetSlotData(SlotIndex, InventorySlotData, InventoryComponent);
    }
}

void UHSInventoryUI::UpdateVisibleSlots()
{
    // 가시 영역에 있는 슬롯만 업데이트 (성능 최적화)
    for (int32 i = VisibleSlotStart; i < VisibleSlotEnd && i < SlotWidgets.Num(); ++i)
    {
        if (ShouldUpdateSlot(i))
        {
            UpdateSlotWidget(i);
        }
    }
}

EHSInventoryFilter UHSInventoryUI::StringToFilter(const FString& FilterString)
{
    if (FilterString == TEXT("무기")) return EHSInventoryFilter::Weapons;
    if (FilterString == TEXT("방어구")) return EHSInventoryFilter::Armor;
    if (FilterString == TEXT("소모품")) return EHSInventoryFilter::Consumables;
    if (FilterString == TEXT("재료")) return EHSInventoryFilter::Materials;
    if (FilterString == TEXT("퀘스트 아이템")) return EHSInventoryFilter::Quest;
    return EHSInventoryFilter::None;
}

FString UHSInventoryUI::FilterToString(EHSInventoryFilter Filter)
{
    switch (Filter)
    {
        case EHSInventoryFilter::Weapons: return TEXT("무기");
        case EHSInventoryFilter::Armor: return TEXT("방어구");
        case EHSInventoryFilter::Consumables: return TEXT("소모품");
        case EHSInventoryFilter::Materials: return TEXT("재료");
        case EHSInventoryFilter::Quest: return TEXT("퀘스트 아이템");
        default: return TEXT("모든 아이템");
    }
}

// 성능 최적화 함수들
void UHSInventoryUI::OptimizeVisibleSlots(const FGeometry& MyGeometry)
{
    // 현재 가시 영역 계산
    FVector2D ViewportSize = MyGeometry.GetLocalSize();
    
    // 간단한 가시성 계산 (실제로는 더 복잡한 로직 필요)
    VisibleSlotStart = 0;
    VisibleSlotEnd = FMath::Min(MaxVisibleSlots, SlotWidgets.Num());
    
    UpdateVisibleSlots();
}

void UHSInventoryUI::CacheFilteredResults()
{
    // 필터링된 결과를 캐시하여 성능 향상
    // 실제 구현에서는 더 복잡한 캐싱 로직 필요
}

bool UHSInventoryUI::ShouldUpdateSlot(int32 SlotIndex) const
{
    // 슬롯 업데이트 필요성 판단
    return SlotIndex >= 0 && SlotIndex < SlotWidgets.Num();
}
