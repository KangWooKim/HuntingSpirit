#include "HSDedicatedServerManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"
#include "Engine/NetConnection.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"

namespace
{
    FString NormalizeBase64Url(const FString& Input)
    {
        FString Result = Input;
        Result.ReplaceInline(TEXT("-"), TEXT("+"));
        Result.ReplaceInline(TEXT("_"), TEXT("/"));
        while (Result.Len() % 4 != 0)
        {
            Result.AppendChar(TEXT('='));
        }
        return Result;
    }

    bool DecodeBase64Url(const FString& Input, FString& Output)
    {
        const FString Normalized = NormalizeBase64Url(Input);
        return FBase64::Decode(Normalized, Output);
    }

    bool ParseJwtPayload(const FString& Token, TSharedPtr<FJsonObject>& OutPayload)
    {
        TArray<FString> Segments;
        Token.ParseIntoArray(Segments, TEXT("."), true);
        if (Segments.Num() < 2)
        {
            return false;
        }

        FString PayloadJson;
        if (!DecodeBase64Url(Segments[1], PayloadJson))
        {
            return false;
        }

        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
        return FJsonSerializer::Deserialize(Reader, OutPayload) && OutPayload.IsValid();
    }
}

UHSDedicatedServerManager::UHSDedicatedServerManager()
{
    CurrentServerStatus = EHSServerStatus::Offline;
    CurrentEnvironment = EHSServerEnvironment::Development;
    ServerConfig = nullptr;
    
    // 자동 관리 기본 설정
    bAutoSessionCleanupEnabled = true;
    bAutoPlayerTimeoutEnabled = true;
    bAutoPerformanceOptimizationEnabled = true;
    PlayerTimeoutSeconds = 300.0f; // 5분
    
    // 오브젝트 풀 초기화
    SessionPool.Reserve(100);
    PlayerPool.Reserve(500);
    
    // 에러 추적 초기화
    ConsecutiveErrorCount = 0;
    LastErrorTime = FDateTime::MinValue();
    
    // 성능 캐싱 초기화
    LastMetricsUpdateTime = 0.0f;
    LastRecordedInBytes = 0;
    LastRecordedOutBytes = 0;
    LastNetworkSampleSeconds = 0.0;
}

void UHSDedicatedServerManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 데디케이트 서버 관리자 초기화 중..."));
    
    // 서버 설정 로드
    ServerConfig = NewObject<UHSServerConfig>(this);
    if (ServerConfig)
    {
        LoadServerConfig(EHSServerEnvironment::Development);
    }

    const bool bServerStarted = StartServer(EHSServerEnvironment::Development);

    // 성능 모니터링 시작
    StartPerformanceMonitoring();
    
    // 자동 관리 기능 활성화
    EnableAutoSessionCleanup(true);
    EnableAutoPlayerTimeout(true);
    EnableAutoPerformanceOptimization(true);

    if (!bServerStarted)
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 초기 서버 시작에 실패했습니다"));
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 초기화 완료"));
}

void UHSDedicatedServerManager::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 관리자 종료 중..."));
    
    StopServer(true);
    StopPerformanceMonitoring();
    DeallocateServerResources();
    
    Super::Deinitialize();
}

bool UHSDedicatedServerManager::StartServer(EHSServerEnvironment Environment)
{
    FScopeLock Lock(&ServerMutex);
    
    if (CurrentServerStatus != EHSServerStatus::Offline && CurrentServerStatus != EHSServerStatus::Error)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 서버가 이미 실행 중이거나 시작 중입니다"));
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 시작 중... 환경: %d"), (int32)Environment);
    
    UpdateServerStatus(EHSServerStatus::Starting);
    CurrentEnvironment = Environment;
    
    // 환경별 설정 로드
    LoadServerConfig(Environment);
    
    if (!ValidateServerState(false))
    {
        HandleServerError(TEXT("서버 상태 검증 실패"));
        return false;
    }
    
    // 서버 리소스 할당
    AllocateServerResources();
    
    // 네트워크 리스너 초기화
    if (!InitializeNetworkListener())
    {
        HandleServerError(TEXT("네트워크 리스너 초기화 실패"));
        DeallocateServerResources();
        return false;
    }
    
    if (!ValidateServerState(true))
    {
        HandleServerError(TEXT("네트워크 초기화 검증 실패"));
        ShutdownNetworkListener();
        DeallocateServerResources();
        return false;
    }
    
    // 플랫폼별 초기화
#if PLATFORM_WINDOWS
    InitializeWindowsSpecific();
#elif PLATFORM_LINUX
    InitializeLinuxSpecific();
#endif
    
    ConsecutiveErrorCount = 0;
    
    UpdateServerStatus(EHSServerStatus::Online);
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 시작 완료"));
    return true;
}

void UHSDedicatedServerManager::StopServer(bool bGracefulShutdown)
{
    FScopeLock Lock(&ServerMutex);
    
    if (CurrentServerStatus == EHSServerStatus::Offline)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 중지 중... (Graceful: %s)"), 
           bGracefulShutdown ? TEXT("Yes") : TEXT("No"));
    
    UpdateServerStatus(EHSServerStatus::Stopping);
    
    if (bGracefulShutdown)
    {
        // 모든 활성 세션 정리
        for (auto& SessionPair : ActiveSessions)
        {
            EndGameSession(SessionPair.Key);
        }
        
        // 연결된 플레이어 정리
        for (auto& PlayerPair : ConnectedPlayers)
        {
            DisconnectPlayer(PlayerPair.Key, TEXT("Server Shutdown"));
        }
        
        // 잠시 대기 (클라이언트 정리 시간)
        FPlatformProcess::Sleep(2.0f);
    }
    
    // 네트워크 리스너 중지
    ShutdownNetworkListener();
    
    // 타이머 정리
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PerformanceMonitoringTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(SessionCleanupTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(PlayerTimeoutTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(AutoOptimizationTimerHandle);
    }
    
    // 리소스 해제
    DeallocateServerResources();
    
    UpdateServerStatus(EHSServerStatus::Offline);
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 중지 완료"));
}

