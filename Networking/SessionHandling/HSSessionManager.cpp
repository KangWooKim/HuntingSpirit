// 사냥의 영혼(HuntingSpirit) 게임의 세션 관리자 구현
// 게임 세션의 생성, 참여, 관리를 담당하는 클래스 구현

#include "HSSessionManager.h"
#include "HuntingSpirit/Cooperation/HSTeamManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"

// 우리 게임에서 사용할 세션 이름
const FName HSGameSessionName = TEXT("HuntingSpiritGameSession");

// 생성자
UHSSessionManager::UHSSessionManager()
{
    // 기본값 설정
    CurrentSessionState = EHSSessionState::SS_None;
    CurrentSessionInfo = FHSSessionInfo();
    bIsSessionHost = false;
    LastSearchResults.Empty();

    // 설정 변수 기본값
    DefaultCreateSettings = FHSSessionCreateSettings();
    DefaultSearchFilter = FHSSessionSearchFilter();
    bAutoReconnectEnabled = true;
    MaxReconnectRetries = 3;
    SessionHeartbeatInterval = 30.0f; // 30초마다
    ConnectionTimeout = 60.0f; // 60초 타임아웃

    // 런타임 변수 초기화
    OnlineSubsystem = nullptr;
    SessionInterface = nullptr;
    CurrentSessionSearch = nullptr;
    CurrentReconnectAttempts = 0;
    bIsInitialized = false;
}

// 초기화
void UHSSessionManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    InitializeOnlineSubsystem();
    
    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 매니저 초기화 완료"));
}

// 정리
void UHSSessionManager::Deinitialize()
{
    // 활성 세션이 있다면 정리
    if (IsInSession())
    {
        LeaveSession();
    }

    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(SessionHeartbeatTimer);
        TimerManager.ClearTimer(ReconnectTimer);
        TimerManager.ClearTimer(ConnectionTimeoutTimer);
        TimerManager.ClearTimer(SessionCleanupTimer);
    }

    // 델리게이트 해제
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
        SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(OnEndSessionCompleteDelegateHandle);
    }

    OnlineSubsystem = nullptr;
    SessionInterface = nullptr;

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 매니저 정리 완료"));

    Super::Deinitialize();
}

// === 세션 생성 및 관리 ===

// 세션 생성
bool UHSSessionManager::CreateSession(const FHSSessionCreateSettings& CreateSettings)
{
    if (!bIsInitialized || !SessionInterface.IsValid())
    {
        HandleSessionError(TEXT("Session interface not available"));
        return false;
    }

    if (CurrentSessionState != EHSSessionState::SS_None)
    {
        HandleSessionError(TEXT("Already in a session or processing"));
        return false;
    }

    ChangeSessionState(EHSSessionState::SS_Creating);

    // 기존 세션이 있다면 제거
    if (SessionInterface->GetNamedSession(HSGameSessionName))
    {
        SessionInterface->DestroySession(HSGameSessionName);
    }

    // 세션 설정 변환
    TSharedPtr<FOnlineSessionSettings> SessionSettings = ConvertToOnlineSessionSettings(CreateSettings);
    if (!SessionSettings.IsValid())
    {
        HandleSessionError(TEXT("Failed to create session settings"));
        return false;
    }

    // 로컬 플레이어 ID 가져오기
    const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        HandleSessionError(TEXT("No valid local player found"));
        return false;
    }

    // 세션 생성 시작
    if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), HSGameSessionName, *SessionSettings))
    {
        HandleSessionError(TEXT("Failed to start session creation"));
        ChangeSessionState(EHSSessionState::SS_None);
        return false;
    }

    // 현재 세션 정보 업데이트
    CurrentSessionInfo.SessionName = CreateSettings.SessionName;
    CurrentSessionInfo.HostName = LocalPlayer->GetNickname();
    CurrentSessionInfo.MapName = CreateSettings.MapName;
    CurrentSessionInfo.GameMode = CreateSettings.GameMode;
    CurrentSessionInfo.SessionType = CreateSettings.SessionType;
    CurrentSessionInfo.MaxPlayers = CreateSettings.MaxPlayers;
    CurrentSessionInfo.CurrentPlayers = 1; // 호스트

    bIsSessionHost = true;

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 생성 시작 - %s"), *CreateSettings.SessionName);
    return true;
}

