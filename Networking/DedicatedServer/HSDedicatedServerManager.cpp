#include "HSDedicatedServerManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"

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
    
    // 네트워크 초기화
    InitializeNetworkListener();
    
    // 성능 모니터링 시작
    StartPerformanceMonitoring();
    
    // 자동 관리 기능 활성화
    EnableAutoSessionCleanup(true);
    EnableAutoPlayerTimeout(true);
    EnableAutoPerformanceOptimization(true);
    
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
    
    if (CurrentServerStatus != EHSServerStatus::Offline)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSDedicatedServerManager: 서버가 이미 실행 중이거나 시작 중입니다"));
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 시작 중... 환경: %d"), (int32)Environment);
    
    UpdateServerStatus(EHSServerStatus::Starting);
    CurrentEnvironment = Environment;
    
    // 환경별 설정 로드
    LoadServerConfig(Environment);
    
    if (!ValidateServerState())
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
        return false;
    }
    
    // 플랫폼별 초기화
#if PLATFORM_WINDOWS
    InitializeWindowsSpecific();
#elif PLATFORM_LINUX
    InitializeLinuxSpecific();
#endif
    
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
    if (ServerConfig)
    {
        // 실제 구현에서는 파일 시스템에 저장
        UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: 서버 설정 저장 완료"));
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

void UHSDedicatedServerManager::HandlePlayerConnection(const FString& PlayerID, const FString& IPAddress, int32 Port)
{
    FScopeLock Lock(&PlayerMutex);
    
    FHSPlayerConnectionInfo PlayerInfo;
    PlayerInfo.PlayerID = PlayerID;
    PlayerInfo.PlayerName = FString::Printf(TEXT("Player_%s"), *PlayerID.Right(8));
    PlayerInfo.IPAddress = IPAddress;
    PlayerInfo.Port = Port;
    PlayerInfo.ConnectionTime = FDateTime::Now();
    PlayerInfo.bIsAuthenticated = false;
    
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
    
    for (auto& PlayerPair : ConnectedPlayers)
    {
        FHSPlayerConnectionInfo& PlayerInfo = PlayerPair.Value;
        
        // 연결 지속시간 업데이트
        PlayerInfo.ConnectionDuration = (FDateTime::Now() - PlayerInfo.ConnectionTime).GetTotalSeconds();
        
        // 핑 시뮬레이션 (실제 구현에서는 실제 핑 측정)
        PlayerInfo.Ping = FMath::RandRange(10.0f, 100.0f);
    }
}

void UHSDedicatedServerManager::CollectCPUMetrics()
{
    // 실제 구현에서는 플랫폼별 CPU 사용률 측정
    CurrentMetrics.CPUUsagePercent = FMath::RandRange(10.0f, 80.0f);
}

void UHSDedicatedServerManager::CollectMemoryMetrics()
{
    // 실제 구현에서는 실제 메모리 사용량 측정
    CurrentMetrics.MemoryUsageMB = FMath::RandRange(1024.0f, 4096.0f);
}

void UHSDedicatedServerManager::CollectNetworkMetrics()
{
    // 실제 구현에서는 실제 네트워크 사용량 측정
    CurrentMetrics.NetworkInKBps = FMath::RandRange(100.0f, 1000.0f);
    CurrentMetrics.NetworkOutKBps = FMath::RandRange(200.0f, 1500.0f);
}

void UHSDedicatedServerManager::CollectGameplayMetrics()
{
    CurrentMetrics.ConnectedPlayers = GetConnectedPlayerCount();
    CurrentMetrics.ActiveGameSessions = ActiveSessions.Num();
    CurrentMetrics.TickRate = GetWorld() ? (1.0f / GetWorld()->GetDeltaSeconds()) : 60.0f;
    CurrentMetrics.AverageLatency = FMath::RandRange(20.0f, 80.0f);
    CurrentMetrics.PacketLossPercent = FMath::RandRange(0.0f, 2.0f);
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
    
    // 실제 구현에서는 외부 인증 서버와 통신
    // 현재는 간단한 검증
    return AuthToken.Len() >= 32; // 최소 32자 토큰
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
    
    // 실제 구현에서는 보안 로그 시스템에 기록
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

bool UHSDedicatedServerManager::ValidateServerState() const
{
    if (!IsConfigurationValid())
    {
        return false;
    }
    
    if (!IsServerInitialized())
    {
        return false;
    }
    
    return true;
}

#if PLATFORM_WINDOWS
void UHSDedicatedServerManager::InitializeWindowsSpecific()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: Windows 전용 초기화"));
    // Windows 전용 서버 설정
}
#elif PLATFORM_LINUX
void UHSDedicatedServerManager::InitializeLinuxSpecific()
{
    UE_LOG(LogTemp, Log, TEXT("HSDedicatedServerManager: Linux 전용 초기화"));
    // Linux 전용 서버 설정
}
#endif