void UHSDedicatedServerManager::RestartServer()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 재시작 중..."));
    
    UpdateServerStatus(EHSServerStatus::Restarting);
    
    EHSServerEnvironment CurrentEnv = CurrentEnvironment;
    StopServer(true);
    
    // 잠시 대기
    FPlatformProcess::Sleep(1.0f);
    
    StartServer(CurrentEnv);
}

void UHSDedicatedServerManager::SetMaintenanceMode(bool bEnabled)
{
    if (bEnabled)
    {
        UpdateServerStatus(EHSServerStatus::Maintenance);
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 점검 모드 활성화"));
    }
    else
    {
        UpdateServerStatus(EHSServerStatus::Online);
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 점검 모드 비활성화"));
    }
}

FString UHSDedicatedServerManager::CreateGameSession(const FString& SessionName, const FString& MapName, int32 MaxPlayers, bool bRanked)
{
    FScopeLock Lock(&SessionMutex);
    
    if (CurrentServerStatus != EHSServerStatus::Online)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 서버가 오프라인 상태에서 세션 생성 요청"));
        return FString();
    }
    
    if (ActiveSessions.Num() >= (ServerConfig ? ServerConfig->GetPerformanceConfig().MaxConcurrentSessions : 50))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 최대 세션 수 초과"));
        return FString();
    }
    
    FString SessionID = GenerateSessionID();
    
    FHSGameSessionInfo SessionInfo;
    SessionInfo.SessionID = SessionID;
    SessionInfo.SessionName = SessionName;
    SessionInfo.MaxPlayers = FMath::Clamp(MaxPlayers, 1, 8);
    SessionInfo.MapName = MapName;
    SessionInfo.GameMode = TEXT("HuntingSpirit");
    SessionInfo.CreationTime = FDateTime::Now();
    SessionInfo.bIsRanked = bRanked;
    SessionInfo.bIsActive = true;
    
    ActiveSessions.Add(SessionID, SessionInfo);
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 게임 세션 생성 - ID: %s, 이름: %s"), 
           *SessionID, *SessionName);
    
    OnGameSessionCreated.Broadcast(SessionInfo);
    
    return SessionID;
}

bool UHSDedicatedServerManager::EndGameSession(const FString& SessionID)
{
    FScopeLock Lock(&SessionMutex);
    
    FHSGameSessionInfo* SessionInfo = ActiveSessions.Find(SessionID);
    if (!SessionInfo)
    {
        return false;
    }
    
    // 세션의 모든 플레이어 제거
    for (const FString& PlayerID : SessionInfo->PlayerIDs)
    {
        DisconnectPlayer(PlayerID, TEXT("Session Ended"));
    }
    
    SessionInfo->bIsActive = false;
    SessionInfo->SessionDuration = (FDateTime::Now() - SessionInfo->CreationTime).GetTotalSeconds();
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 게임 세션 종료 - ID: %s, 지속시간: %.1f초"), 
           *SessionID, SessionInfo->SessionDuration);
    
    OnGameSessionEnded.Broadcast(SessionID);
    
    ActiveSessions.Remove(SessionID);
    
    return true;
}

