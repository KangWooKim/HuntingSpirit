#include "HSMainMenuWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CanvasPanel.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

UHSMainMenuWidget::UHSMainMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    CurrentMenuState = EHSMenuState::MainMenu;
    PreviousMenuState = EHSMenuState::MainMenu;
    CurrentSettingsCategory = EHSSettingsCategory::Graphics;
    MatchmakingSystem = nullptr;
    CurrentSaveData = nullptr;
    bIsInitialized = false;
    bIsAnimating = false;
    MatchAcceptanceTimeRemaining = 0.0f;
    LastStatsUpdateTime = 0.0f;
    
    // 애니메이션 설정 기본값
    AnimationSettings = FHSMenuAnimationInfo();
}

void UHSMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 메인 메뉴 위젯 초기화 시작"));
    
    InitializeWidgetBindings();
    InitializeMatchmakingSystem();
    InitializeCurrentSaveData();
    
    // 기본 메뉴 상태 설정
    SetMenuState(EHSMenuState::MainMenu);
    
    // UI 업데이트 타이머 시작
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            UIUpdateTimerHandle,
            this,
            &UHSMainMenuWidget::UpdateMenuVisibility,
            UIUpdateIntervalSeconds,
            true
        );
    }
    
    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 메인 메뉴 위젯 초기화 완료"));
}

void UHSMainMenuWidget::NativeDestruct()
{
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 메인 메뉴 위젯 종료 중"));
    
    // 타이머 정리
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(UIUpdateTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(MatchAcceptanceTimerHandle);
    }
    
    // 매치메이킹 시스템 델리게이트 해제
    if (MatchmakingSystem)
    {
        MatchmakingSystem->OnMatchmakingStatusChanged.RemoveAll(this);
        MatchmakingSystem->OnMatchFound.RemoveAll(this);
        MatchmakingSystem->OnMatchmakingError.RemoveAll(this);
        MatchmakingSystem->OnEstimatedWaitTimeUpdated.RemoveAll(this);
    }
    
    Super::NativeDestruct();
}

void UHSMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // 매치 수락 타이머 업데이트
    if (CurrentMenuState == EHSMenuState::MatchFound && MatchAcceptanceTimeRemaining > 0.0f)
    {
        UpdateMatchAcceptanceTimer();
    }
    
    // 통계 업데이트 (성능 최적화를 위한 주기적 업데이트)
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentMenuState == EHSMenuState::Profile && 
        CurrentTime - LastStatsUpdateTime > StatsUpdateInterval)
    {
        UpdateStatisticsDisplay();
        LastStatsUpdateTime = CurrentTime;
    }
}

void UHSMainMenuWidget::SetMenuState(EHSMenuState NewState)
{
    if (CurrentMenuState == NewState || bIsAnimating)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 메뉴 상태 변경 %d -> %d"), 
           (int32)CurrentMenuState, (int32)NewState);
    
    // 메뉴 스택 관리
    if (NewState != EHSMenuState::MainMenu)
    {
        MenuStateStack.AddUnique(CurrentMenuState);
    }
    
    PreviousMenuState = CurrentMenuState;
    CurrentMenuState = NewState;
    
    // 메뉴별 특별 처리
    switch (NewState)
    {
        case EHSMenuState::Settings:
            LoadSettingsFromSaveData();
            break;
        case EHSMenuState::Profile:
            RefreshPlayerStats();
            break;
        case EHSMenuState::MatchFound:
            StartMatchAcceptanceTimer();
            break;
    }
    
    UpdateMenuVisibility();
    OnMenuStateChanged.Broadcast(NewState);
}

void UHSMainMenuWidget::NavigateBack()
{
    if (MenuStateStack.Num() > 0)
    {
        EHSMenuState PrevState = MenuStateStack.Pop();
        SetMenuState(PrevState);
    }
    else
    {
        SetMenuState(EHSMenuState::MainMenu);
    }
}

void UHSMainMenuWidget::ShowMainMenu()
{
    MenuStateStack.Empty();
    SetMenuState(EHSMenuState::MainMenu);
}

