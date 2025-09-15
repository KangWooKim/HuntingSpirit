#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/GridPanel.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
#include "Engine/Texture2D.h"
#include "HSInventoryComponent.h"
#include "../../Items/HSItemBase.h"
#include "HSInventoryUI.generated.h"

class UHSInventorySlotWidget;
class UHSItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotDragStarted, int32, SlotIndex, UHSItemInstance*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotDropped, int32, FromSlot, int32, ToSlot);

/**
 * 인벤토리 슬롯 위젯
 * 개별 아이템 슬롯을 나타내는 UI 컴포넌트
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UHSInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    // UI 컴포넌트들
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ItemIcon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> QuantityText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> SlotBackground;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> SlotButton;

    // 슬롯 데이터
    UPROPERTY(BlueprintReadOnly, Category = "Slot")
    int32 SlotIndex;

    UPROPERTY(BlueprintReadOnly, Category = "Slot")
    FHSInventorySlot SlotData;

    UPROPERTY(BlueprintReadOnly, Category = "Slot")
    TObjectPtr<UHSInventoryComponent> OwnerInventory;

public:
    // 슬롯 설정 함수
    UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
    void SetSlotData(int32 InSlotIndex, const FHSInventorySlot& InSlotData, UHSInventoryComponent* InInventory);

    UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
    void UpdateSlotDisplay();

    UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
    void SetHighlighted(bool bHighlighted);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory Slot")
    void OnSlotDataChanged();

protected:
    UFUNCTION()
    void OnSlotButtonClicked();

    // 드래그 앤 드롭 시각적 피드백
    void UpdateDragVisual(bool bIsDragging);
};

/**
 * 인벤토리 UI 메인 위젯
 * 전체 인벤토리 인터페이스를 관리하는 UI 클래스
 * 현업 최적화: 가상화, 지연 로딩, 캐싱 시스템 적용
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSInventoryUI : public UUserWidget
{
    GENERATED_BODY()

public:
    UHSInventoryUI(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // UI 컴포넌트들
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UGridPanel> InventoryGrid;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> InventoryTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> SortButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> ClearButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> FilterComboBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> ItemListScrollBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> SlotCountText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WeightText;

    // 인벤토리 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
    int32 GridColumns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
    int32 GridRows;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
    TSubclassOf<UHSInventorySlotWidget> SlotWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
    FVector2D SlotSize;

    // 참조
    UPROPERTY(BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UHSInventoryComponent> InventoryComponent;

    // 슬롯 위젯 캐시 (성능 최적화)
    UPROPERTY()
    TArray<TObjectPtr<UHSInventorySlotWidget>> SlotWidgets;

    // 필터링 시스템
    UPROPERTY(BlueprintReadOnly, Category = "Filtering")
    EHSInventoryFilter CurrentFilter;

    UPROPERTY()
    TArray<FHSInventorySlot> FilteredSlots;

    // 성능 최적화 변수
    UPROPERTY()
    bool bNeedsRefresh;

    UPROPERTY()
    float LastUpdateTime;

    UPROPERTY()
    int32 VisibleSlotStart;

    UPROPERTY()
    int32 VisibleSlotEnd;

public:
    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Inventory UI Events")
    FOnSlotClicked OnSlotClicked;

    UPROPERTY(BlueprintAssignable, Category = "Inventory UI Events")
    FOnSlotDragStarted OnSlotDragStarted;

    UPROPERTY(BlueprintAssignable, Category = "Inventory UI Events")
    FOnSlotDropped OnSlotDropped;

    // 초기화 및 설정
    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void InitializeInventoryUI(UHSInventoryComponent* InInventoryComponent);

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void RefreshInventoryDisplay();

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void UpdateSlotCount();

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void UpdateWeight();

    // 필터링 기능
    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void SetFilter(EHSInventoryFilter Filter);

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void ApplyCurrentFilter();

    // 슬롯 관리
    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    UHSInventorySlotWidget* GetSlotWidget(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void HighlightSlot(int32 SlotIndex, bool bHighlight);

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void ClearAllHighlights();

    // 애니메이션 및 시각 효과
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory UI")
    void PlaySlotAddAnimation(int32 SlotIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory UI")
    void PlaySlotRemoveAnimation(int32 SlotIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory UI")
    void PlayInventoryFullAnimation();

    // 검색 기능
    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    TArray<int32> SearchItems(const FString& SearchTerm);

    UFUNCTION(BlueprintCallable, Category = "Inventory UI")
    void HighlightSearchResults(const TArray<int32>& ResultSlots);

    // 슬롯 이벤트 처리 (SlotWidget에서 접근을 위해 public으로 변경)
    UFUNCTION()
    void HandleSlotClicked(int32 SlotIndex);

protected:
    // UI 이벤트 처리
    UFUNCTION()
    void OnSortButtonClicked();

    UFUNCTION()
    void OnClearButtonClicked();

    UFUNCTION()
    void OnFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    // 인벤토리 이벤트 콜백
    UFUNCTION()
    void OnInventoryChanged(int32 SlotIndex, UHSItemInstance* Item);

    UFUNCTION()
    void OnItemAdded(UHSItemInstance* Item, int32 Quantity, int32 SlotIndex);

    UFUNCTION()
    void OnItemRemoved(UHSItemInstance* Item, int32 Quantity, int32 SlotIndex);

    UFUNCTION()
    void OnInventoryFull(UHSItemInstance* FailedItem);

    UFUNCTION()
    void HandleSlotDragStarted(int32 SlotIndex, UHSItemInstance* Item);

    UFUNCTION()
    void HandleSlotDropped(int32 FromSlot, int32 ToSlot);

    // 내부 헬퍼 함수
    void CreateSlotWidgets();
    void UpdateSlotWidget(int32 SlotIndex);
    void UpdateVisibleSlots();
    EHSInventoryFilter StringToFilter(const FString& FilterString);
    FString FilterToString(EHSInventoryFilter Filter);

    // 성능 최적화 함수
    void OptimizeVisibleSlots(const FGeometry& MyGeometry);
    void CacheFilteredResults();
    bool ShouldUpdateSlot(int32 SlotIndex) const;

private:
    // 성능 최적화 상수
    static constexpr float UpdateInterval = 0.1f;
    static constexpr int32 MaxVisibleSlots = 50;
    
    // 드래그 앤 드롭 관련
    int32 DraggedSlotIndex;
    bool bIsDragging;
};
