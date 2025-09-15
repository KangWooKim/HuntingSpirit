// HSDamageNumberWidget.cpp
#include "HSDamageNumberWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/KismetMathLibrary.h"

void UHSDamageNumberWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 데미지 타입별 색상 설정
    DamageTypeColors.Add(EDamageNumberType::Normal, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));      // 흰색
    DamageTypeColors.Add(EDamageNumberType::Critical, FLinearColor(1.0f, 0.2f, 0.0f, 1.0f));    // 빨간색
    DamageTypeColors.Add(EDamageNumberType::Healing, FLinearColor(0.0f, 1.0f, 0.2f, 1.0f));     // 녹색
    DamageTypeColors.Add(EDamageNumberType::Blocked, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));     // 회색
    DamageTypeColors.Add(EDamageNumberType::Immune, FLinearColor(0.8f, 0.8f, 0.0f, 1.0f));      // 노란색
    
    // 데미지 타입별 크기 설정
    DamageTypeScales.Add(EDamageNumberType::Normal, 1.0f);
    DamageTypeScales.Add(EDamageNumberType::Critical, 1.5f);
    DamageTypeScales.Add(EDamageNumberType::Healing, 1.2f);
    DamageTypeScales.Add(EDamageNumberType::Blocked, 0.8f);
    DamageTypeScales.Add(EDamageNumberType::Immune, 0.9f);
    
    // 텍스트 포맷 설정 - 멤버 변수 초기화 제거 (헤더에서 const로 선언)
    
    // 애니메이션 설정
    FloatSpeed = 100.0f;
    FadeStartTime = 0.5f;
    RandomOffsetRange = 50.0f;
    bUseManualAnimation = true;  // 기본적으로 수동 애니메이션 사용
    AnimationDuration = 1.5f;
    
    // 초기 상태
    bIsAnimating = false;
    AnimationTime = 0.0f;
    CurrentDamageType = EDamageNumberType::Normal;
}

void UHSDamageNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // 수동 애니메이션 업데이트
    if (bIsAnimating && bUseManualAnimation)
    {
        UpdateManualAnimation(InDeltaTime);
    }
}

void UHSDamageNumberWidget::SetDamageNumber(float Damage, bool bIsCritical)
{
    EDamageNumberType DamageType = bIsCritical ? EDamageNumberType::Critical : EDamageNumberType::Normal;
    SetDamageNumberWithType(Damage, DamageType);
}

void UHSDamageNumberWidget::SetDamageNumberWithType(float Value, EDamageNumberType DamageType)
{
    if (!DamageText)
    {
        return;
    }
    
    CurrentDamageType = DamageType;
    
    // 텍스트 포맷 설정
    FString FormattedText;
    switch (DamageType)
    {
        case EDamageNumberType::Critical:
            FormattedText = FString::Printf(TEXT("%.0f!"), Value);
            break;
        case EDamageNumberType::Healing:
            FormattedText = FString::Printf(TEXT("+%.0f"), Value);
            break;
        case EDamageNumberType::Blocked:
            FormattedText = TEXT("Blocked");
            break;
        case EDamageNumberType::Immune:
            FormattedText = TEXT("Immune");
            break;
        default:
            FormattedText = FString::Printf(TEXT("%.0f"), Value);
            break;
    }
    
    DamageText->SetText(FText::FromString(FormattedText));
    
    // 색상 설정
    if (DamageTypeColors.Contains(DamageType))
    {
        DamageText->SetColorAndOpacity(DamageTypeColors[DamageType]);
    }
    
    // 크기 설정
    if (DamageTypeScales.Contains(DamageType))
    {
        float Scale = DamageTypeScales[DamageType];
        InitialScale = Scale;
        TargetScale = Scale * 1.2f;  // 애니메이션 시 약간 커짐
        
        if (RootPanel)
        {
            RootPanel->SetRenderScale(FVector2D(InitialScale, InitialScale));
        }
    }
}

void UHSDamageNumberWidget::PlayDamageAnimation()
{
    // 랜덤 움직임 적용
    ApplyRandomMovement();
    
    if (bUseManualAnimation)
    {
        // 수동 애니메이션 시작
        bIsAnimating = true;
        AnimationTime = 0.0f;
        SetVisibility(ESlateVisibility::HitTestInvisible);
        
        // 초기 위치 저장
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
        {
            InitialPosition = CanvasSlot->GetPosition();
        }
    }
    else
    {
        // 블루프린트 애니메이션 사용 - 부모 클래스의 PlayAnimation 호출
        if (CurrentDamageType == EDamageNumberType::Critical && CriticalBounceAnim)
        {
            PlayAnimation(CriticalBounceAnim, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);
        }
        else if (FloatUpAnim)
        {
            PlayAnimation(FloatUpAnim, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);
        }
    }
}

void UHSDamageNumberWidget::ResetWidget()
{
    // 위젯 초기화
    bIsAnimating = false;
    AnimationTime = 0.0f;
    
    if (RootPanel)
    {
        RootPanel->SetRenderScale(FVector2D(1.0f, 1.0f));
        RootPanel->SetRenderOpacity(1.0f);
    }
    
    if (DamageText)
    {
        DamageText->SetText(FText::GetEmpty());
    }
    
    // 위치 초기화
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        CanvasSlot->SetPosition(FVector2D::ZeroVector);
    }
}

void UHSDamageNumberWidget::ApplyRandomMovement()
{
    // 랜덤 오프셋 생성
    float RandomX = FMath::RandRange(-RandomOffsetRange, RandomOffsetRange);
    float RandomY = FMath::RandRange(-RandomOffsetRange * 0.5f, RandomOffsetRange * 0.5f);
    RandomOffset = FVector2D(RandomX, RandomY);
}

void UHSDamageNumberWidget::UpdateManualAnimation(float DeltaTime)
{
    AnimationTime += DeltaTime;
    float Progress = FMath::Clamp(AnimationTime / AnimationDuration, 0.0f, 1.0f);
    
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        // 위치 업데이트 (위로 떠오르는 효과)
        float VerticalOffset = FloatSpeed * AnimationTime;
        float HorizontalOffset = RandomOffset.X * FMath::Sin(Progress * PI);
        
        FVector2D NewPosition = InitialPosition + FVector2D(HorizontalOffset, -VerticalOffset);
        CanvasSlot->SetPosition(NewPosition);
    }
    
    if (RootPanel)
    {
        // 크기 애니메이션 (처음에 커졌다가 작아짐)
        float ScaleProgress = FMath::Sin(Progress * PI);
        float CurrentScale = FMath::Lerp(InitialScale, TargetScale, ScaleProgress * 0.5f);
        RootPanel->SetRenderScale(FVector2D(CurrentScale, CurrentScale));
        
        // 페이드 아웃
        if (AnimationTime > FadeStartTime)
        {
            float FadeProgress = (AnimationTime - FadeStartTime) / (AnimationDuration - FadeStartTime);
            float Opacity = 1.0f - FadeProgress;
            RootPanel->SetRenderOpacity(Opacity);
        }
    }
    
    // 애니메이션 완료
    if (Progress >= 1.0f)
    {
        bIsAnimating = false;
        SetVisibility(ESlateVisibility::Hidden);
    }
}
