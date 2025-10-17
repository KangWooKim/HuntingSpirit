#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Animation/WidgetAnimation.h"
#include "../../Networking/Matchmaking/HSMatchmakingSystem.h"
#include "../../Core/SaveGame/HSSaveGameData.h"
#include "HSMainMenuWidget.generated.h"

UENUM(BlueprintType)
enum class EHSMenuState : uint8
{
    MainMenu,
    Matchmaking,
    Settings,
    Profile,
    Achievements,
    Credits,
    Loading,
    MatchFound
};

UENUM(BlueprintType)
enum class EHSSettingsCategory : uint8
{
    Graphics,
    Audio,
    Input,
    Gameplay,
    Network,
    Accessibility
};

USTRUCT(BlueprintType)
struct FHSMenuAnimationInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float FadeInDuration = 0.3f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float FadeOutDuration = 0.2f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float SlideInDuration = 0.4f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float SlideOutDuration = 0.3f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float ScaleInDuration = 0.25f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float ScaleOutDuration = 0.2f;

    FHSMenuAnimationInfo()
    {
        FadeInDuration = 0.3f;
        FadeOutDuration = 0.2f;
        SlideInDuration = 0.4f;
        SlideOutDuration = 0.3f;
        ScaleInDuration = 0.25f;
        ScaleOutDuration = 0.2f;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuStateChanged, EHSMenuState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsCategoryChanged, EHSSettingsCategory, NewCategory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchmakingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchmakingCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchAccepted, const FString&, MatchID);