void UHSMainMenuWidget::StartQuickMatch()
{
    if (!MatchmakingSystem)
    {
        ShowErrorDialog(TEXT("매치메이킹 시스템을 사용할 수 없습니다."));
        return;
    }
    
    FHSMatchmakingRequest Request;
    Request.MatchType = EHSMatchType::QuickMatch;
    Request.PreferredRegion = EHSRegion::Auto;
    Request.MaxPing = 100;
    Request.SkillRating = 1000.0f; // 실제로는 플레이어 레이팅 사용
    Request.bAllowCrossPlatform = true;
    
    if (MatchmakingSystem->StartMatchmaking(Request))
    {
        SetMenuState(EHSMenuState::Matchmaking);
        OnMatchmakingStarted.Broadcast();
    }
    else
    {
        ShowErrorDialog(TEXT("매치메이킹을 시작할 수 없습니다."));
    }
}

void UHSMainMenuWidget::StartRankedMatch()
{
    if (!MatchmakingSystem)
    {
        ShowErrorDialog(TEXT("매치메이킹 시스템을 사용할 수 없습니다."));
        return;
    }
    
    FHSMatchmakingRequest Request;
    Request.MatchType = EHSMatchType::RankedMatch;
    Request.PreferredRegion = EHSRegion::Auto;
    Request.MaxPing = 80; // 랭크 매치는 더 엄격한 핑 요구사항
    Request.SkillRating = 1000.0f;
    Request.bAllowCrossPlatform = false; // 랭크 매치는 크로스플랫폼 비활성화
    
    if (MatchmakingSystem->StartMatchmaking(Request))
    {
        SetMenuState(EHSMenuState::Matchmaking);
        OnMatchmakingStarted.Broadcast();
    }
    else
    {
        ShowErrorDialog(TEXT("랭크 매치를 시작할 수 없습니다."));
    }
}

void UHSMainMenuWidget::StartCustomMatch()
{
    // 커스텀 매치 로직 구현 예정
    ShowErrorDialog(TEXT("커스텀 매치는 아직 구현되지 않았습니다."));
}

void UHSMainMenuWidget::CancelMatchmaking()
{
    if (MatchmakingSystem)
    {
        MatchmakingSystem->CancelMatchmaking();
        OnMatchmakingCancelled.Broadcast();
    }
    
    SetMenuState(EHSMenuState::MainMenu);
}

void UHSMainMenuWidget::AcceptMatch()
{
    if (MatchmakingSystem && !MatchmakingSystem->GetCurrentMatchID().IsEmpty())
    {
        FString MatchID = MatchmakingSystem->GetCurrentMatchID();
        
        if (MatchmakingSystem->AcceptMatch(MatchID))
        {
            OnMatchAccepted.Broadcast(MatchID);
            SetMenuState(EHSMenuState::Loading);
        }
        else
        {
            ShowErrorDialog(TEXT("매치 수락에 실패했습니다."));
        }
    }
}

void UHSMainMenuWidget::DeclineMatch()
{
    if (MatchmakingSystem && !MatchmakingSystem->GetCurrentMatchID().IsEmpty())
    {
        MatchmakingSystem->DeclineMatch(MatchmakingSystem->GetCurrentMatchID());
        SetMenuState(EHSMenuState::Matchmaking);
    }
}

void UHSMainMenuWidget::ShowSettings(EHSSettingsCategory Category)
{
    SetSettingsCategory(Category);
    SetMenuState(EHSMenuState::Settings);
}

void UHSMainMenuWidget::SetSettingsCategory(EHSSettingsCategory Category)
{
    if (CurrentSettingsCategory == Category)
    {
        return;
    }
    
    CurrentSettingsCategory = Category;
    UpdateSettingsUI();
    OnSettingsCategoryChanged.Broadcast(Category);
}

void UHSMainMenuWidget::ApplySettings()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    // 설정 검증
    if (!ValidateSettings())
    {
        ShowErrorDialog(TEXT("잘못된 설정 값이 있습니다."));
        return;
    }
    
    // 설정 적용
    ApplyCurrentSettings();
    SaveSettingsToSaveData();
    
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 설정 적용 완료"));
}