bool UHSDedicatedServerManager::JoinGameSession(const FString& SessionID, const FString& PlayerID)
{
    FScopeLock Lock(&SessionMutex);
    
    FHSGameSessionInfo* SessionInfo = ActiveSessions.Find(SessionID);
    if (!SessionInfo || !SessionInfo->bIsActive)
    {
        return false;
    }
    
    if (SessionInfo->CurrentPlayers >= SessionInfo->MaxPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 세션 인원 초과 - %s"), *SessionID);
        return false;
    }
    
    if (SessionInfo->PlayerIDs.Contains(PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 플레이어가 이미 세션에 참가중 - %s"), *PlayerID);
        return false;
    }
    
    SessionInfo->PlayerIDs.Add(PlayerID);
    SessionInfo->CurrentPlayers = SessionInfo->PlayerIDs.Num();
    
    // 플레이어 연결 정보 업데이트
    FHSPlayerConnectionInfo* PlayerInfo = ConnectedPlayers.Find(PlayerID);
    if (PlayerInfo)
    {
        PlayerInfo->SessionID = SessionID;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 플레이어가 세션에 참가 - 플레이어: %s, 세션: %s"), 
           *PlayerID, *SessionID);
    
    return true;
}

bool UHSDedicatedServerManager::LeaveGameSession(const FString& SessionID, const FString& PlayerID)
{
    FScopeLock Lock(&SessionMutex);
    
    FHSGameSessionInfo* SessionInfo = ActiveSessions.Find(SessionID);
    if (!SessionInfo)
    {
        return false;
    }
    
    SessionInfo->PlayerIDs.Remove(PlayerID);
    SessionInfo->CurrentPlayers = SessionInfo->PlayerIDs.Num();
    
    // 플레이어 연결 정보 업데이트
    FHSPlayerConnectionInfo* PlayerInfo = ConnectedPlayers.Find(PlayerID);
    if (PlayerInfo)
    {
        PlayerInfo->SessionID.Empty();
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 플레이어가 세션에서 퇴장 - 플레이어: %s, 세션: %s"), 
           *PlayerID, *SessionID);
    
    // 세션이 비었으면 종료
    if (SessionInfo->CurrentPlayers == 0)
    {
        EndGameSession(SessionID);
    }
    
    return true;
}

TArray<FHSGameSessionInfo> UHSDedicatedServerManager::GetActiveGameSessions() const
{
    FScopeLock Lock(&SessionMutex);
    
    TArray<FHSGameSessionInfo> Sessions;
    for (const auto& SessionPair : ActiveSessions)
    {
        if (SessionPair.Value.bIsActive)
        {
            Sessions.Add(SessionPair.Value);
        }
    }
    
    return Sessions;
}

FHSGameSessionInfo UHSDedicatedServerManager::GetGameSessionInfo(const FString& SessionID) const
{
    FScopeLock Lock(&SessionMutex);
    
    const FHSGameSessionInfo* SessionInfo = ActiveSessions.Find(SessionID);
    return SessionInfo ? *SessionInfo : FHSGameSessionInfo();
}

bool UHSDedicatedServerManager::AuthenticatePlayer(const FString& PlayerID, const FString& AuthToken)
{
    if (!ValidatePlayerAuthentication(PlayerID, AuthToken))
    {
        LogSecurityEvent(TEXT("Authentication Failed"), PlayerID);
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 플레이어 인증 성공 - %s"), *PlayerID);
    
    return true;
}

void UHSDedicatedServerManager::DisconnectPlayer(const FString& PlayerID, const FString& Reason)
{
    FScopeLock Lock(&PlayerMutex);
    
    FHSPlayerConnectionInfo* PlayerInfo = ConnectedPlayers.Find(PlayerID);
    if (!PlayerInfo)
    {
        return;
    }
    
    // 세션에서 제거
    if (!PlayerInfo->SessionID.IsEmpty())
    {
        LeaveGameSession(PlayerInfo->SessionID, PlayerID);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 플레이어 연결 해제 - %s, 이유: %s"), 
           *PlayerID, *Reason);
    
    OnPlayerDisconnected.Broadcast(PlayerID);
    
    ConnectedPlayers.Remove(PlayerID);
}

TArray<FHSPlayerConnectionInfo> UHSDedicatedServerManager::GetConnectedPlayers() const
{
    FScopeLock Lock(&PlayerMutex);
    
    TArray<FHSPlayerConnectionInfo> Players;
    ConnectedPlayers.GenerateValueArray(Players);
    
    return Players;
}

FHSPlayerConnectionInfo UHSDedicatedServerManager::GetPlayerConnectionInfo(const FString& PlayerID) const
{
    FScopeLock Lock(&PlayerMutex);
    
    const FHSPlayerConnectionInfo* PlayerInfo = ConnectedPlayers.Find(PlayerID);
    return PlayerInfo ? *PlayerInfo : FHSPlayerConnectionInfo();
}

int32 UHSDedicatedServerManager::GetConnectedPlayerCount() const
{
    FScopeLock Lock(&PlayerMutex);
    return ConnectedPlayers.Num();
}

void UHSDedicatedServerManager::RegisterPlayerConnection(const FString& PlayerID, const FString& PlayerName, const FString& IPAddress, int32 Port)
{
    if (PlayerID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 빈 플레이어 ID로 연결 등록을 시도했습니다"));
        return;
    }

    const FString SanitizedName = PlayerName.IsEmpty() ? PlayerID : PlayerName;
    const FString SanitizedIP = IPAddress.IsEmpty() ? TEXT("Unknown") : IPAddress;

    HandlePlayerConnection(PlayerID, SanitizedIP, Port, SanitizedName, true);
}

void UHSDedicatedServerManager::UnregisterPlayerConnection(const FString& PlayerID, const FString& Reason)
{
    if (PlayerID.IsEmpty())
    {
        return;
    }

    HandlePlayerDisconnection(PlayerID, Reason.IsEmpty() ? TEXT("Player Logout") : Reason);
}

void UHSDedicatedServerManager::LoadServerConfig(EHSServerEnvironment Environment)
{
    if (!ServerConfig)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 설정 로드 중... 환경: %d"), (int32)Environment);
    
    // 환경별 설정 적용
    switch (Environment)
    {
        case EHSServerEnvironment::Development:
            ServerConfig->SetDevelopmentEnvironment();
            break;
        case EHSServerEnvironment::Staging:
            ServerConfig->SetStagingEnvironment();
            break;
        case EHSServerEnvironment::Production:
            ServerConfig->SetProductionEnvironment();
            break;
        case EHSServerEnvironment::LoadTest:
            ServerConfig->SetLoadTestEnvironment();
            break;
    }
    
    // 설정 검증
    if (!ServerConfig->ValidateConfiguration())
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 서버 설정 검증 실패"));
        return;
    }
    
    CurrentEnvironment = Environment;
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 설정 로드 완료"));
}

void UHSDedicatedServerManager::SaveServerConfig()
{
    if (!ServerConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 저장할 서버 설정이 없습니다"));
        return;
    }

    const FString ConfigDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Server"));
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ConfigDirectory))
    {
        PlatformFile.CreateDirectoryTree(*ConfigDirectory);
    }

    const FString ConfigFilePath = FPaths::Combine(ConfigDirectory, TEXT("HSServerConfig.json"));

    if (ServerConfig->SaveConfigurationToFile(ConfigFilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 설정 저장 완료 - %s"), *ConfigFilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 서버 설정 저장 실패 - %s"), *ConfigFilePath);
    }
}

