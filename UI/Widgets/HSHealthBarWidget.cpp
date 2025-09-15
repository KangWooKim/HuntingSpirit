// HSHealthBarWidget.cpp
#include "HSHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"

void UHSHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 기본 색상 설정
    NormalHealthColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);     // 녹색
    LowHealthColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);        // 노란색
    CriticalHealthColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);    // 빨간색
    
    // 임계값 설정
    LowHealthThreshold = 0.5f;      // 50% 이하
    CriticalHealthThreshold = 0.25f; // 25% 이하
    
    // 보간 속도 설정
    HealthBarInterpSpeed = 2.0f;
    
    // 초기 값 설정
    DelayedHealthPercent = 1.0f;
    
    // 체력바 초기화
    if (HealthBar)
    {
        HealthBar->SetPercent(1.0f);
        HealthBar->SetFillColorAndOpacity(NormalHealthColor);
    }
    
    if (HealthBarBackground)
    {
        HealthBarBackground->SetPercent(1.0f);
        HealthBarBackground->SetFillColorAndOpacity(FLinearColor(0.5f, 0.0f, 0.0f, 0.8f));
    }
}

void UHSHealthBarWidget::UpdateHealthBar(float CurrentHealth, float MaxHealth)
{
    // 체력 값 캐싱
    CachedCurrentHealth = CurrentHealth;
    CachedMaxHealth = MaxHealth;
    
    // 체력 비율 계산
    const float HealthPercent = MaxHealth > 0 ? CurrentHealth / MaxHealth : 0.0f;
    
    // 즉시 체력바 업데이트
    if (HealthBar)
    {
        HealthBar->SetPercent(HealthPercent);
    }
    
    // 체력 텍스트 업데이트
    if (HealthText)
    {
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, MaxHealth)));
    }
    
    // 색상 업데이트
    UpdateHealthBarColor(HealthPercent);
    
    // 낮은 체력 경고 설정
    SetLowHealthWarning(HealthPercent <= CriticalHealthThreshold);
    
    // 지연된 체력바 업데이트 타이머 설정 (데미지 표시 효과)
    GetWorld()->GetTimerManager().ClearTimer(DelayedHealthBarTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(
        DelayedHealthBarTimerHandle,
        this,
        &UHSHealthBarWidget::UpdateDelayedHealthBar,
        0.5f,
        false
    );
    
    // 체력 변화 애니메이션 재생
    bool bIsHealing = HealthPercent > DelayedHealthPercent;
    PlayHealthChangeAnimation(bIsHealing);
}

void UHSHealthBarWidget::PlayHealthChangeAnimation(bool bIsHealing)
{
    if (HealthChangeAnim)
    {
        // 애니메이션 방향 설정 (회복이면 정방향, 데미지면 역방향)
        float PlaybackSpeed = bIsHealing ? 1.0f : -1.0f;
        PlayAnimation(HealthChangeAnim, 0.0f, 1, EUMGSequencePlayMode::Forward, PlaybackSpeed);
    }
}

void UHSHealthBarWidget::SetLowHealthWarning(bool bEnable)
{
    if (bEnable && LowHealthPulseAnim)
    {
        // 낮은 체력 펄스 애니메이션 반복 재생
        PlayAnimation(LowHealthPulseAnim, 0.0f, 0, EUMGSequencePlayMode::PingPong);
    }
    else if (!bEnable && IsAnimationPlaying(LowHealthPulseAnim))
    {
        // 애니메이션 중지
        StopAnimation(LowHealthPulseAnim);
    }
}

void UHSHealthBarWidget::UpdateHealthBarColor(float HealthPercent)
{
    if (!HealthBar)
    {
        return;
    }
    
    FLinearColor NewColor;
    
    if (HealthPercent <= CriticalHealthThreshold)
    {
        // 치명적 체력 - 빨간색
        NewColor = CriticalHealthColor;
    }
    else if (HealthPercent <= LowHealthThreshold)
    {
        // 낮은 체력 - 노란색으로 보간
        float Alpha = (HealthPercent - CriticalHealthThreshold) / (LowHealthThreshold - CriticalHealthThreshold);
        NewColor = FLinearColor::LerpUsingHSV(CriticalHealthColor, LowHealthColor, Alpha);
    }
    else
    {
        // 정상 체력 - 녹색으로 보간
        float Alpha = (HealthPercent - LowHealthThreshold) / (1.0f - LowHealthThreshold);
        NewColor = FLinearColor::LerpUsingHSV(LowHealthColor, NormalHealthColor, Alpha);
    }
    
    HealthBar->SetFillColorAndOpacity(NewColor);
    
    // 테두리 색상도 업데이트
    if (HealthBarBorder)
    {
        HealthBarBorder->SetBrushColor(NewColor);
    }
}

void UHSHealthBarWidget::UpdateDelayedHealthBar()
{
    if (HealthBarBackground)
    {
        // 현재 체력 비율 계산
        float CurrentHealthPercent = CachedMaxHealth > 0 ? CachedCurrentHealth / CachedMaxHealth : 0.0f;
        
        // 부드럽게 배경 체력바 업데이트
        DelayedHealthPercent = FMath::FInterpTo(
            DelayedHealthPercent,
            CurrentHealthPercent,
            GetWorld()->GetDeltaSeconds(),
            HealthBarInterpSpeed
        );
        
        HealthBarBackground->SetPercent(DelayedHealthPercent);
    }
}