void UHSMainMenuWidget::ResetSettingsToDefault()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    // 기본값으로 재설정
    CurrentSaveData->GraphicsSettings = FHSGraphicsSettings();
    CurrentSaveData->AudioSettings = FHSAudioSettings();
    CurrentSaveData->InputSettings = FHSInputSettings();
    CurrentSaveData->GameplaySettings = FHSGameplaySettings();
    CurrentSaveData->NetworkSettings = FHSNetworkSettings();
    CurrentSaveData->AccessibilitySettings = FHSAccessibilitySettings();
    
    LoadSettingsFromSaveData();
    UpdateSettingsUI();
    
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 설정 기본값으로 재설정"));
}

void UHSMainMenuWidget::SaveSettings()
{
    SaveSettingsToSaveData();
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 설정 저장 완료"));
}

void UHSMainMenuWidget::ShowProfile()
{
    SetMenuState(EHSMenuState::Profile);
}

void UHSMainMenuWidget::RefreshPlayerStats()
{
    UpdateStatisticsDisplay();
}

void UHSMainMenuWidget::ShowAchievements()
{
    SetMenuState(EHSMenuState::Achievements);
}

void UHSMainMenuWidget::PlayFadeInAnimation(UWidget* TargetWidget)
{
    if (!TargetWidget)
    {
        return;
    }
    
    PlayWidgetAnimation(TargetWidget, TEXT("FadeIn"), AnimationSettings.FadeInDuration);
}

void UHSMainMenuWidget::PlayFadeOutAnimation(UWidget* TargetWidget)
{
    if (!TargetWidget)
    {
        return;
    }
    
    PlayWidgetAnimation(TargetWidget, TEXT("FadeOut"), AnimationSettings.FadeOutDuration);
}

void UHSMainMenuWidget::PlaySlideInAnimation(UWidget* TargetWidget, bool bFromLeft)
{
    if (!TargetWidget)
    {
        return;
    }
    
    FString AnimationType = bFromLeft ? TEXT("SlideInLeft") : TEXT("SlideInRight");
    PlayWidgetAnimation(TargetWidget, AnimationType, AnimationSettings.SlideInDuration);
}

void UHSMainMenuWidget::PlaySlideOutAnimation(UWidget* TargetWidget, bool bToLeft)
{
    if (!TargetWidget)
    {
        return;
    }
    
    FString AnimationType = bToLeft ? TEXT("SlideOutLeft") : TEXT("SlideOutRight");
    PlayWidgetAnimation(TargetWidget, AnimationType, AnimationSettings.SlideOutDuration);
}

void UHSMainMenuWidget::InitializeWidgetBindings()
{
    // 메인 메뉴 버튼 바인딩
    if (Button_QuickMatch)
    {
        Button_QuickMatch->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnQuickMatchClicked);
    }
    
    if (Button_RankedMatch)
    {
        Button_RankedMatch->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnRankedMatchClicked);
    }
    
    if (Button_CustomMatch)
    {
        Button_CustomMatch->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnCustomMatchClicked);
    }
    
    if (Button_Settings)
    {
        Button_Settings->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnSettingsClicked);
    }
    
    if (Button_Profile)
    {
        Button_Profile->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnProfileClicked);
    }
    
    if (Button_Quit)
    {
        Button_Quit->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnQuitClicked);
    }
    
    // 매치메이킹 버튼 바인딩
    if (Button_CancelMatchmaking)
    {
        Button_CancelMatchmaking->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnCancelMatchmakingClicked);
    }
    
    if (Button_AcceptMatch)
    {
        Button_AcceptMatch->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnAcceptMatchClicked);
    }
    
    if (Button_DeclineMatch)
    {
        Button_DeclineMatch->OnClicked.AddDynamic(this, &UHSMainMenuWidget::OnDeclineMatchClicked);
    }
    
    // 설정 위젯 바인딩
    if (ComboBox_OverallQuality)
    {
        ComboBox_OverallQuality->OnSelectionChanged.AddDynamic(this, &UHSMainMenuWidget::OnOverallQualityChanged);
        PopulateQualityComboBox();
    }
    
    if (ComboBox_Resolution)
    {
        ComboBox_Resolution->OnSelectionChanged.AddDynamic(this, &UHSMainMenuWidget::OnResolutionChanged);
        PopulateResolutionComboBox();
    }
    
    if (CheckBox_Fullscreen)
    {
        CheckBox_Fullscreen->OnCheckStateChanged.AddDynamic(this, &UHSMainMenuWidget::OnFullscreenChanged);
    }
    
    if (CheckBox_VSync)
    {
        CheckBox_VSync->OnCheckStateChanged.AddDynamic(this, &UHSMainMenuWidget::OnVSyncChanged);
    }
    
    if (Slider_FrameRate)
    {
        Slider_FrameRate->OnValueChanged.AddDynamic(this, &UHSMainMenuWidget::OnFrameRateChanged);
    }
    
    if (Slider_MasterVolume)
    {
        Slider_MasterVolume->OnValueChanged.AddDynamic(this, &UHSMainMenuWidget::OnMasterVolumeChanged);
    }
    
    if (Slider_SFXVolume)
    {
        Slider_SFXVolume->OnValueChanged.AddDynamic(this, &UHSMainMenuWidget::OnSFXVolumeChanged);
    }
    
    if (Slider_MusicVolume)
    {
        Slider_MusicVolume->OnValueChanged.AddDynamic(this, &UHSMainMenuWidget::OnMusicVolumeChanged);
    }
    
    if (Slider_MouseSensitivity)
    {
        Slider_MouseSensitivity->OnValueChanged.AddDynamic(this, &UHSMainMenuWidget::OnMouseSensitivityChanged);
    }
}