void UHSDedicatedServerManager::StartPerformanceMonitoring()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            PerformanceMonitoringTimerHandle,
            this,
            &UHSDedicatedServerManager::UpdatePerformanceMetrics,
            PerformanceUpdateInterval,
            true
        );
        
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 성능 모니터링 시작"));
    }
}

void UHSDedicatedServerManager::StopPerformanceMonitoring()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PerformanceMonitoringTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 성능 모니터링 중지"));
    }
}

void UHSDedicatedServerManager::UpdatePerformanceMetrics()
{
    // 캐시 확인 (성능 최적화)
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - LastMetricsUpdateTime < MetricsCacheDuration)
    {
        return;
    }
    
    // 각종 성능 지표 수집
    CollectCPUMetrics();
    CollectMemoryMetrics();
    CollectNetworkMetrics();
    CollectGameplayMetrics();
    
    CurrentMetrics.LastUpdateTime = FDateTime::Now();
    LastMetricsUpdateTime = CurrentTime;
    
    OnPerformanceMetricsUpdated.Broadcast(CurrentMetrics);
}

void UHSDedicatedServerManager::EnableAutoSessionCleanup(bool bEnabled)
{
    bAutoSessionCleanupEnabled = bEnabled;
    
    if (bEnabled && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            SessionCleanupTimerHandle,
            this,
            &UHSDedicatedServerManager::ProcessAutoSessionCleanup,
            SessionCleanupInterval,
            true
        );
    }
    else if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SessionCleanupTimerHandle);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 자동 세션 정리 %s"), 
           bEnabled ? TEXT("활성화") : TEXT("비활성화"));
}

void UHSDedicatedServerManager::EnableAutoPlayerTimeout(bool bEnabled, float TimeoutSeconds)
{
    bAutoPlayerTimeoutEnabled = bEnabled;
    PlayerTimeoutSeconds = TimeoutSeconds;
    
    if (bEnabled && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            PlayerTimeoutTimerHandle,
            this,
            &UHSDedicatedServerManager::ProcessAutoPlayerTimeout,
            PlayerTimeoutCheckInterval,
            true
        );
    }
    else if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PlayerTimeoutTimerHandle);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 자동 플레이어 타임아웃 %s (%.1f초)"), 
           bEnabled ? TEXT("활성화") : TEXT("비활성화"), TimeoutSeconds);
}

void UHSDedicatedServerManager::EnableAutoPerformanceOptimization(bool bEnabled)
{
    bAutoPerformanceOptimizationEnabled = bEnabled;
    
    if (bEnabled && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            AutoOptimizationTimerHandle,
            this,
            &UHSDedicatedServerManager::ProcessAutoPerformanceOptimization,
            AutoOptimizationInterval,
            true
        );
    }
    else if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoOptimizationTimerHandle);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 자동 성능 최적화 %s"), 
           bEnabled ? TEXT("활성화") : TEXT("비활성화"));
}

void UHSDedicatedServerManager::UpdateServerStatus(EHSServerStatus NewStatus)
{
    if (CurrentServerStatus == NewStatus)
    {
        return;
    }
    
    EHSServerStatus OldStatus = CurrentServerStatus;
    CurrentServerStatus = NewStatus;
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 상태 변경 %d -> %d"), 
           (int32)OldStatus, (int32)NewStatus);
    
    OnServerStatusChanged.Broadcast(NewStatus);
}

bool UHSDedicatedServerManager::InitializeNetworkListener()
{
    if (!ServerConfig)
    {
        return false;
    }
    
    const FHSNetworkConfig& NetConfig = ServerConfig->GetNetworkConfig();
    
    // 소켓 서브시스템 가져오기
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 소켓 서브시스템을 찾을 수 없음"));
        return false;
    }
    
    // 서버 소켓 생성
    ServerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("HuntingSpirit Server Socket"), false));
    if (!ServerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 서버 소켓 생성 실패"));
        return false;
    }
    
    // 서버 주소 설정
    ServerAddress = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    ServerAddress->SetIp(*NetConfig.ServerIP, bIsValid);
    if (!bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 잘못된 서버 IP: %s"), *NetConfig.ServerIP);
        return false;
    }
    ServerAddress->SetPort(NetConfig.ServerPort);
    
    // 소켓 옵션 설정
    ServerSocket->SetReuseAddr(true);
    ServerSocket->SetNonBlocking(true);
    ServerSocket->SetRecvErr(true);
    
    // 소켓 바인딩
    if (!ServerSocket->Bind(*ServerAddress))
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 소켓 바인딩 실패 - %s:%d"), 
               *NetConfig.ServerIP, NetConfig.ServerPort);
        return false;
    }
    
    // 리스닝 시작
    if (!ServerSocket->Listen(NetConfig.MaxConnections))
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 소켓 리스닝 시작 실패"));
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 네트워크 리스너 초기화 완료 - %s:%d"), 
           *NetConfig.ServerIP, NetConfig.ServerPort);
    
    return true;
}

void UHSDedicatedServerManager::ShutdownNetworkListener()
{
    if (ServerSocket.IsValid())
    {
        ServerSocket->Close();
        ServerSocket.Reset();
    }
    
    ServerAddress.Reset();
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 네트워크 리스너 종료"));
}

FString UHSDedicatedServerManager::GenerateSessionID() const
{
    return FGuid::NewGuid().ToString();
}

