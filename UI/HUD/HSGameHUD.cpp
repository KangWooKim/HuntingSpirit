// HSGameHUD.cpp
#include "HSGameHUD.h"
#include "HuntingSpirit/UI/Widgets/HSHealthBarWidget.h"
#include "HuntingSpirit/UI/Widgets/HSStaminaBarWidget.h"
#include "HuntingSpirit/UI/Widgets/HSDamageNumberWidget.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

AHSGameHUD::AHSGameHUD()
{
    // HUD 가시성 기본값 설정
    bIsHUDVisible = true;
    bShowDebugInfo = false;
}

void AHSGameHUD::BeginPlay()
{
    Super::BeginPlay();
    
    // 위젯 생성 및 초기화
    CreateWidgets();
    
    // 데미지 넘버 풀 초기화
    InitializeDamageNumberPool();
}

void AHSGameHUD::DrawHUD()
{
    Super::DrawHUD();
    
    // 디버그 정보 표시
    if (bShowDebugInfo)
    {
        // FPS 표시
        const float FPS = 1.0f / GetWorld()->GetDeltaSeconds();
        DrawText(FString::Printf(TEXT("FPS: %.1f"), FPS), FColor::Yellow, 10, 10);
        
        // 활성 데미지 넘버 개수 표시
        DrawText(FString::Printf(TEXT("Active Damage Numbers: %d"), ActiveDamageNumbers.Num()), 
                FColor::Yellow, 10, 30);
        
        // 메모리 풀 상태 표시
        const int32 PooledCount = DamageNumberPool.Num();
        DrawText(FString::Printf(TEXT("Pooled Damage Numbers: %d/%d"), PooledCount, DamageNumberPoolSize), 
                FColor::Yellow, 10, 50);
    }
}

void AHSGameHUD::UpdateHealthBar(float CurrentHealth, float MaxHealth)
{
    if (HealthBarWidget)
    {
        HealthBarWidget->UpdateHealthBar(CurrentHealth, MaxHealth);
    }
}

void AHSGameHUD::UpdateStaminaBar(float CurrentStamina, float MaxStamina)
{
    if (StaminaBarWidget)
    {
        StaminaBarWidget->UpdateStaminaBar(CurrentStamina, MaxStamina);
    }
}

void AHSGameHUD::ShowDamageNumber(float Damage, FVector WorldLocation, bool bIsCritical)
{
    // 데미지 넘버 위젯을 풀에서 가져오기
    UHSDamageNumberWidget* DamageNumberWidget = GetDamageNumberFromPool();
    
    if (DamageNumberWidget)
    {
        // 월드 좌표를 스크린 좌표로 변환
        FVector2D ScreenLocation;
        if (UGameplayStatics::ProjectWorldToScreen(GetOwningPlayerController(), WorldLocation, ScreenLocation))
        {
            // 데미지 넘버 설정 및 애니메이션 시작
            DamageNumberWidget->SetDamageNumber(Damage, bIsCritical);
            DamageNumberWidget->SetPositionInViewport(ScreenLocation);
            DamageNumberWidget->PlayDamageAnimation();
            
            // 활성 리스트에 추가
            ActiveDamageNumbers.Add(DamageNumberWidget);
            
            // 애니메이션 완료 후 풀로 반환하는 콜백 설정
            FTimerHandle TimerHandle;
            FTimerDelegate TimerDelegate;
            TimerDelegate.BindLambda([this, DamageNumberWidget]()
            {
                ReturnDamageNumberToPool(DamageNumberWidget);
            });
            
            // 1.5초 후 풀로 반환
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 1.5f, false);
        }
    }
}

void AHSGameHUD::ShowHUD()
{
    Super::ShowHUD();
    bIsHUDVisible = true;
    
    if (HealthBarWidget)
    {
        HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
    }
    
    if (StaminaBarWidget)
    {
        StaminaBarWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void AHSGameHUD::HideHUD()
{
    bIsHUDVisible = false;
    
    if (HealthBarWidget)
    {
        HealthBarWidget->SetVisibility(ESlateVisibility::Hidden);
    }
    
    if (StaminaBarWidget)
    {
        StaminaBarWidget->SetVisibility(ESlateVisibility::Hidden);
    }
}

void AHSGameHUD::ToggleHUDVisibility()
{
    if (bIsHUDVisible)
    {
        HideHUD();
    }
    else
    {
        ShowHUD();
    }
}

void AHSGameHUD::CreateWidgets()
{
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController)
    {
        return;
    }
    
    // 체력바 위젯 생성
    if (HealthBarWidgetClass)
    {
        HealthBarWidget = CreateWidget<UHSHealthBarWidget>(PlayerController, HealthBarWidgetClass);
        if (HealthBarWidget)
        {
            HealthBarWidget->AddToViewport(0);
        }
    }
    
    // 스태미너바 위젯 생성
    if (StaminaBarWidgetClass)
    {
        StaminaBarWidget = CreateWidget<UHSStaminaBarWidget>(PlayerController, StaminaBarWidgetClass);
        if (StaminaBarWidget)
        {
            StaminaBarWidget->AddToViewport(0);
        }
    }
}

void AHSGameHUD::InitializeDamageNumberPool()
{
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !DamageNumberWidgetClass)
    {
        return;
    }
    
    // 풀 크기만큼 데미지 넘버 위젯 미리 생성
    for (int32 i = 0; i < DamageNumberPoolSize; ++i)
    {
        UHSDamageNumberWidget* DamageNumberWidget = CreateWidget<UHSDamageNumberWidget>(
            PlayerController, DamageNumberWidgetClass);
        
        if (DamageNumberWidget)
        {
            DamageNumberWidget->AddToViewport(10); // 데미지 넘버는 다른 UI보다 위에 표시
            DamageNumberWidget->SetVisibility(ESlateVisibility::Hidden);
            DamageNumberPool.Add(DamageNumberWidget);
        }
    }
}

UHSDamageNumberWidget* AHSGameHUD::GetDamageNumberFromPool()
{
    if (DamageNumberPool.Num() > 0)
    {
        UHSDamageNumberWidget* DamageNumber = DamageNumberPool.Pop();
        DamageNumber->SetVisibility(ESlateVisibility::Visible);
        return DamageNumber;
    }
    
    // 풀이 비어있으면 새로 생성 (풀 크기 초과 상황)
    APlayerController* PlayerController = GetOwningPlayerController();
    if (PlayerController && DamageNumberWidgetClass)
    {
        UHSDamageNumberWidget* NewDamageNumber = CreateWidget<UHSDamageNumberWidget>(
            PlayerController, DamageNumberWidgetClass);
        
        if (NewDamageNumber)
        {
            NewDamageNumber->AddToViewport(10);
            UE_LOG(LogTemp, Warning, TEXT("Damage number pool exhausted, creating new widget"));
            return NewDamageNumber;
        }
    }
    
    return nullptr;
}

void AHSGameHUD::ReturnDamageNumberToPool(UHSDamageNumberWidget* DamageNumber)
{
    if (DamageNumber)
    {
        // 활성 리스트에서 제거
        ActiveDamageNumbers.Remove(DamageNumber);
        
        // 위젯 초기화 및 숨기기
        DamageNumber->SetVisibility(ESlateVisibility::Hidden);
        
        // 풀에 반환
        if (DamageNumberPool.Num() < DamageNumberPoolSize)
        {
            DamageNumberPool.Add(DamageNumber);
        }
        else
        {
            // 풀이 가득 찬 경우 위젯 제거
            DamageNumber->RemoveFromParent();
        }
    }
}