void UHSMainMenuWidget::InitializeMatchmakingSystem()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        MatchmakingSystem = GameInstance->GetSubsystem<UHSMatchmakingSystem>();
        
        if (MatchmakingSystem)
        {
            // 매치메이킹 이벤트 바인딩
            MatchmakingSystem->OnMatchmakingStatusChanged.AddDynamic(this, &UHSMainMenuWidget::OnMatchmakingStatusChanged);
            MatchmakingSystem->OnMatchFound.AddDynamic(this, &UHSMainMenuWidget::OnMatchFound);
            MatchmakingSystem->OnMatchmakingError.AddDynamic(this, &UHSMainMenuWidget::OnMatchmakingError);
            MatchmakingSystem->OnEstimatedWaitTimeUpdated.AddDynamic(this, &UHSMainMenuWidget::OnEstimatedWaitTimeUpdated);
            
            UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 매치메이킹 시스템 연결 완료"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("HSMainMenuWidget: 매치메이킹 시스템을 찾을 수 없음"));
        }
    }
}

void UHSMainMenuWidget::InitializeCurrentSaveData()
{
    // 실제 구현에서는 저장 시스템에서 로드
    CurrentSaveData = NewObject<UHSSaveGameData>(this);
    
    if (CurrentSaveData)
    {
        LoadSettingsFromSaveData();
        UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 저장 데이터 로드 완료"));
    }
}

