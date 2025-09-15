// HSGameHUD.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HSGameHUD.generated.h"

// Forward declarations
class UHSHealthBarWidget;
class UHSStaminaBarWidget;
class UHSDamageNumberWidget;
class UUserWidget;

/**
 * HuntingSpirit 게임의 메인 HUD 클래스
 * 체력바, 스태미너바, 데미지 표시 등 인게임 UI를 관리합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API AHSGameHUD : public AHUD
{
    GENERATED_BODY()

public:
    AHSGameHUD();

protected:
    // HUD 초기화
    virtual void BeginPlay() override;
    
    // HUD 그리기 (디버그 정보 등)
    virtual void DrawHUD() override;

public:
    // 체력바 업데이트
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateHealthBar(float CurrentHealth, float MaxHealth);
    
    // 스태미너바 업데이트
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateStaminaBar(float CurrentStamina, float MaxStamina);
    
    // 데미지 표시
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void ShowDamageNumber(float Damage, FVector WorldLocation, bool bIsCritical = false);
    
    // HUD 표시/숨기기 (AHUD 오버라이드)
    virtual void ShowHUD() override;
    
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void HideHUD();

    // 위젯 가시성 토글
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void ToggleHUDVisibility();

protected:
    // 위젯 클래스 레퍼런스
    UPROPERTY(EditDefaultsOnly, Category = "Widgets")
    TSubclassOf<UHSHealthBarWidget> HealthBarWidgetClass;
    
    UPROPERTY(EditDefaultsOnly, Category = "Widgets")
    TSubclassOf<UHSStaminaBarWidget> StaminaBarWidgetClass;
    
    UPROPERTY(EditDefaultsOnly, Category = "Widgets")
    TSubclassOf<UHSDamageNumberWidget> DamageNumberWidgetClass;
    
    // 위젯 인스턴스
    UPROPERTY()
    UHSHealthBarWidget* HealthBarWidget;
    
    UPROPERTY()
    UHSStaminaBarWidget* StaminaBarWidget;
    
    // 데미지 넘버 풀 (최적화)
    UPROPERTY()
    TArray<UHSDamageNumberWidget*> DamageNumberPool;
    
    // 활성화된 데미지 넘버 위젯들
    UPROPERTY()
    TArray<UHSDamageNumberWidget*> ActiveDamageNumbers;

private:
    // 위젯 생성 및 초기화
    void CreateWidgets();
    
    // 데미지 넘버 풀 초기화
    void InitializeDamageNumberPool();
    
    // 풀에서 데미지 넘버 위젯 가져오기
    UHSDamageNumberWidget* GetDamageNumberFromPool();
    
    // 데미지 넘버 위젯을 풀로 반환
    void ReturnDamageNumberToPool(UHSDamageNumberWidget* DamageNumber);

    // HUD 가시성 상태
    bool bIsHUDVisible;
    
    // 데미지 넘버 풀 크기
    static constexpr int32 DamageNumberPoolSize = 20;
    
    // 디버그 표시 여부는 부모 클래스 AHUD의 bShowDebugInfo 사용
};