void UHSDedicatedServerManager::CleanupInactiveSessions()
{
    FScopeLock Lock(&SessionMutex);
    
    TArray<FString> SessionsToRemove;
    
    for (auto& SessionPair : ActiveSessions)
    {
        FHSGameSessionInfo& SessionInfo = SessionPair.Value;
        
        if (!SessionInfo.bIsActive)
        {
            SessionsToRemove.Add(SessionPair.Key);
            continue;
        }
        
        // 최대 세션 시간 초과 확인
        float MaxDuration = ServerConfig ? ServerConfig->GetGameplayConfig().MaxSessionDuration : 3600.0f;
        float CurrentDuration = (FDateTime::Now() - SessionInfo.CreationTime).GetTotalSeconds();
        
        if (CurrentDuration > MaxDuration)
        {
            UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 세션 시간 초과로 종료 - %s"), *SessionPair.Key);
            SessionsToRemove.Add(SessionPair.Key);
        }
    }
    
    for (const FString& SessionID : SessionsToRemove)
    {
        EndGameSession(SessionID);
    }
}

void UHSDedicatedServerManager::ValidateSessionIntegrity()
{
    FScopeLock Lock(&SessionMutex);
    
    for (auto& SessionPair : ActiveSessions)
    {
        FHSGameSessionInfo& SessionInfo = SessionPair.Value;
        
        // 플레이어 수 동기화
        SessionInfo.CurrentPlayers = SessionInfo.PlayerIDs.Num();
        
        // 세션 지속시간 업데이트
        SessionInfo.SessionDuration = (FDateTime::Now() - SessionInfo.CreationTime).GetTotalSeconds();
    }
}

void UHSDedicatedServerManager::HandlePlayerConnection(const FString& PlayerID, const FString& IPAddress, int32 Port, const FString& PlayerName, bool bAuthenticated)
{
    FScopeLock Lock(&PlayerMutex);
    
    if (FHSPlayerConnectionInfo* ExistingInfo = ConnectedPlayers.Find(PlayerID))
    {
        ExistingInfo->PlayerName = PlayerName.IsEmpty() ? ExistingInfo->PlayerName : PlayerName;
        ExistingInfo->IPAddress = IPAddress;
        ExistingInfo->Port = Port;
        ExistingInfo->ConnectionTime = FDateTime::Now();
        ExistingInfo->bIsAuthenticated = bAuthenticated;
        
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 플레이어 정보 업데이트 - %s (%s:%d)"), 
               *PlayerID, *IPAddress, Port);
        
        OnPlayerConnected.Broadcast(*ExistingInfo);
        return;
    }

    FHSPlayerConnectionInfo PlayerInfo;
    PlayerInfo.PlayerID = PlayerID;
    PlayerInfo.PlayerName = PlayerName.IsEmpty() ? FString::Printf(TEXT("Player_%s"), *PlayerID.Right(8)) : PlayerName;
    PlayerInfo.IPAddress = IPAddress;
    PlayerInfo.Port = Port;
    PlayerInfo.ConnectionTime = FDateTime::Now();
    PlayerInfo.bIsAuthenticated = bAuthenticated;
    
    ConnectedPlayers.Add(PlayerID, PlayerInfo);
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 플레이어 연결 - %s (%s:%d)"), 
           *PlayerID, *IPAddress, Port);
    
    OnPlayerConnected.Broadcast(PlayerInfo);
}

void UHSDedicatedServerManager::HandlePlayerDisconnection(const FString& PlayerID, const FString& Reason)
{
    DisconnectPlayer(PlayerID, Reason);
}

void UHSDedicatedServerManager::UpdatePlayerConnectionMetrics()
{
    FScopeLock Lock(&PlayerMutex);
    
    TMap<FString, float> LatestPingByPlayer;
    if (UWorld* World = GetWorld())
    {
        if (UNetDriver* NetDriver = World->GetNetDriver())
        {
            for (UNetConnection* Connection : NetDriver->ClientConnections)
            {
                if (!Connection)
                {
                    continue;
                }

                FString ConnectionPlayerId;
                if (Connection->PlayerController && Connection->PlayerController->PlayerState && Connection->PlayerController->PlayerState->GetUniqueId().IsValid())
                {
                    ConnectionPlayerId = Connection->PlayerController->PlayerState->GetUniqueId()->ToString();
                }
                else if (Connection->PlayerId.IsValid())
                {
                    ConnectionPlayerId = Connection->PlayerId->ToString();
                }

                if (!ConnectionPlayerId.IsEmpty())
                {
                    LatestPingByPlayer.Add(ConnectionPlayerId, Connection->AvgLag * 1000.0f);
                }
            }
        }
    }

    const FDateTime NowUtc = FDateTime::UtcNow();

    for (auto& PlayerPair : ConnectedPlayers)
    {
        FHSPlayerConnectionInfo& PlayerInfo = PlayerPair.Value;
        PlayerInfo.ConnectionDuration = (NowUtc - PlayerInfo.ConnectionTime).GetTotalSeconds();

        if (const float* PingMs = LatestPingByPlayer.Find(PlayerInfo.PlayerID))
        {
            PlayerInfo.Ping = *PingMs;
        }
    }
}

void UHSDedicatedServerManager::CollectCPUMetrics()
{
    float CpuUsagePercent = 0.0f;

    const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
    FPlatformTime::UpdateCPUTime(DeltaSeconds);

    const FCPUTime CpuSample = FPlatformTime::GetCPUTime();
    CpuUsagePercent = CpuSample.CPUTimePct;

    CurrentMetrics.CPUUsagePercent = FMath::Clamp(CpuUsagePercent, 0.0f, 100.0f);
}