void UHSMainMenuWidget::UpdateMenuVisibility()
{
    if (!bIsInitialized)
    {
        return;
    }
    
    // 모든 패널 숨김
    TArray<UCanvasPanel*> Panels = {
        Panel_Matchmaking,
        Panel_MatchFound,
        Panel_Settings,
        Panel_Profile
    };
    
    for (UCanvasPanel* Panel : Panels)
    {
        if (Panel)
        {
            Panel->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    
    // 현재 상태에 맞는 패널 표시
    switch (CurrentMenuState)
    {
        case EHSMenuState::MainMenu:
            // 메인 메뉴는 항상 표시됨
            break;
        case EHSMenuState::Matchmaking:
            if (Panel_Matchmaking)
            {
                Panel_Matchmaking->SetVisibility(ESlateVisibility::Visible);
                UpdateMatchmakingUI();
            }
            break;
        case EHSMenuState::MatchFound:
            if (Panel_MatchFound)
            {
                Panel_MatchFound->SetVisibility(ESlateVisibility::Visible);
            }
            break;
        case EHSMenuState::Settings:
            if (Panel_Settings)
            {
                Panel_Settings->SetVisibility(ESlateVisibility::Visible);
                UpdateSettingsUI();
            }
            break;
        case EHSMenuState::Profile:
            if (Panel_Profile)
            {
                Panel_Profile->SetVisibility(ESlateVisibility::Visible);
                UpdateProfileUI();
            }
            break;
    }
}

void UHSMainMenuWidget::UpdateMatchmakingUI()
{
    if (!MatchmakingSystem)
    {
        return;
    }
    
    EHSMatchmakingStatus Status = MatchmakingSystem->GetCurrentStatus();
    
    // 상태 텍스트 업데이트
    if (Text_MatchmakingStatus)
    {
        FString StatusText;
        switch (Status)
        {
            case EHSMatchmakingStatus::Searching:
                StatusText = TEXT("매치 검색 중...");
                break;
            case EHSMatchmakingStatus::MatchFound:
                StatusText = TEXT("매치 발견!");
                break;
            case EHSMatchmakingStatus::JoiningMatch:
                StatusText = TEXT("매치 참가 중...");
                break;
            case EHSMatchmakingStatus::Error:
                StatusText = TEXT("오류 발생");
                break;
            default:
                StatusText = TEXT("대기 중");
                break;
        }
        Text_MatchmakingStatus->SetText(FText::FromString(StatusText));
    }
    
    // 예상 대기시간 업데이트
    if (Text_EstimatedWaitTime)
    {
        float WaitTime = MatchmakingSystem->GetEstimatedWaitTime();
        FString WaitTimeText = FString::Printf(TEXT("예상 대기시간: %.0f초"), WaitTime);
        Text_EstimatedWaitTime->SetText(FText::FromString(WaitTimeText));
    }
    
    // 진행바 업데이트 (시뮬레이션)
    if (ProgressBar_Matchmaking)
    {
        float Progress = Status == EHSMatchmakingStatus::Searching ? 0.5f : 0.0f;
        ProgressBar_Matchmaking->SetPercent(Progress);
    }
}

void UHSMainMenuWidget::UpdateSettingsUI()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    // 그래픽 설정 UI 업데이트
    if (CurrentSettingsCategory == EHSSettingsCategory::Graphics)
    {
        if (ComboBox_OverallQuality)
        {
            FString QualityText = FString::Printf(TEXT("%d"), (int32)CurrentSaveData->GraphicsSettings.OverallQuality);
            ComboBox_OverallQuality->SetSelectedOption(QualityText);
        }
        
        if (CheckBox_Fullscreen)
        {
            CheckBox_Fullscreen->SetIsChecked(CurrentSaveData->GraphicsSettings.bFullscreenMode);
        }
        
        if (CheckBox_VSync)
        {
            CheckBox_VSync->SetIsChecked(CurrentSaveData->GraphicsSettings.bVSyncEnabled);
        }
        
        if (Slider_FrameRate)
        {
            Slider_FrameRate->SetValue(CurrentSaveData->GraphicsSettings.FrameRateLimit);
        }
    }
    
    // 오디오 설정 UI 업데이트
    if (CurrentSettingsCategory == EHSSettingsCategory::Audio)
    {
        if (Slider_MasterVolume)
        {
            Slider_MasterVolume->SetValue(CurrentSaveData->AudioSettings.MasterVolume);
        }
        
        if (Slider_SFXVolume)
        {
            Slider_SFXVolume->SetValue(CurrentSaveData->AudioSettings.SFXVolume);
        }
        
        if (Slider_MusicVolume)
        {
            Slider_MusicVolume->SetValue(CurrentSaveData->AudioSettings.MusicVolume);
        }
    }
    
    // 입력 설정 UI 업데이트
    if (CurrentSettingsCategory == EHSSettingsCategory::Input)
    {
        if (Slider_MouseSensitivity)
        {
            Slider_MouseSensitivity->SetValue(CurrentSaveData->InputSettings.MouseSensitivity);
        }
    }
}

void UHSMainMenuWidget::UpdateProfileUI()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    UpdateStatisticsDisplay();
}

void UHSMainMenuWidget::LoadSettingsFromSaveData()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    // 백업 생성 (취소 시 복원용)
    BackupGraphicsSettings = CurrentSaveData->GraphicsSettings;
    BackupAudioSettings = CurrentSaveData->AudioSettings;
    BackupInputSettings = CurrentSaveData->InputSettings;
    BackupGameplaySettings = CurrentSaveData->GameplaySettings;
    BackupNetworkSettings = CurrentSaveData->NetworkSettings;
    BackupAccessibilitySettings = CurrentSaveData->AccessibilitySettings;
}

