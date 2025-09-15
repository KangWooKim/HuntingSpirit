// HSStaminaBarWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSStaminaBarWidget.generated.h"

// Forward declarations
class UProgressBar;
class UTextBlock;
class UCanvasPanel;

/**
 * 플레이어 스태미너를 표시하는 위젯
 * 스태미너바와 수치를 표시하며, 스태미너 소진 시 경고 효과를 제공합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API UHSStaminaBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 위젯 초기화
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    // 스태미너바 업데이트
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void UpdateStaminaBar(float CurrentStamina, float MaxStamina);
    
    // 스태미너 소진 경고
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void PlayExhaustedWarning();
    
    // 스태미너 회복 중 표시
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void SetRegenerating(bool bIsRegenerating);

protected:
    // UI 컴포넌트
    UPROPERTY(meta = (BindWidget))
    UProgressBar* StaminaBar;
    
    UPROPERTY(meta = (BindWidget))
    UProgressBar* StaminaBarGhost;
    
    UPROPERTY(meta = (BindWidget))
    UTextBlock* StaminaText;
    
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* StaminaContainer;
    
    // 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    UWidgetAnimation* ExhaustedAnim;
    
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    UWidgetAnimation* RegeneratingAnim;
    
    // 스태미너바 색상
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor NormalStaminaColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor LowStaminaColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor ExhaustedStaminaColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor RegeneratingStaminaColor;
    
    // 임계값
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
    float LowStaminaThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
    float ExhaustedThreshold;

private:
    // 스태미너 비율에 따른 색상 업데이트
    void UpdateStaminaBarColor(float StaminaPercent);
    
    // 고스트 바 업데이트 (소비한 스태미너 표시)
    void UpdateGhostBar(float DeltaTime);
    
    // 자동 숨기기 타이머
    void StartAutoHideTimer();
    void HideStaminaBar();
    
    // 상태 변수
    float CurrentStaminaPercent;
    float GhostStaminaPercent;
    float LastStaminaChangeTime;
    bool bIsRegenerating;
    bool bIsExhausted;
    
    // 타이머 핸들
    FTimerHandle AutoHideTimerHandle;
    
    // 설정 값
    UPROPERTY(EditAnywhere, Category = "Behavior")
    float AutoHideDelay;
    
    UPROPERTY(EditAnywhere, Category = "Behavior")
    bool bAutoHideWhenFull;
    
    UPROPERTY(EditAnywhere, Category = "Animation")
    float GhostBarSpeed;
};