void UHSDedicatedServerManager::CollectMemoryMetrics()
{
    const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
    CurrentMetrics.MemoryUsageMB = static_cast<float>(MemoryStats.UsedPhysical) / (1024.0f * 1024.0f);
}

void UHSDedicatedServerManager::CollectNetworkMetrics()
{
    if (!GetWorld())
    {
        CurrentMetrics.NetworkInKBps = 0.0f;
        CurrentMetrics.NetworkOutKBps = 0.0f;
        return;
    }

    if (UNetDriver* NetDriver = GetWorld()->GetNetDriver())
    {
        const double CurrentTimeSeconds = FPlatformTime::Seconds();

        if (LastNetworkSampleSeconds <= 0.0)
        {
            LastNetworkSampleSeconds = CurrentTimeSeconds;
            LastRecordedInBytes = NetDriver->InBytes;
            LastRecordedOutBytes = NetDriver->OutBytes;
            CurrentMetrics.NetworkInKBps = 0.0f;
            CurrentMetrics.NetworkOutKBps = 0.0f;
            return;
        }

        const double DeltaSeconds = CurrentTimeSeconds - LastNetworkSampleSeconds;
        if (DeltaSeconds <= KINDA_SMALL_NUMBER)
        {
            return;
        }

        const uint64 InDiff = NetDriver->InBytes - LastRecordedInBytes;
        const uint64 OutDiff = NetDriver->OutBytes - LastRecordedOutBytes;

        CurrentMetrics.NetworkInKBps = static_cast<float>((InDiff / 1024.0) / DeltaSeconds);
        CurrentMetrics.NetworkOutKBps = static_cast<float>((OutDiff / 1024.0) / DeltaSeconds);

        LastRecordedInBytes = NetDriver->InBytes;
        LastRecordedOutBytes = NetDriver->OutBytes;
        LastNetworkSampleSeconds = CurrentTimeSeconds;
    }
    else
    {
        CurrentMetrics.NetworkInKBps = 0.0f;
        CurrentMetrics.NetworkOutKBps = 0.0f;
    }
}

void UHSDedicatedServerManager::CollectGameplayMetrics()
{
    double TotalLatency = 0.0;
    int32 PlayerCount = 0;
    {
        FScopeLock PlayerLock(&PlayerMutex);
        PlayerCount = ConnectedPlayers.Num();
        for (const auto& PlayerPair : ConnectedPlayers)
        {
            TotalLatency += PlayerPair.Value.Ping;
        }
    }
    CurrentMetrics.ConnectedPlayers = PlayerCount;

    {
        FScopeLock SessionLock(&SessionMutex);
        CurrentMetrics.ActiveGameSessions = ActiveSessions.Num();
    }
    if (UWorld* World = GetWorld())
    {
        const float DeltaSeconds = World->GetDeltaSeconds();
        CurrentMetrics.TickRate = DeltaSeconds > KINDA_SMALL_NUMBER ? (1.0f / DeltaSeconds) : 0.0f;
    }
    else
    {
        CurrentMetrics.TickRate = 0.0f;
    }
    CurrentMetrics.AverageLatency = PlayerCount > 0 ? static_cast<float>(TotalLatency / PlayerCount) : 0.0f;

    float TotalPacketLossPercent = 0.0f;
    int32 ConnectionCount = 0;
    if (UWorld* World = GetWorld())
    {
        if (UNetDriver* NetDriver = World->GetNetDriver())
        {
            for (UNetConnection* Connection : NetDriver->ClientConnections)
            {
                if (!Connection)
                {
                    continue;
                }

                const float IncomingLoss = Connection->GetInLossPercentage().GetAvgLossPercentage();
                const float OutgoingLoss = Connection->GetOutLossPercentage().GetAvgLossPercentage();
                TotalPacketLossPercent += (IncomingLoss + OutgoingLoss) * 50.0f;
                ++ConnectionCount;
            }
        }
    }

    CurrentMetrics.PacketLossPercent = ConnectionCount > 0 ? TotalPacketLossPercent / ConnectionCount : 0.0f;
}

void UHSDedicatedServerManager::ProcessAutoSessionCleanup()
{
    if (!bAutoSessionCleanupEnabled)
    {
        return;
    }
    
    CleanupInactiveSessions();
    ValidateSessionIntegrity();
}

void UHSDedicatedServerManager::ProcessAutoPlayerTimeout()
{
    if (!bAutoPlayerTimeoutEnabled)
    {
        return;
    }
    
    FScopeLock Lock(&PlayerMutex);
    
    TArray<FString> PlayersToTimeout;
    FDateTime CurrentTime = FDateTime::Now();
    
    for (const auto& PlayerPair : ConnectedPlayers)
    {
        const FHSPlayerConnectionInfo& PlayerInfo = PlayerPair.Value;
        float ConnectionDuration = (CurrentTime - PlayerInfo.ConnectionTime).GetTotalSeconds();
        
        if (ConnectionDuration > PlayerTimeoutSeconds)
        {
            PlayersToTimeout.Add(PlayerPair.Key);
        }
    }
    
    for (const FString& PlayerID : PlayersToTimeout)
    {
        DisconnectPlayer(PlayerID, TEXT("Connection Timeout"));
    }
}