void UHSMainMenuWidget::SaveSettingsToSaveData()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    // 실제 구현에서는 저장 시스템을 통해 디스크에 저장
    CurrentSaveData->UpdateSaveDate();
}

void UHSMainMenuWidget::ApplyCurrentSettings()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    // 각 카테고리별 설정 적용
    CurrentSaveData->ApplyGraphicsSettings();
    CurrentSaveData->ApplyAudioSettings();
    CurrentSaveData->ApplyInputSettings();
    CurrentSaveData->ApplyGameplaySettings();
    CurrentSaveData->ApplyNetworkSettings();
    CurrentSaveData->ApplyAccessibilitySettings();
}

void UHSMainMenuWidget::PopulateQualityComboBox()
{
    if (!ComboBox_OverallQuality)
    {
        return;
    }
    
    ComboBox_OverallQuality->ClearOptions();
    ComboBox_OverallQuality->AddOption(TEXT("Low"));
    ComboBox_OverallQuality->AddOption(TEXT("Medium"));
    ComboBox_OverallQuality->AddOption(TEXT("High"));
    ComboBox_OverallQuality->AddOption(TEXT("Epic"));
    ComboBox_OverallQuality->AddOption(TEXT("Ultra"));
}

void UHSMainMenuWidget::PopulateResolutionComboBox()
{
    if (!ComboBox_Resolution)
    {
        return;
    }
    
    ComboBox_Resolution->ClearOptions();
    ComboBox_Resolution->AddOption(TEXT("1920x1080"));
    ComboBox_Resolution->AddOption(TEXT("2560x1440"));
    ComboBox_Resolution->AddOption(TEXT("3840x2160"));
    ComboBox_Resolution->AddOption(TEXT("1366x768"));
    ComboBox_Resolution->AddOption(TEXT("1600x900"));
}

void UHSMainMenuWidget::UpdateStatisticsDisplay()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    const FHSPlayerLifetimeStatistics& PlayerStats = CurrentSaveData->PlayerProfile.Statistics;
    
    // 플레이어 기본 정보
    if (Text_PlayerName)
    {
        Text_PlayerName->SetText(FText::FromString(CurrentSaveData->PlayerProfile.PlayerName));
    }
    
    if (Text_PlayerLevel)
    {
        FString LevelText = FString::Printf(TEXT("레벨 %d"), CurrentSaveData->PlayerProfile.PlayerLevel);
        Text_PlayerLevel->SetText(FText::FromString(LevelText));
    }
    
    // 플레이 시간
    if (Text_TotalPlayTime)
    {
        FString PlayTimeText = FormatPlayTime(PlayerStats.TotalPlayTime);
        Text_TotalPlayTime->SetText(FText::FromString(PlayTimeText));
    }
    
    // 총 런 수
    if (Text_TotalRuns)
    {
        FString RunsText = FString::Printf(TEXT("총 런: %d"), PlayerStats.TotalRuns);
        Text_TotalRuns->SetText(FText::FromString(RunsText));
    }
    
    // 성공률
    if (Text_SuccessRate)
    {
        FString SuccessRateText = FormatSuccessRate(PlayerStats.SuccessfulRuns, PlayerStats.TotalRuns);
        Text_SuccessRate->SetText(FText::FromString(SuccessRateText));
    }
}

FString UHSMainMenuWidget::FormatPlayTime(int32 TotalSeconds) const
{
    int32 Hours = TotalSeconds / 3600;
    int32 Minutes = (TotalSeconds % 3600) / 60;
    
    if (Hours > 0)
    {
        return FString::Printf(TEXT("%d시간 %d분"), Hours, Minutes);
    }
    else
    {
        return FString::Printf(TEXT("%d분"), Minutes);
    }
}

FString UHSMainMenuWidget::FormatSuccessRate(int32 Successful, int32 Total) const
{
    if (Total == 0)
    {
        return TEXT("성공률: 0%");
    }
    
    float Rate = (float)Successful / (float)Total * 100.0f;
    return FString::Printf(TEXT("성공률: %.1f%%"), Rate);
}