// 세션 파괴
bool UHSSessionManager::DestroySession()
{
    if (!SessionInterface.IsValid())
    {
        return false;
    }

    if (CurrentSessionState != EHSSessionState::SS_InSession)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSessionManager: 파괴할 세션이 없습니다"));
        return false;
    }

    ChangeSessionState(EHSSessionState::SS_Destroying);

    if (!SessionInterface->DestroySession(NAME_GameSession))
    {
        HandleSessionError(TEXT("Failed to destroy session"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 파괴 시작"));
    return true;
}

// 세션 탈퇴
bool UHSSessionManager::LeaveSession()
{
    if (CurrentSessionState != EHSSessionState::SS_InSession)
    {
        return false;
    }

    ChangeSessionState(EHSSessionState::SS_Leaving);

    // 호스트인 경우 세션 파괴, 아니면 단순 탈퇴
    if (bIsSessionHost)
    {
        return DestroySession();
    }
    else
    {
        // 클라이언트는 단순히 연결 해제
        if (UWorld* World = GetWorld())
        {
            World->GetFirstPlayerController()->ClientTravel(TEXT("/Game/Maps/MainMenu"), ETravelType::TRAVEL_Absolute);
        }
        
        ChangeSessionState(EHSSessionState::SS_None);
        return true;
    }
}

// 세션 시작
bool UHSSessionManager::StartSession()
{
    if (!SessionInterface.IsValid() || !bIsSessionHost)
    {
        return false;
    }

    if (CurrentSessionState != EHSSessionState::SS_InSession)
    {
        return false;
    }

    if (!SessionInterface->StartSession(NAME_GameSession))
    {
        HandleSessionError(TEXT("Failed to start session"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 시작"));
    return true;
}

// 세션 종료
bool UHSSessionManager::EndSession()
{
    if (!SessionInterface.IsValid() || !bIsSessionHost)
    {
        return false;
    }

    if (!SessionInterface->EndSession(NAME_GameSession))
    {
        HandleSessionError(TEXT("Failed to end session"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 종료"));
    return true;
}

// === 세션 검색 및 참여 ===

// 세션 검색
bool UHSSessionManager::SearchSessions(const FHSSessionSearchFilter& SearchFilter)
{
    if (!SessionInterface.IsValid())
    {
        HandleSessionError(TEXT("Session interface not available"));
        return false;
    }

    if (CurrentSessionState == EHSSessionState::SS_Searching)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSessionManager: 이미 검색 중입니다"));
        return false;
    }

    ChangeSessionState(EHSSessionState::SS_Searching);

    // 세션 검색 설정 생성
    CurrentSessionSearch = MakeShareable(new FOnlineSessionSearch());
    CurrentSessionSearch->MaxSearchResults = SearchFilter.MaxSearchResults;
    CurrentSessionSearch->bIsLanQuery = SearchFilter.bSearchLAN;

    // 검색 필터 적용
    ApplySearchFilter(SearchFilter, CurrentSessionSearch);

    // 로컬 플레이어 ID 가져오기
    const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        HandleSessionError(TEXT("No valid local player found"));
        return false;
    }

    // 세션 검색 시작
    if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), CurrentSessionSearch.ToSharedRef()))
    {
        HandleSessionError(TEXT("Failed to start session search"));
        ChangeSessionState(EHSSessionState::SS_None);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 검색 시작 (최대 %d개)"), SearchFilter.MaxSearchResults);
    return true;
}

// 세션 참여
bool UHSSessionManager::JoinSession(const FHSSessionInfo& SessionInfo)
{
    if (!SessionInterface.IsValid())
    {
        HandleSessionError(TEXT("Session interface not available"));
        return false;
    }

    if (CurrentSessionState != EHSSessionState::SS_None)
    {
        HandleSessionError(TEXT("Already in session or processing"));
        return false;
    }

    if (!SessionInfo.SearchResult.IsValid())
    {
        HandleSessionError(TEXT("Invalid session search result"));
        return false;
    }

    ChangeSessionState(EHSSessionState::SS_Joining);

    // 로컬 플레이어 ID 가져오기
    const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        HandleSessionError(TEXT("No valid local player found"));
        return false;
    }

    // 세션 참여 시작
    if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *SessionInfo.SearchResult))
    {
        HandleSessionError(TEXT("Failed to join session"));
        ChangeSessionState(EHSSessionState::SS_None);
        return false;
    }

    // 현재 세션 정보 업데이트
    CurrentSessionInfo = SessionInfo;
    bIsSessionHost = false;

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 참여 시작 - %s"), *SessionInfo.SessionName);
    return true;
}

// 인덱스로 세션 참여
bool UHSSessionManager::JoinSessionByIndex(int32 SessionIndex)
{
    if (!LastSearchResults.IsValidIndex(SessionIndex))
    {
        HandleSessionError(FString::Printf(TEXT("Invalid session index: %d"), SessionIndex));
        return false;
    }

    return JoinSession(LastSearchResults[SessionIndex]);
}

// 빠른 매칭
bool UHSSessionManager::QuickMatch(const FHSSessionSearchFilter& SearchFilter)
{
    if (!SearchSessions(SearchFilter))
    {
        return false;
    }

    // 검색 완료 시 자동으로 첫 번째 세션에 참여하도록 플래그 설정
    // 실제 구현에서는 OnSessionSearchCompleted에서 처리
    
    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 빠른 매칭 시작"));
    return true;
}

// === 세션 정보 조회 ===

// 세션 플레이어 수 가져오기
int32 UHSSessionManager::GetSessionPlayerCount() const
{
    if (!SessionInterface.IsValid() || !IsInSession())
    {
        return 0;
    }

    const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session)
    {
        return 0;
    }

    // UE5에서는 세션 설정의 연결 정보로 플레이어 수를 계산
    int32 MaxConnections = Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections;
    int32 OpenConnections = Session->NumOpenPublicConnections + Session->NumOpenPrivateConnections;
    
    return FMath::Max(0, MaxConnections - OpenConnections);
}

