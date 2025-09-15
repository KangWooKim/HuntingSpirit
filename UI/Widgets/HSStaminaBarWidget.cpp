// HSStaminaBarWidget.cpp
#include "HSStaminaBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"

void UHSStaminaBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 색상 설정
    NormalStaminaColor = FLinearColor(1.0f, 0.84f, 0.0f, 1.0f);      // 황금색
    LowStaminaColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);          // 주황색
    ExhaustedStaminaColor = FLinearColor(0.5f, 0.0f, 0.0f, 1.0f);    // 어두운 빨간색
    RegeneratingStaminaColor = FLinearColor(0.0f, 0.7f, 1.0f, 1.0f); // 하늘색
    
    // 임계값 설정
    LowStaminaThreshold = 0.3f;      // 30% 이하
    ExhaustedThreshold = 0.05f;      // 5% 이하
    
    // 동작 설정
    AutoHideDelay = 3.0f;
    bAutoHideWhenFull = true;
    GhostBarSpeed = 0.5f;
    
    // 초기 상태
    CurrentStaminaPercent = 1.0f;
    GhostStaminaPercent = 1.0f;
    bIsRegenerating = false;
    bIsExhausted = false;
    
    // 스태미너바 초기화
    if (StaminaBar)
    {
        StaminaBar->SetPercent(1.0f);
        StaminaBar->SetFillColorAndOpacity(NormalStaminaColor);
    }
    
    if (StaminaBarGhost)
    {
        StaminaBarGhost->SetPercent(1.0f);
        StaminaBarGhost->SetFillColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f));
    }
}

void UHSStaminaBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // 고스트 바 업데이트 (소비한 스태미너를 천천히 따라감)
    UpdateGhostBar(InDeltaTime);
}

void UHSStaminaBarWidget::UpdateStaminaBar(float CurrentStamina, float MaxStamina)
{
    // 스태미너 비율 계산
    const float StaminaPercent = MaxStamina > 0 ? CurrentStamina / MaxStamina : 0.0f;
    const float PreviousPercent = CurrentStaminaPercent;
    CurrentStaminaPercent = StaminaPercent;
    
    // 스태미너바 업데이트
    if (StaminaBar)
    {
        StaminaBar->SetPercent(StaminaPercent);
    }
    
    // 스태미너 텍스트 업데이트 (선택적 표시)
    if (StaminaText)
    {
        // 전체 스태미너일 때는 텍스트 숨기기
        if (FMath::IsNearlyEqual(StaminaPercent, 1.0f))
        {
            StaminaText->SetVisibility(ESlateVisibility::Hidden);
        }
        else
        {
            StaminaText->SetVisibility(ESlateVisibility::Visible);
            StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), StaminaPercent * 100)));
        }
    }
    
    // 색상 업데이트
    UpdateStaminaBarColor(StaminaPercent);
    
    // 스태미너 소진 체크
    bool bWasExhausted = bIsExhausted;
    bIsExhausted = StaminaPercent <= ExhaustedThreshold;
    
    if (bIsExhausted && !bWasExhausted)
    {
        PlayExhaustedWarning();
    }
    
    // 스태미너 회복 중 체크
    if (StaminaPercent > PreviousPercent && StaminaPercent < 1.0f)
    {
        SetRegenerating(true);
    }
    else if (FMath::IsNearlyEqual(StaminaPercent, 1.0f))
    {
        SetRegenerating(false);
    }
    
    // 스태미너가 변경되면 위젯 표시
    if (!FMath::IsNearlyEqual(StaminaPercent, PreviousPercent))
    {
        SetVisibility(ESlateVisibility::Visible);
        LastStaminaChangeTime = GetWorld()->GetTimeSeconds();
        
        // 자동 숨기기 타이머 시작
        if (bAutoHideWhenFull)
        {
            StartAutoHideTimer();
        }
    }
    
    // 고스트 바 업데이트 (스태미너가 감소한 경우)
    if (StaminaPercent < GhostStaminaPercent)
    {
        // 즉시 고스트 바를 현재 값으로 설정하지 않고, 천천히 따라가도록 함
        // UpdateGhostBar에서 처리
    }
    else
    {
        // 스태미너가 증가한 경우 고스트 바도 즉시 업데이트
        GhostStaminaPercent = StaminaPercent;
        if (StaminaBarGhost)
        {
            StaminaBarGhost->SetPercent(GhostStaminaPercent);
        }
    }
}

