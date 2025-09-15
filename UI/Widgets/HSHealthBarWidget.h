// HSHealthBarWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSHealthBarWidget.generated.h"

// Forward declarations
class UProgressBar;
class UTextBlock;
class UBorder;

/**
 * 플레이어 체력을 표시하는 위젯
 * 체력바와 체력 수치를 표시합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API UHSHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 위젯 초기화
    virtual void NativeConstruct() override;
    
    // 체력바 업데이트
    UFUNCTION(BlueprintCallable, Category = "Health")
    void UpdateHealthBar(float CurrentHealth, float MaxHealth);
    
    // 체력 변화 애니메이션
    UFUNCTION(BlueprintCallable, Category = "Health")
    void PlayHealthChangeAnimation(bool bIsHealing);
    
    // 낮은 체력 경고 효과
    UFUNCTION(BlueprintCallable, Category = "Health")
    void SetLowHealthWarning(bool bEnable);

protected:
    // UI 컴포넌트 바인딩
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HealthBar;
    
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HealthBarBackground;
    
    UPROPERTY(meta = (BindWidget))
    UTextBlock* HealthText;
    
    UPROPERTY(meta = (BindWidget))
    UBorder* HealthBarBorder;
    
    // 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    UWidgetAnimation* HealthChangeAnim;
    
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    UWidgetAnimation* LowHealthPulseAnim;
    
    // 체력바 색상 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor NormalHealthColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor LowHealthColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor CriticalHealthColor;
    
    // 체력 임계값
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float LowHealthThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float CriticalHealthThreshold;

private:
    // 체력 비율에 따른 색상 업데이트
    void UpdateHealthBarColor(float HealthPercent);
    
    // 지연된 체력바 업데이트 (데미지 표시 효과)
    void UpdateDelayedHealthBar();
    
    // 타이머 핸들
    FTimerHandle DelayedHealthBarTimerHandle;
    
    // 현재 체력 값 캐싱
    float CachedCurrentHealth;
    float CachedMaxHealth;
    
    // 지연된 체력바 값
    float DelayedHealthPercent;
    
    // 부드러운 체력바 업데이트를 위한 보간 속도
    UPROPERTY(EditAnywhere, Category = "Animation")
    float HealthBarInterpSpeed;
};