// 세션 최대 플레이어 수 가져오기
int32 UHSSessionManager::GetSessionMaxPlayers() const
{
    if (!SessionInterface.IsValid() || !IsInSession())
    {
        return 0;
    }

    const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session)
    {
        return 0;
    }

    return Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections;
}

// === 세션 설정 관리 ===

// 세션 설정 업데이트
bool UHSSessionManager::UpdateSessionSetting(const FString& SettingKey, const FString& SettingValue)
{
    if (!SessionInterface.IsValid() || !bIsSessionHost)
    {
        return false;
    }

    FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session)
    {
        return false;
    }

    // 세션 설정 업데이트
    Session->SessionSettings.Set(FName(*SettingKey), SettingValue, EOnlineDataAdvertisementType::ViaOnlineService);
    
    // 세션 업데이트 (UE5에서는 UpdateSession 사용)
    SessionInterface->UpdateSession(NAME_GameSession, Session->SessionSettings);

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 설정 업데이트 - %s: %s"), *SettingKey, *SettingValue);
    return true;
}

// 세션 설정 가져오기
FString UHSSessionManager::GetSessionSetting(const FString& SettingKey) const
{
    if (!SessionInterface.IsValid())
    {
        return FString();
    }

    const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session)
    {
        return FString();
    }

    FString SettingValue;
    if (Session->SessionSettings.Get(FName(*SettingKey), SettingValue))
    {
        return SettingValue;
    }

    return FString();
}

// 최대 플레이어 수 변경
bool UHSSessionManager::ChangeMaxPlayers(int32 NewMaxPlayers)
{
    if (!SessionInterface.IsValid() || !bIsSessionHost)
    {
        return false;
    }

    FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session)
    {
        return false;
    }

    // 현재 플레이어 수보다 작게 설정할 수 없음
    int32 MaxConnections = Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections;
    int32 OpenConnections = Session->NumOpenPublicConnections + Session->NumOpenPrivateConnections;
    int32 CurrentPlayers = FMath::Max(0, MaxConnections - OpenConnections);
    
    if (NewMaxPlayers < CurrentPlayers)
    {
        HandleSessionError(FString::Printf(TEXT("Cannot set max players (%d) below current players (%d)"), NewMaxPlayers, CurrentPlayers));
        return false;
    }

    Session->SessionSettings.NumPublicConnections = NewMaxPlayers;
    Session->SessionSettings.NumPrivateConnections = 0;
    SessionInterface->UpdateSession(NAME_GameSession, Session->SessionSettings);

    CurrentSessionInfo.MaxPlayers = NewMaxPlayers;

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 최대 플레이어 수 변경 - %d"), NewMaxPlayers);
    return true;
}