/**
 * 메인 메뉴 위젯 - 게임의 중심 UI 시스템
 * 매치메이킹, 설정, 프로필 관리 통합
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UHSMainMenuWidget(const FObjectInitializer& ObjectInitializer);

    // UUserWidget 인터페이스
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // === 메뉴 네비게이션 ===
    UFUNCTION(BlueprintCallable, Category = "Menu Navigation")
    void SetMenuState(EHSMenuState NewState);

    UFUNCTION(BlueprintPure, Category = "Menu Navigation")
    EHSMenuState GetCurrentMenuState() const { return CurrentMenuState; }

    UFUNCTION(BlueprintCallable, Category = "Menu Navigation")
    void NavigateBack();

    UFUNCTION(BlueprintCallable, Category = "Menu Navigation")
    void ShowMainMenu();

    // === 매치메이킹 ===
    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void StartQuickMatch();

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void StartRankedMatch();

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void StartCustomMatch();

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void CancelMatchmaking();

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void AcceptMatch();

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void DeclineMatch();

    // === 설정 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ShowSettings(EHSSettingsCategory Category = EHSSettingsCategory::Graphics);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetSettingsCategory(EHSSettingsCategory Category);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplySettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ResetSettingsToDefault();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SaveSettings();

    // === 프로필 및 통계 ===
    UFUNCTION(BlueprintCallable, Category = "Profile")
    void ShowProfile();

    UFUNCTION(BlueprintCallable, Category = "Profile")
    void RefreshPlayerStats();

    UFUNCTION(BlueprintCallable, Category = "Profile")
    void ShowAchievements();

    // === UI 애니메이션 ===
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void PlayFadeInAnimation(UWidget* TargetWidget);

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void PlayFadeOutAnimation(UWidget* TargetWidget);

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void PlaySlideInAnimation(UWidget* TargetWidget, bool bFromLeft = true);

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void PlaySlideOutAnimation(UWidget* TargetWidget, bool bToLeft = true);

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Menu Events")
    FOnMenuStateChanged OnMenuStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Menu Events")
    FOnSettingsCategoryChanged OnSettingsCategoryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Matchmaking Events")
    FOnMatchmakingStarted OnMatchmakingStarted;

    UPROPERTY(BlueprintAssignable, Category = "Matchmaking Events")
    FOnMatchmakingCancelled OnMatchmakingCancelled;

    UPROPERTY(BlueprintAssignable, Category = "Matchmaking Events")
    FOnMatchAccepted OnMatchAccepted;

protected:
    // === 위젯 바인딩 - 메인 메뉴 ===
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_QuickMatch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_RankedMatch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_CustomMatch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Settings;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Profile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Quit;

    // === 위젯 바인딩 - 매치메이킹 ===
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> Panel_Matchmaking;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MatchmakingStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_EstimatedWaitTime;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> ProgressBar_Matchmaking;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_CancelMatchmaking;

    // === 위젯 바인딩 - 매치 발견 ===
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> Panel_MatchFound;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MatchInfo;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_AcceptMatch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_DeclineMatch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> ProgressBar_MatchAcceptance;

    // === 위젯 바인딩 - 설정 ===
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> Panel_Settings;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBox_SettingsCategories;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBox_SettingsContent;

    // 그래픽 설정
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> ComboBox_OverallQuality;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> ComboBox_Resolution;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CheckBox_Fullscreen;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CheckBox_VSync;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_FrameRate;

    // 오디오 설정
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MasterVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_SFXVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MusicVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_VoiceVolume;

    // 입력 설정
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MouseSensitivity;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_ControllerSensitivity;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CheckBox_InvertMouseY;

    // === 위젯 바인딩 - 프로필 ===
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> Panel_Profile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_PlayerName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_PlayerLevel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_TotalPlayTime;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_TotalRuns;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_SuccessRate;

    // === 위젯 바인딩 - 오류 표시 (선택적) ===
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> Panel_ErrorDialog;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_ErrorMessage;

    // === 내부 상태 ===
    UPROPERTY()
    EHSMenuState CurrentMenuState;

    UPROPERTY()
    EHSMenuState PreviousMenuState;

    UPROPERTY()
    EHSSettingsCategory CurrentSettingsCategory;

    UPROPERTY()
    TObjectPtr<UHSMatchmakingSystem> MatchmakingSystem;

    UPROPERTY()
    TObjectPtr<UHSSaveGameData> CurrentSaveData;

    // === 애니메이션 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    FHSMenuAnimationInfo AnimationSettings;

    // === 메뉴 네비게이션 스택 ===
    UPROPERTY()
    TArray<EHSMenuState> MenuStateStack;

    // === 콜백 함수 ===
    UFUNCTION()
    void OnQuickMatchClicked();

    UFUNCTION()
    void OnRankedMatchClicked();

    UFUNCTION()
    void OnCustomMatchClicked();

    UFUNCTION()
    void OnSettingsClicked();

    UFUNCTION()
    void OnProfileClicked();

    UFUNCTION()
    void OnQuitClicked();

    UFUNCTION()
    void OnCancelMatchmakingClicked();

    UFUNCTION()
    void OnAcceptMatchClicked();

    UFUNCTION()
    void OnDeclineMatchClicked();

    // 매치메이킹 이벤트 콜백
    UFUNCTION()
    void OnMatchmakingStatusChanged(EHSMatchmakingStatus NewStatus);

    UFUNCTION()
    void OnMatchFound(const FHSMatchInfo& MatchInfo);

    UFUNCTION()
    void OnMatchmakingError(const FString& ErrorMessage);

    UFUNCTION()
    void OnEstimatedWaitTimeUpdated(float WaitTimeSeconds);

    // 설정 변경 콜백
    UFUNCTION()
    void OnOverallQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnFullscreenChanged(bool bIsChecked);

    UFUNCTION()
    void OnVSyncChanged(bool bIsChecked);

    UFUNCTION()
    void OnFrameRateChanged(float Value);

    UFUNCTION()
    void OnMasterVolumeChanged(float Value);

    UFUNCTION()
    void OnSFXVolumeChanged(float Value);

    UFUNCTION()
    void OnMusicVolumeChanged(float Value);

    UFUNCTION()
    void OnMouseSensitivityChanged(float Value);

    // === 내부 함수 ===
    void InitializeWidgetBindings();
    void InitializeMatchmakingSystem();
    void InitializeCurrentSaveData();
    
    void UpdateMenuVisibility();
    void UpdateMatchmakingUI();
    void UpdateSettingsUI();
    void UpdateProfileUI();
    
    void LoadSettingsFromSaveData();
    void SaveSettingsToSaveData();
    void ApplyCurrentSettings();
    
    void PopulateQualityComboBox();
    void PopulateResolutionComboBox();
    void UpdateStatisticsDisplay();
    
    // 애니메이션 헬퍼
    void PlayWidgetAnimation(UWidget* TargetWidget, const FString& AnimationType, float Duration);
    void OnAnimationFinished();
    
    // 유틸리티
    FString FormatPlayTime(int32 TotalSeconds) const;
    FString FormatSuccessRate(int32 Successful, int32 Total) const;
    void ShowErrorDialog(const FString& ErrorMessage);
    
    // 메모리 캐싱
    mutable TMap<EHSMenuState, TWeakObjectPtr<UWidget>> CachedPanels;
    mutable float LastStatsUpdateTime;
    static constexpr float StatsUpdateInterval = 1.0f;

private:
    // 매치 수락 타이머
    FTimerHandle MatchAcceptanceTimerHandle;
    float MatchAcceptanceTimeRemaining;
    
    // UI 업데이트 타이머
    FTimerHandle UIUpdateTimerHandle;
    
    // 이전 설정 백업 (취소 시 복원용)
    FHSGraphicsSettings BackupGraphicsSettings;
    FHSAudioSettings BackupAudioSettings;
    FHSInputSettings BackupInputSettings;
    FHSGameplaySettings BackupGameplaySettings;
    FHSNetworkSettings BackupNetworkSettings;
    FHSAccessibilitySettings BackupAccessibilitySettings;
    
    // 내부 상태 관리
    bool bIsInitialized;
    bool bIsAnimating;
    
    // 매치 수락 타임아웃 처리
    void StartMatchAcceptanceTimer();
    void UpdateMatchAcceptanceTimer();
    void OnMatchAcceptanceTimeout();
    
    // 설정 검증
    bool ValidateSettings() const;
    void RestoreBackupSettings();
    
    static constexpr float MatchAcceptanceTimeoutSeconds = 30.0f;
    static constexpr float UIUpdateIntervalSeconds = 0.1f;
    static constexpr float ErrorMessageDisplaySeconds = 4.0f;

    void HideErrorDialog();
    FHSMatchmakingRequest BuildMatchmakingRequest(EHSMatchType MatchType, int32 DefaultMaxPing, bool bAllowCrossPlatform) const;
    float CalculatePlayerSkillRating() const;
    EHSRegion ResolvePreferredRegion() const;

    mutable FTimerHandle ErrorDialogTimerHandle;
};
