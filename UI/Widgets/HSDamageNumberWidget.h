// HSDamageNumberWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSDamageNumberWidget.generated.h"

// Forward declarations
class UTextBlock;
class UCanvasPanel;

// 데미지 타입 열거형
UENUM(BlueprintType)
enum class EDamageNumberType : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    Critical    UMETA(DisplayName = "Critical"),
    Healing     UMETA(DisplayName = "Healing"),
    Blocked     UMETA(DisplayName = "Blocked"),
    Immune      UMETA(DisplayName = "Immune")
};

/**
 * 데미지 숫자를 표시하는 위젯
 * 플로팅 텍스트 애니메이션과 함께 데미지/힐링 값을 표시합니다.
 */
UCLASS()
class HUNTINGSPIRIT_API UHSDamageNumberWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 위젯 초기화
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    // 데미지 넘버 설정
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetDamageNumber(float Damage, bool bIsCritical = false);
    
    // 데미지 타입별 설정
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetDamageNumberWithType(float Value, EDamageNumberType DamageType);
    
    // 애니메이션 재생 - PlayAnimation 이름 충돌 피하기 위해 이름 변경
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void PlayDamageAnimation();
    
    // 위젯 초기화 (풀링용)
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void ResetWidget();

protected:
    // UI 컴포넌트
    UPROPERTY(meta = (BindWidget))
    UTextBlock* DamageText;
    
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* RootPanel;
    
    // 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    UWidgetAnimation* FloatUpAnim;
    
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    UWidgetAnimation* CriticalBounceAnim;
    
    // 데미지 타입별 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TMap<EDamageNumberType, FLinearColor> DamageTypeColors;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TMap<EDamageNumberType, float> DamageTypeScales;
    
    // 텍스트 포맷
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FString NormalDamageFormat;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FString CriticalDamageFormat;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FString HealingFormat;

private:
    // 랜덤 움직임 적용
    void ApplyRandomMovement();
    
    // 수동 애니메이션 업데이트 (애니메이션 블루프린트가 없을 경우)
    void UpdateManualAnimation(float DeltaTime);
    
    // 현재 데미지 타입
    EDamageNumberType CurrentDamageType;
    
    // 수동 애니메이션용 변수
    float AnimationTime;
    float AnimationDuration;
    FVector2D InitialPosition;
    FVector2D RandomOffset;
    float InitialScale;
    float TargetScale;
    
    // 애니메이션 상태
    bool bIsAnimating;
    
    // 애니메이션 설정
    UPROPERTY(EditAnywhere, Category = "Animation")
    float FloatSpeed;
    
    UPROPERTY(EditAnywhere, Category = "Animation")
    float FadeStartTime;
    
    UPROPERTY(EditAnywhere, Category = "Animation")
    float RandomOffsetRange;
    
    UPROPERTY(EditAnywhere, Category = "Animation")
    bool bUseManualAnimation;
};