// === 플레이어 관리 ===

// 플레이어 추방
bool UHSSessionManager::KickPlayer(const FString& PlayerName)
{
    if (!SessionInterface.IsValid() || !bIsSessionHost)
    {
        return false;
    }

    // 실제 구현에서는 해당 플레이어의 연결을 끊고 세션에서 제거
    // UE5의 온라인 서브시스템을 통해 처리
    
    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 플레이어 추방 - %s"), *PlayerName);
    return true;
}

// 플레이어 금지
bool UHSSessionManager::BanPlayer(const FString& PlayerName)
{
    if (!bIsSessionHost)
    {
        return false;
    }

    // 금지 목록에 추가
    if (!BannedPlayers.Contains(PlayerName))
    {
        BannedPlayers.Add(PlayerName);
    }

    // 현재 세션에 있다면 추방
    KickPlayer(PlayerName);

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 플레이어 금지 - %s"), *PlayerName);
    return true;
}

// 세션 플레이어 이름 목록 가져오기
TArray<FString> UHSSessionManager::GetSessionPlayerNames() const
{
    TArray<FString> PlayerNames;

    if (!SessionInterface.IsValid() || !IsInSession())
    {
        return PlayerNames;
    }

    const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session)
    {
        return PlayerNames;
    }

    // UE5에서는 세션 설정의 연결 정보로 플레이어 수를 계산
    int32 MaxConnections = Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections;
    int32 OpenConnections = Session->NumOpenPublicConnections + Session->NumOpenPrivateConnections;
    int32 NumPlayers = FMath::Max(0, MaxConnections - OpenConnections);
    
    for (int32 i = 0; i < NumPlayers; ++i)
    {
        // 실제 구현에서는 더 정확한 플레이어 정보 수집 필요
        PlayerNames.Add(FString::Printf(TEXT("Player_%d"), i + 1));
    }

    return PlayerNames;
}

// === 네트워크 진단 ===

// 세션 연결 품질 가져오기
int32 UHSSessionManager::GetSessionConnectionQuality() const
{
    if (!IsInSession())
    {
        return 0;
    }

    // 핑과 패킷 손실률을 기반으로 연결 품질 계산
    int32 Ping = GetSessionPing();
    
    if (Ping < 50)
        return 4; // 매우 좋음
    else if (Ping < 100)
        return 3; // 좋음
    else if (Ping < 200)
        return 2; // 보통
    else if (Ping < 300)
        return 1; // 나쁨
    else
        return 0; // 매우 나쁨
}

// 세션 핑 가져오기
int32 UHSSessionManager::GetSessionPing() const
{
    if (!IsInSession())
    {
        return 999;
    }

    // 실제 구현에서는 네트워크 상태를 통해 핑을 계산
    // 여기서는 간소화된 구현
    return CurrentSessionInfo.Ping;
}

// 네트워크 통계 문자열 가져오기
FString UHSSessionManager::GetNetworkStatsString() const
{
    return FString::Printf(TEXT("Session: %s | Players: %d/%d | Ping: %dms | Quality: %d/4"),
        *CurrentSessionInfo.SessionName,
        GetSessionPlayerCount(),
        GetSessionMaxPlayers(),
        GetSessionPing(),
        GetSessionConnectionQuality()
    );
}

// === 유틸리티 함수들 ===

// 자동 재연결 설정
void UHSSessionManager::SetAutoReconnect(bool bEnable, int32 MaxRetries)
{
    bAutoReconnectEnabled = bEnable;
    MaxReconnectRetries = FMath::Max(0, MaxRetries);
    
    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 자동 재연결 %s (최대 %d회)"), 
           bEnable ? TEXT("활성화") : TEXT("비활성화"), MaxRetries);
}