void UHSDedicatedServerManager::ProcessAutoPerformanceOptimization()
{
    if (!bAutoPerformanceOptimizationEnabled)
    {
        return;
    }
    
    OptimizeMemoryUsage();
    
    // CPU 사용률이 높을 때 최적화
    if (CurrentMetrics.CPUUsagePercent > 85.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 높은 CPU 사용률 감지 (%.1f%%), 최적화 수행"), 
               CurrentMetrics.CPUUsagePercent);
        
        // 틱 레이트 조절
        if (GetWorld())
        {
            GetWorld()->GetWorldSettings()->MaxUndilatedFrameTime = 0.02f; // 50fps로 제한
        }
    }
    
    // 메모리 사용률이 높을 때 가비지 컬렉션
    if (CurrentMetrics.MemoryUsageMB > 3072.0f) // 3GB 초과
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 높은 메모리 사용률 감지 (%.1fMB), 가비지 컬렉션 수행"), 
               CurrentMetrics.MemoryUsageMB);
        
        if (GEngine)
        {
            GEngine->ForceGarbageCollection(true);
        }
    }
}

void UHSDedicatedServerManager::AllocateServerResources()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 리소스 할당 중..."));
    
    // 오브젝트 풀 크기 조정
    if (ServerConfig)
    {
        const FHSPerformanceConfig& PerfConfig = ServerConfig->GetPerformanceConfig();
        
        SessionPool.Reserve(PerfConfig.MaxConcurrentSessions);
        PlayerPool.Reserve(PerfConfig.MaxConcurrentSessions * 8); // 세션당 최대 8명
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 리소스 할당 완료"));
}

void UHSDedicatedServerManager::DeallocateServerResources()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 리소스 해제 중..."));
    
    // 모든 활성 세션 정리
    ActiveSessions.Empty();
    ConnectedPlayers.Empty();
    
    // 오브젝트 풀 정리
    SessionPool.Empty();
    PlayerPool.Empty();
    
    // 캐시 정리
    SessionInfoCache.Empty();
    PlayerInfoCache.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 리소스 해제 완료"));
}

void UHSDedicatedServerManager::OptimizeMemoryUsage()
{
    // 캐시 정리
    SessionInfoCache.Empty();
    PlayerInfoCache.Empty();
    
    // 오브젝트 풀 압축
    SessionPool.Shrink();
    PlayerPool.Shrink();
}

bool UHSDedicatedServerManager::ValidatePlayerAuthentication(const FString& PlayerID, const FString& AuthToken) const
{
    if (PlayerID.IsEmpty() || AuthToken.IsEmpty())
    {
        return false;
    }

    if (!ServerConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 서버 설정이 없어 인증을 진행할 수 없습니다"));
        return false;
    }

    const FHSSecurityConfig SecurityConfig = ServerConfig->GetSecurityConfig();

    switch (SecurityConfig.AuthMethod)
    {
        case EHSAuthenticationMethod::None:
            return true;

        case EHSAuthenticationMethod::Basic:
        {
            FString DecodedToken;
            if (!FBase64::Decode(AuthToken, DecodedToken))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Basic 토큰 디코딩 실패"));
                return false;
            }

            TArray<FString> Segments;
            DecodedToken.ParseIntoArray(Segments, TEXT(":"), true);
            if (Segments.Num() < 2)
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Basic 토큰 형식이 잘못되었습니다"));
                return false;
            }

            if (!Segments[0].Equals(PlayerID, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Basic 토큰 플레이어 ID 불일치"));
                return false;
            }

            FString Signature = Segments.Last();
            if (Signature.Len() < 8)
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Basic 토큰 서명이 너무 짧습니다"));
                return false;
            }

            if (Segments.Num() >= 3)
            {
                const int64 Timestamp = FCString::Atoi64(*Segments[1]);
                if (Timestamp > 0)
                {
                    const FDateTime TokenTime = FDateTime::FromUnixTimestamp(Timestamp);
                    if ((FDateTime::UtcNow() - TokenTime).GetTotalSeconds() > SecurityConfig.TokenValidityDuration)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Basic 토큰이 만료되었습니다"));
                        return false;
                    }
                }
            }

            return true;
        }

        case EHSAuthenticationMethod::Token:
        {
            TSharedPtr<FJsonObject> Payload;
            if (!ParseJwtPayload(AuthToken, Payload))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: JWT 페이로드 파싱 실패"));
                return false;
            }

            FString Subject;
            Payload->TryGetStringField(TEXT("sub"), Subject);
            if (Subject.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("playerId"), Subject);
            }

            if (!Subject.Equals(PlayerID, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: JWT 플레이어 ID 불일치"));
                return false;
            }

            double ExpirationSeconds = 0.0;
            if (!Payload->TryGetNumberField(TEXT("exp"), ExpirationSeconds))
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: JWT 만료 정보 없음"));
                return false;
            }

            const FDateTime ExpirationTime = FDateTime::FromUnixTimestamp(static_cast<int64>(ExpirationSeconds));
            if (FDateTime::UtcNow() >= ExpirationTime)
            {
                UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: JWT 토큰이 만료되었습니다"));
                return false;
            }

            return true;
        }

        default:
            UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 지원되지 않는 인증 방식입니다 - %d"), static_cast<int32>(SecurityConfig.AuthMethod));
            break;
    }

    return false;
}

bool UHSDedicatedServerManager::ValidateSessionAccess(const FString& SessionID, const FString& PlayerID) const
{
    const FHSGameSessionInfo* SessionInfo = ActiveSessions.Find(SessionID);
    if (!SessionInfo || !SessionInfo->bIsActive)
    {
        return false;
    }
    
    return SessionInfo->PlayerIDs.Contains(PlayerID);
}