void UHSMainMenuWidget::ShowErrorDialog(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("HSMainMenuWidget: 에러 다이얼로그 - %s"), *ErrorMessage);
    
    // 실제 구현에서는 에러 다이얼로그 UI 표시
    // 현재는 로그로만 출력
}

// 버튼 클릭 이벤트 핸들러들
void UHSMainMenuWidget::OnQuickMatchClicked()
{
    StartQuickMatch();
}

void UHSMainMenuWidget::OnRankedMatchClicked()
{
    StartRankedMatch();
}

void UHSMainMenuWidget::OnCustomMatchClicked()
{
    StartCustomMatch();
}

void UHSMainMenuWidget::OnSettingsClicked()
{
    ShowSettings();
}

void UHSMainMenuWidget::OnProfileClicked()
{
    ShowProfile();
}

void UHSMainMenuWidget::OnQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UHSMainMenuWidget::OnCancelMatchmakingClicked()
{
    CancelMatchmaking();
}

void UHSMainMenuWidget::OnAcceptMatchClicked()
{
    AcceptMatch();
}

void UHSMainMenuWidget::OnDeclineMatchClicked()
{
    DeclineMatch();
}

// 매치메이킹 이벤트 콜백들
void UHSMainMenuWidget::OnMatchmakingStatusChanged(EHSMatchmakingStatus NewStatus)
{
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 매치메이킹 상태 변경 - %d"), (int32)NewStatus);
    
    if (NewStatus == EHSMatchmakingStatus::MatchFound)
    {
        SetMenuState(EHSMenuState::MatchFound);
    }
    else if (NewStatus == EHSMatchmakingStatus::Error || NewStatus == EHSMatchmakingStatus::NotSearching)
    {
        SetMenuState(EHSMenuState::MainMenu);
    }
    
    UpdateMatchmakingUI();
}

void UHSMainMenuWidget::OnMatchFound(const FHSMatchInfo& MatchInfo)
{
    UE_LOG(LogTemp, Log, TEXT("HSMainMenuWidget: 매치 발견 - %s"), *MatchInfo.MatchID);
    
    // 매치 정보 UI 업데이트
    if (Text_MatchInfo)
    {
        FString InfoText = FString::Printf(
            TEXT("매치 ID: %s\n플레이어: %d/%d\n핑: %dms"), 
            *MatchInfo.MatchID, 
            MatchInfo.CurrentPlayers, 
            MatchInfo.MaxPlayers, 
            MatchInfo.PingMs
        );
        Text_MatchInfo->SetText(FText::FromString(InfoText));
    }
}

void UHSMainMenuWidget::OnMatchmakingError(const FString& ErrorMessage)
{
    ShowErrorDialog(FString::Printf(TEXT("매치메이킹 오류: %s"), *ErrorMessage));
    SetMenuState(EHSMenuState::MainMenu);
}

void UHSMainMenuWidget::OnEstimatedWaitTimeUpdated(float WaitTimeSeconds)
{
    // UI에서 대기시간 업데이트는 UpdateMatchmakingUI에서 처리됨
}

// 설정 변경 콜백들
void UHSMainMenuWidget::OnOverallQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    if (SelectedItem == TEXT("Low"))
    {
        CurrentSaveData->GraphicsSettings.OverallQuality = EHSQualityLevel::Low;
    }
    else if (SelectedItem == TEXT("Medium"))
    {
        CurrentSaveData->GraphicsSettings.OverallQuality = EHSQualityLevel::Medium;
    }
    else if (SelectedItem == TEXT("High"))
    {
        CurrentSaveData->GraphicsSettings.OverallQuality = EHSQualityLevel::High;
    }
    else if (SelectedItem == TEXT("Epic"))
    {
        CurrentSaveData->GraphicsSettings.OverallQuality = EHSQualityLevel::Epic;
    }
    else if (SelectedItem == TEXT("Ultra"))
    {
        CurrentSaveData->GraphicsSettings.OverallQuality = EHSQualityLevel::Ultra;
    }
}