// 세션 매니저 상태 문자열 가져오기
FString UHSSessionManager::GetSessionManagerStatusString() const
{
    return FString::Printf(TEXT("State: %d | Host: %s | Players: %d | Reconnects: %d/%d"),
        (int32)CurrentSessionState,
        bIsSessionHost ? TEXT("Yes") : TEXT("No"),
        GetSessionPlayerCount(),
        CurrentReconnectAttempts,
        MaxReconnectRetries
    );
}

// === 프라이빗 함수들 ===

// 온라인 서브시스템 초기화
void UHSSessionManager::InitializeOnlineSubsystem()
{
    OnlineSubsystem = IOnlineSubsystem::Get();
    if (!OnlineSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 온라인 서브시스템을 찾을 수 없습니다"));
        return;
    }

    SessionInterface = OnlineSubsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 인터페이스를 가져올 수 없습니다"));
        return;
    }

    // 델리게이트 바인딩
    OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UHSSessionManager::OnCreateSessionComplete);
    OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UHSSessionManager::OnStartSessionComplete);
    OnFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHSSessionManager::OnFindSessionsComplete);
    OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHSSessionManager::OnJoinSessionComplete);
    OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHSSessionManager::OnDestroySessionComplete);
    OnEndSessionCompleteDelegate = FOnEndSessionCompleteDelegate::CreateUObject(this, &UHSSessionManager::OnEndSessionComplete);

    // 델리게이트 등록
    OnCreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
    OnStartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);
    OnFindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);
    OnJoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
    OnDestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
    OnEndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(OnEndSessionCompleteDelegate);

    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 온라인 서브시스템 초기화 완료 - %s"), 
           *OnlineSubsystem->GetSubsystemName().ToString());
}

// 세션 상태 변경
void UHSSessionManager::ChangeSessionState(EHSSessionState NewState)
{
    FScopeLock StateLock(&SessionStateMutex);
    
    if (CurrentSessionState != NewState)
    {
        EHSSessionState OldState = CurrentSessionState;
        CurrentSessionState = NewState;
        
        // 이벤트 브로드캐스트
        OnSessionStateChanged.Broadcast(NewState);
        
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 상태 변경 - %d -> %d"), (int32)OldState, (int32)NewState);
    }
}

// 세션 설정 변환
TSharedPtr<FOnlineSessionSettings> UHSSessionManager::ConvertToOnlineSessionSettings(const FHSSessionCreateSettings& CreateSettings) const
{
    TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
    
    SessionSettings->NumPublicConnections = CreateSettings.MaxPlayers;
    SessionSettings->NumPrivateConnections = 0;
    SessionSettings->bIsLANMatch = CreateSettings.bIsLANMatch;
    SessionSettings->bIsDedicated = (CreateSettings.SessionType == EHSSessionType::ST_Dedicated);
    SessionSettings->bUsesStats = true;
    SessionSettings->bAllowInvites = !CreateSettings.bIsInviteOnly;
    SessionSettings->bAllowJoinInProgress = true;
    SessionSettings->bAllowJoinViaPresence = CreateSettings.bIsPublic;
    SessionSettings->bShouldAdvertise = CreateSettings.bIsPublic;
    SessionSettings->bAntiCheatProtected = true;

    // 커스텀 설정 추가
    SessionSettings->Set(TEXT("SESSION_NAME"), CreateSettings.SessionName, EOnlineDataAdvertisementType::ViaOnlineService);
    SessionSettings->Set(TEXT("MAP_NAME"), CreateSettings.MapName, EOnlineDataAdvertisementType::ViaOnlineService);
    SessionSettings->Set(TEXT("GAME_MODE"), CreateSettings.GameMode, EOnlineDataAdvertisementType::ViaOnlineService);
    SessionSettings->Set(TEXT("SESSION_TYPE"), (int32)CreateSettings.SessionType, EOnlineDataAdvertisementType::ViaOnlineService);

    // 비밀번호가 있는 경우
    if (!CreateSettings.Password.IsEmpty())
    {
        SessionSettings->Set(TEXT("PASSWORD"), CreateSettings.Password, EOnlineDataAdvertisementType::ViaOnlineService);
    }

    // 커스텀 설정 추가
    for (const auto& CustomSetting : CreateSettings.CustomSettings)
    {
        SessionSettings->Set(FName(*CustomSetting.Key), CustomSetting.Value, EOnlineDataAdvertisementType::ViaOnlineService);
    }

    return SessionSettings;
}