void UHSDedicatedServerManager::LogSecurityEvent(const FString& Event, const FString& PlayerID)
{
    UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 보안 이벤트 - %s (플레이어: %s)"), 
           *Event, *PlayerID);

    const FString LogDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"));
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*LogDirectory))
    {
        PlatformFile.CreateDirectoryTree(*LogDirectory);
    }

    const FString LogFilePath = FPaths::Combine(LogDirectory, TEXT("SecurityEvents.log"));
    const FString LogLine = FString::Printf(TEXT("%s | %s | Player: %s%s"),
        *FDateTime::UtcNow().ToString(), *Event, *PlayerID, LINE_TERMINATOR);

    if (!FFileHelper::SaveStringToFile(LogLine, *LogFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 보안 이벤트 로그 기록 실패 - %s"), *LogFilePath);
    }
}

bool UHSDedicatedServerManager::IsServerInitialized() const
{
    return ServerConfig != nullptr && ServerSocket.IsValid();
}

bool UHSDedicatedServerManager::IsNetworkReady() const
{
    return ServerSocket.IsValid() && ServerAddress.IsValid();
}

bool UHSDedicatedServerManager::IsConfigurationValid() const
{
    return ServerConfig && ServerConfig->ValidateConfiguration();
}

void UHSDedicatedServerManager::HandleServerError(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 서버 에러 - %s"), *ErrorMessage);
    
    ConsecutiveErrorCount++;
    LastErrorTime = FDateTime::Now();
    
    UpdateServerStatus(EHSServerStatus::Error);
    
    // 연속 에러가 임계치를 넘으면 자동 복구 시도
    if (ConsecutiveErrorCount >= MaxConsecutiveErrors)
    {
        AttemptAutoRecovery();
    }
}

void UHSDedicatedServerManager::AttemptAutoRecovery()
{
    UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 자동 복구 시도 중..."));
    
    // 네트워크 재시작
    ShutdownNetworkListener();
    FPlatformProcess::Sleep(1.0f);
    
    if (InitializeNetworkListener())
    {
        ConsecutiveErrorCount = 0;
        UpdateServerStatus(EHSServerStatus::Online);
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 자동 복구 성공"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSDedicatedServerManager: 자동 복구 실패"));
    }
}

bool UHSDedicatedServerManager::ValidateServerState(bool bRequireNetworkReady) const
{
    if (!IsConfigurationValid())
    {
        return false;
    }
    
    if (bRequireNetworkReady)
    {
        return IsServerInitialized() && IsNetworkReady();
    }
    
    return true;
}

#if PLATFORM_WINDOWS
void UHSDedicatedServerManager::InitializeWindowsSpecific()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: Windows 전용 초기화 시작"));

    if (!ServerConfig || !ServerSocket.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Windows 초기화 건너뜀 - 서버 리소스가 준비되지 않았습니다"));
        return;
    }

    const FHSNetworkConfig& NetConfig = ServerConfig->GetNetworkConfig();

    int32 AppliedSendSize = 0;
    const int32 DesiredSendSize = FMath::Max(NetConfig.SendBufferSize, 65536);
    if (!ServerSocket->SetSendBufferSize(DesiredSendSize, AppliedSendSize))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 송신 버퍼 크기 설정 실패 (요청: %d)"), DesiredSendSize);
    }

    int32 AppliedReceiveSize = 0;
    const int32 DesiredReceiveSize = FMath::Max(NetConfig.ReceiveBufferSize, 65536);
    if (!ServerSocket->SetReceiveBufferSize(DesiredReceiveSize, AppliedReceiveSize))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 수신 버퍼 크기 설정 실패 (요청: %d)"), DesiredReceiveSize);
    }

    ServerSocket->SetNoDelay(true);
    ServerSocket->SetLinger(false, 0);

    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: Windows 전용 네트워크 튜닝 완료 (Send %d/%d, Receive %d/%d)"),
           DesiredSendSize, AppliedSendSize, DesiredReceiveSize, AppliedReceiveSize);
}
#elif PLATFORM_LINUX
void UHSDedicatedServerManager::InitializeLinuxSpecific()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: Linux 전용 초기화 시작"));

    if (!ServerConfig || !ServerSocket.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: Linux 초기화 건너뜀 - 서버 리소스가 준비되지 않았습니다"));
        return;
    }

    const FHSNetworkConfig& NetConfig = ServerConfig->GetNetworkConfig();

    ServerSocket->SetReusePort(true);

    int32 AppliedSendSize = 0;
    const int32 DesiredSendSize = FMath::Max(NetConfig.SendBufferSize * 2, NetConfig.SendBufferSize);
    if (!ServerSocket->SetSendBufferSize(DesiredSendSize, AppliedSendSize))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: (Linux) 송신 버퍼 크기 설정 실패 (요청: %d)"), DesiredSendSize);
    }

    int32 AppliedReceiveSize = 0;
    const int32 DesiredReceiveSize = FMath::Max(NetConfig.ReceiveBufferSize * 2, NetConfig.ReceiveBufferSize);
    if (!ServerSocket->SetReceiveBufferSize(DesiredReceiveSize, AppliedReceiveSize))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: (Linux) 수신 버퍼 크기 설정 실패 (요청: %d)"), DesiredReceiveSize);
    }

    ServerSocket->SetLinger(true, 1);

    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: Linux 전용 네트워크 튜닝 완료 (Send %d/%d, Receive %d/%d)"),
           DesiredSendSize, AppliedSendSize, DesiredReceiveSize, AppliedReceiveSize);
}
#endif