void UHSMainMenuWidget::OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    if (SelectedItem == TEXT("1920x1080"))
    {
        CurrentSaveData->GraphicsSettings.ScreenResolution = FIntPoint(1920, 1080);
    }
    else if (SelectedItem == TEXT("2560x1440"))
    {
        CurrentSaveData->GraphicsSettings.ScreenResolution = FIntPoint(2560, 1440);
    }
    else if (SelectedItem == TEXT("3840x2160"))
    {
        CurrentSaveData->GraphicsSettings.ScreenResolution = FIntPoint(3840, 2160);
    }
    // 추가 해상도들...
}

void UHSMainMenuWidget::OnFullscreenChanged(bool bIsChecked)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->GraphicsSettings.bFullscreenMode = bIsChecked;
    }
}

void UHSMainMenuWidget::OnVSyncChanged(bool bIsChecked)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->GraphicsSettings.bVSyncEnabled = bIsChecked;
    }
}

void UHSMainMenuWidget::OnFrameRateChanged(float Value)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->GraphicsSettings.FrameRateLimit = Value;
    }
}

void UHSMainMenuWidget::OnMasterVolumeChanged(float Value)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->AudioSettings.MasterVolume = Value;
    }
}

void UHSMainMenuWidget::OnSFXVolumeChanged(float Value)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->AudioSettings.SFXVolume = Value;
    }
}

void UHSMainMenuWidget::OnMusicVolumeChanged(float Value)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->AudioSettings.MusicVolume = Value;
    }
}

void UHSMainMenuWidget::OnMouseSensitivityChanged(float Value)
{
    if (CurrentSaveData)
    {
        CurrentSaveData->InputSettings.MouseSensitivity = Value;
    }
}

void UHSMainMenuWidget::PlayWidgetAnimation(UWidget* TargetWidget, const FString& AnimationType, float Duration)
{
    if (!TargetWidget)
    {
        return;
    }
    
    bIsAnimating = true;
    
    // 실제 구현에서는 UMG 애니메이션 시스템 사용
    // 현재는 시뮬레이션
    if (GetWorld())
    {
        FTimerHandle AnimationTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            AnimationTimerHandle,
            [this]() { OnAnimationFinished(); },
            Duration,
            false
        );
    }
}

void UHSMainMenuWidget::OnAnimationFinished()
{
    bIsAnimating = false;
}

void UHSMainMenuWidget::StartMatchAcceptanceTimer()
{
    MatchAcceptanceTimeRemaining = MatchAcceptanceTimeoutSeconds;
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            MatchAcceptanceTimerHandle,
            this,
            &UHSMainMenuWidget::OnMatchAcceptanceTimeout,
            MatchAcceptanceTimeoutSeconds,
            false
        );
    }
}

void UHSMainMenuWidget::UpdateMatchAcceptanceTimer()
{
    float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
    MatchAcceptanceTimeRemaining -= DeltaTime;
    
    if (ProgressBar_MatchAcceptance)
    {
        float Progress = MatchAcceptanceTimeRemaining / MatchAcceptanceTimeoutSeconds;
        ProgressBar_MatchAcceptance->SetPercent(Progress);
    }
}

void UHSMainMenuWidget::OnMatchAcceptanceTimeout()
{
    DeclineMatch();
}

bool UHSMainMenuWidget::ValidateSettings() const
{
    if (!CurrentSaveData)
    {
        return false;
    }
    
    // 기본적인 설정 검증
    if (CurrentSaveData->GraphicsSettings.FrameRateLimit < 30.0f || 
        CurrentSaveData->GraphicsSettings.FrameRateLimit > 300.0f)
    {
        return false;
    }
    
    if (CurrentSaveData->AudioSettings.MasterVolume < 0.0f || 
        CurrentSaveData->AudioSettings.MasterVolume > 1.0f)
    {
        return false;
    }
    
    return true;
}

void UHSMainMenuWidget::RestoreBackupSettings()
{
    if (!CurrentSaveData)
    {
        return;
    }
    
    CurrentSaveData->GraphicsSettings = BackupGraphicsSettings;
    CurrentSaveData->AudioSettings = BackupAudioSettings;
    CurrentSaveData->InputSettings = BackupInputSettings;
    CurrentSaveData->GameplaySettings = BackupGameplaySettings;
    CurrentSaveData->NetworkSettings = BackupNetworkSettings;
    CurrentSaveData->AccessibilitySettings = BackupAccessibilitySettings;
}