// 검색 필터 적용
void UHSSessionManager::ApplySearchFilter(const FHSSessionSearchFilter& Filter, TSharedPtr<FOnlineSessionSearch> SessionSearch) const
{
    if (!SessionSearch.IsValid())
    {
        return;
    }

    // 기본 필터 설정
    SessionSearch->bIsLanQuery = Filter.bSearchLAN;
    SessionSearch->MaxSearchResults = Filter.MaxSearchResults;

    // 게임 모드 필터
    if (!Filter.GameMode.IsEmpty())
    {
        SessionSearch->QuerySettings.Set(TEXT("GAME_MODE"), Filter.GameMode, EOnlineComparisonOp::Equals);
    }

    // 맵 필터
    if (!Filter.MapName.IsEmpty())
    {
        SessionSearch->QuerySettings.Set(TEXT("MAP_NAME"), Filter.MapName, EOnlineComparisonOp::Equals);
    }

    // 공개 세션만 검색
    if (Filter.bPublicOnly)
    {
        SessionSearch->QuerySettings.Set(FName("SEARCH_PRESENCE"), true, EOnlineComparisonOp::Equals);
    }

    // 비어있지 않은 세션만 검색
    if (Filter.bNonEmptyOnly)
    {
        SessionSearch->QuerySettings.Set(FName("SEARCH_EMPTY_SERVERS_ONLY"), false, EOnlineComparisonOp::Equals);
    }

    // 가득 찬 세션 제외
    if (Filter.bExcludeFullSessions)
    {
        SessionSearch->QuerySettings.Set(FName("SEARCH_EXCLUDE_UNIQUEID_TYPE"), true, EOnlineComparisonOp::Equals);
    }

    // 커스텀 필터 적용
    for (const auto& CustomFilter : Filter.CustomFilters)
    {
        SessionSearch->QuerySettings.Set(FName(*CustomFilter.Key), CustomFilter.Value, EOnlineComparisonOp::Equals);
    }
}

// 검색 결과 변환
FHSSessionInfo UHSSessionManager::ConvertFromSearchResult(const FOnlineSessionSearchResult& SearchResult) const
{
    FHSSessionInfo SessionInfo;
    
    // 기본 정보 설정
    int32 MaxConnections = SearchResult.Session.SessionSettings.NumPublicConnections + SearchResult.Session.SessionSettings.NumPrivateConnections;
    // UE5에서는 세션 설정에서 사용 중인 연결 수를 계산
    int32 UsedConnections = MaxConnections - (SearchResult.Session.NumOpenPublicConnections + SearchResult.Session.NumOpenPrivateConnections);
    SessionInfo.CurrentPlayers = FMath::Max(0, UsedConnections);
    SessionInfo.MaxPlayers = MaxConnections;
    SessionInfo.Ping = SearchResult.PingInMs;
    SessionInfo.SearchResult = MakeShareable(&SearchResult);

    // 세션 설정에서 정보 추출
    FString SessionName, MapName, GameMode;
    int32 SessionTypeInt;

    if (SearchResult.Session.SessionSettings.Get(TEXT("SESSION_NAME"), SessionName))
    {
        SessionInfo.SessionName = SessionName;
    }

    if (SearchResult.Session.SessionSettings.Get(TEXT("MAP_NAME"), MapName))
    {
        SessionInfo.MapName = MapName;
    }

    if (SearchResult.Session.SessionSettings.Get(TEXT("GAME_MODE"), GameMode))
    {
        SessionInfo.GameMode = GameMode;
    }

    if (SearchResult.Session.SessionSettings.Get(TEXT("SESSION_TYPE"), SessionTypeInt))
    {
        SessionInfo.SessionType = (EHSSessionType)SessionTypeInt;
    }

    // 호스트 이름 (실제 구현에서는 OwningUserId를 통해 조회)
    SessionInfo.HostName = TEXT("Unknown Host");

    // 세션 설정 저장
    for (const auto& Setting : SearchResult.Session.SessionSettings.Settings)
    {
        FString Value;
        Setting.Value.Data.GetValue(Value);
        SessionInfo.SessionSettings.Add(Setting.Key.ToString(), Value);
    }

    return SessionInfo;
}