void UHSStaminaBarWidget::PlayExhaustedWarning()
{
    if (ExhaustedAnim)
    {
        PlayAnimation(ExhaustedAnim);
    }
    
    // 화면 흔들림 효과 등 추가 가능
}

void UHSStaminaBarWidget::SetRegenerating(bool bInIsRegenerating)
{
    bIsRegenerating = bInIsRegenerating;
    
    if (bIsRegenerating && RegeneratingAnim)
    {
        // 회복 중 애니메이션 반복 재생
        PlayAnimation(RegeneratingAnim, 0.0f, 0, EUMGSequencePlayMode::Forward);
    }
    else if (!bIsRegenerating && IsAnimationPlaying(RegeneratingAnim))
    {
        StopAnimation(RegeneratingAnim);
    }
}

void UHSStaminaBarWidget::UpdateStaminaBarColor(float StaminaPercent)
{
    if (!StaminaBar)
    {
        return;
    }
    
    FLinearColor NewColor;
    
    if (bIsRegenerating)
    {
        // 회복 중일 때는 회복 색상 사용
        NewColor = RegeneratingStaminaColor;
    }
    else if (StaminaPercent <= ExhaustedThreshold)
    {
        // 소진 상태
        NewColor = ExhaustedStaminaColor;
    }
    else if (StaminaPercent <= LowStaminaThreshold)
    {
        // 낮은 스태미너
        float Alpha = (StaminaPercent - ExhaustedThreshold) / (LowStaminaThreshold - ExhaustedThreshold);
        NewColor = FLinearColor::LerpUsingHSV(ExhaustedStaminaColor, LowStaminaColor, Alpha);
    }
    else
    {
        // 정상 스태미너
        float Alpha = (StaminaPercent - LowStaminaThreshold) / (1.0f - LowStaminaThreshold);
        NewColor = FLinearColor::LerpUsingHSV(LowStaminaColor, NormalStaminaColor, Alpha);
    }
    
    StaminaBar->SetFillColorAndOpacity(NewColor);
}

void UHSStaminaBarWidget::UpdateGhostBar(float DeltaTime)
{
    // 고스트 바가 현재 스태미너를 천천히 따라감
    if (GhostStaminaPercent > CurrentStaminaPercent)
    {
        GhostStaminaPercent = FMath::FInterpTo(
            GhostStaminaPercent,
            CurrentStaminaPercent,
            DeltaTime,
            GhostBarSpeed
        );
        
        if (StaminaBarGhost)
        {
            StaminaBarGhost->SetPercent(GhostStaminaPercent);
        }
    }
}

void UHSStaminaBarWidget::StartAutoHideTimer()
{
    // 기존 타이머 취소
    GetWorld()->GetTimerManager().ClearTimer(AutoHideTimerHandle);
    
    // 스태미너가 가득 찬 경우에만 자동 숨기기
    if (FMath::IsNearlyEqual(CurrentStaminaPercent, 1.0f))
    {
        GetWorld()->GetTimerManager().SetTimer(
            AutoHideTimerHandle,
            this,
            &UHSStaminaBarWidget::HideStaminaBar,
            AutoHideDelay,
            false
        );
    }
}

void UHSStaminaBarWidget::HideStaminaBar()
{
    // 페이드 아웃 애니메이션이 있다면 재생
    // 없다면 즉시 숨기기
    SetVisibility(ESlateVisibility::Hidden);
}