// === 타이머 콜백 함수들 ===

// 세션 하트비트 처리
void UHSSessionManager::ProcessSessionHeartbeat()
{
    if (!IsInSession())
    {
        return;
    }

    // 세션 상태 유효성 검사
    if (!ValidateSessionIntegrity())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSessionManager: 세션 무결성 검사 실패"));
        HandleSessionError(TEXT("Session integrity check failed"));
    }

    // 세션 정보 업데이트
    CurrentSessionInfo.CurrentPlayers = GetSessionPlayerCount();
}

// 자동 재연결 시도
void UHSSessionManager::AttemptReconnect()
{
    if (!bAutoReconnectEnabled || CurrentReconnectAttempts >= MaxReconnectRetries)
    {
        HandleSessionError(TEXT("Maximum reconnection attempts reached"));
        return;
    }

    CurrentReconnectAttempts++;
    
    // 간단한 재연결 로직 (실제 구현에서는 마지막 세션 정보를 사용)
    UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 재연결 시도 %d/%d"), 
           CurrentReconnectAttempts, MaxReconnectRetries);
}

// 연결 타임아웃 처리
void UHSSessionManager::HandleConnectionTimeout()
{
    HandleSessionError(TEXT("Connection timeout"));
    
    if (bAutoReconnectEnabled)
    {
        AttemptReconnect();
    }
}

// 세션 정리
void UHSSessionManager::CleanupSession()
{
    // 오래된 검색 결과 제거
    LastSearchResults.Empty();
    
    // 금지된 플레이어 목록 정리 (예: 24시간 후 자동 해제)
    // 실제 구현에서는 시간 기반 정리 로직 필요
}

// === 온라인 서브시스템 콜백 함수들 ===

// 세션 생성 완료
void UHSSessionManager::OnCreateSessionComplete(FName SessionName, bool bSuccess)
{
    if (bSuccess)
    {
        ChangeSessionState(EHSSessionState::SS_InSession);
        OnSessionCreated.Broadcast(true, TEXT(""));
        
        // 세션 하트비트 시작
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(SessionHeartbeatTimer, this, 
                                            &UHSSessionManager::ProcessSessionHeartbeat, 
                                            SessionHeartbeatInterval, true);
        }
        
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 생성 완료"));
    }
    else
    {
        ChangeSessionState(EHSSessionState::SS_Error);
        OnSessionCreated.Broadcast(false, TEXT("Failed to create session"));
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 생성 실패"));
    }
}

// 세션 시작 완료
void UHSSessionManager::OnStartSessionComplete(FName SessionName, bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 시작 완료"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 시작 실패"));
    }
}

// 세션 검색 완료
void UHSSessionManager::OnFindSessionsComplete(bool bSuccess)
{
    if (bSuccess && CurrentSessionSearch.IsValid())
    {
        LastSearchResults.Empty();
        
        // 검색 결과를 내부 형식으로 변환
        for (const FOnlineSessionSearchResult& SearchResult : CurrentSessionSearch->SearchResults)
        {
            FHSSessionInfo SessionInfo = ConvertFromSearchResult(SearchResult);
            LastSearchResults.Add(SessionInfo);
        }
        
        ChangeSessionState(EHSSessionState::SS_None);
        OnSessionSearchCompleted.Broadcast(true, LastSearchResults);
        
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 검색 완료 - %d개 발견"), LastSearchResults.Num());
    }
    else
    {
        ChangeSessionState(EHSSessionState::SS_Error);
        OnSessionSearchCompleted.Broadcast(false, TArray<FHSSessionInfo>());
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 검색 실패"));
    }
}

// 세션 참여 완료
void UHSSessionManager::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        ChangeSessionState(EHSSessionState::SS_InSession);
        OnSessionJoined.Broadcast(true, TEXT(""));
        
        // 연결된 서버로 이동
        FString TravelURL;
        if (SessionInterface->GetResolvedConnectString(NAME_GameSession, TravelURL))
        {
            if (UWorld* World = GetWorld())
            {
                World->GetFirstPlayerController()->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
            }
        }
        
        // 재연결 시도 횟수 리셋
        CurrentReconnectAttempts = 0;
        
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 참여 완료"));
    }
    else
    {
        ChangeSessionState(EHSSessionState::SS_Error);
        FString ErrorMessage = FString::Printf(TEXT("Failed to join session: %d"), (int32)Result);
        OnSessionJoined.Broadcast(false, ErrorMessage);
        
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 참여 실패 - %d"), (int32)Result);
    }
}

// 세션 파괴 완료
void UHSSessionManager::OnDestroySessionComplete(FName SessionName, bool bSuccess)
{
    if (bSuccess)
    {
        ChangeSessionState(EHSSessionState::SS_None);
        OnSessionDestroyed.Broadcast(true, TEXT(""));
        
        // 세션 정보 리셋
        CurrentSessionInfo = FHSSessionInfo();
        bIsSessionHost = false;
        
        // 타이머 정리
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(SessionHeartbeatTimer);
        }
        
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 파괴 완료"));
    }
    else
    {
        ChangeSessionState(EHSSessionState::SS_Error);
        OnSessionDestroyed.Broadcast(false, TEXT("Failed to destroy session"));
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 파괴 실패"));
    }
}

// 세션 종료 완료
void UHSSessionManager::OnEndSessionComplete(FName SessionName, bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("HSSessionManager: 세션 종료 완료"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSSessionManager: 세션 종료 실패"));
    }
}

// === 디버그 및 로깅 함수들 ===

// 세션 상태 로그 출력
void UHSSessionManager::LogSessionState() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 세션 상태 ==="));
    UE_LOG(LogTemp, Warning, TEXT("상태: %d"), (int32)CurrentSessionState);
    UE_LOG(LogTemp, Warning, TEXT("호스트: %s"), bIsSessionHost ? TEXT("예") : TEXT("아니오"));
    UE_LOG(LogTemp, Warning, TEXT("세션명: %s"), *CurrentSessionInfo.SessionName);
    UE_LOG(LogTemp, Warning, TEXT("플레이어: %d/%d"), GetSessionPlayerCount(), GetSessionMaxPlayers());
    UE_LOG(LogTemp, Warning, TEXT("핑: %dms"), GetSessionPing());
    UE_LOG(LogTemp, Warning, TEXT("재연결 시도: %d/%d"), CurrentReconnectAttempts, MaxReconnectRetries);
}

// 검색 결과 로그 출력
void UHSSessionManager::LogSearchResults() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 세션 검색 결과 (%d개) ==="), LastSearchResults.Num());
    for (int32 i = 0; i < LastSearchResults.Num(); ++i)
    {
        const FHSSessionInfo& Session = LastSearchResults[i];
        UE_LOG(LogTemp, Warning, TEXT("[%d] %s | %s | %d/%d | %dms"), 
               i, *Session.SessionName, *Session.HostName, 
               Session.CurrentPlayers, Session.MaxPlayers, Session.Ping);
    }
}

// === 에러 처리 ===

// 세션 에러 처리
void UHSSessionManager::HandleSessionError(const FString& ErrorMessage, int32 ErrorCode)
{
    UE_LOG(LogTemp, Error, TEXT("HSSessionManager 에러: %s (코드: %d)"), *ErrorMessage, ErrorCode);
    
    OnSessionError.Broadcast(ErrorMessage, ErrorCode);
    
    // 심각한 에러인 경우 상태를 에러로 변경
    if (CurrentSessionState != EHSSessionState::SS_None)
    {
        ChangeSessionState(EHSSessionState::SS_Error);
    }
}

// 네트워크 에러 복구
void UHSSessionManager::RecoverFromNetworkError()
{
    if (bAutoReconnectEnabled && CurrentReconnectAttempts < MaxReconnectRetries)
    {
        AttemptReconnect();
    }
}

// 세션 무결성 확인
bool UHSSessionManager::ValidateSessionIntegrity() const
{
    if (!IsInSession() || !SessionInterface.IsValid())
    {
        return false;
    }

    const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    return Session != nullptr;
}
