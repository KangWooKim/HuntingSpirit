#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "../../HuntingSpiritGameMode.h"
#include "HSServerConfig.h"
#include "HSDedicatedServerManager.generated.h"

UENUM(BlueprintType)
enum class EHSServerStatus : uint8
{
    Offline,
    Starting,
    Online,
    Stopping,
    Restarting,
    Maintenance,
    Error
};

UENUM(BlueprintType)
enum class EHSServerEnvironment : uint8
{
    Development,
    Staging,
    Production,
    LoadTest
};

USTRUCT(BlueprintType)
struct FHSServerPerformanceMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float CPUUsagePercent = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float MemoryUsageMB = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float NetworkInKBps = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float NetworkOutKBps = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float TickRate = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    int32 ConnectedPlayers = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    int32 ActiveGameSessions = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float AverageLatency = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float PacketLossPercent = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    FDateTime LastUpdateTime;

    FHSServerPerformanceMetrics()
    {
        CPUUsagePercent = 0.0f;
        MemoryUsageMB = 0.0f;
        NetworkInKBps = 0.0f;
        NetworkOutKBps = 0.0f;
        TickRate = 0.0f;
        ConnectedPlayers = 0;
        ActiveGameSessions = 0;
        AverageLatency = 0.0f;
        PacketLossPercent = 0.0f;
        LastUpdateTime = FDateTime::Now();
    }
};

USTRUCT(BlueprintType)
struct FHSGameSessionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    FString SessionID;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    FString SessionName;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    int32 CurrentPlayers = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    int32 MaxPlayers = 4;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    FString MapName;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    FString GameMode;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    FDateTime CreationTime;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    float SessionDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    bool bIsActive = true;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    bool bIsRanked = false;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    TArray<FString> PlayerIDs;

    FHSGameSessionInfo()
    {
        CurrentPlayers = 0;
        MaxPlayers = 4;
        CreationTime = FDateTime::Now();
        SessionDuration = 0.0f;
        bIsActive = true;
        bIsRanked = false;
    }
};

USTRUCT(BlueprintType)
struct FHSPlayerConnectionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString PlayerID;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString IPAddress;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    int32 Port = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    float Ping = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FDateTime ConnectionTime;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    float ConnectionDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    bool bIsAuthenticated = false;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString SessionID;

    FHSPlayerConnectionInfo()
    {
        Port = 0;
        Ping = 0.0f;
        ConnectionTime = FDateTime::Now();
        ConnectionDuration = 0.0f;
        bIsAuthenticated = false;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnServerStatusChanged, EHSServerStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerConnected, const FHSPlayerConnectionInfo&, PlayerInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDisconnected, const FString&, PlayerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameSessionCreated, const FHSGameSessionInfo&, SessionInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameSessionEnded, const FString&, SessionID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerformanceMetricsUpdated, const FHSServerPerformanceMetrics&, Metrics);

/**
 * 데디케이트 서버 관리자 - 서버 생명주기 및 성능 모니터링
 * 자동 복구, 리소스 관리, 세션 최적화 적용
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSDedicatedServerManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSDedicatedServerManager();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 서버 생명주기 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Server Management")
    bool StartServer(EHSServerEnvironment Environment = EHSServerEnvironment::Development);

    UFUNCTION(BlueprintCallable, Category = "Server Management")
    void StopServer(bool bGracefulShutdown = true);

    UFUNCTION(BlueprintCallable, Category = "Server Management")
    void RestartServer();

    UFUNCTION(BlueprintCallable, Category = "Server Management")
    void SetMaintenanceMode(bool bEnabled);

    // === 서버 상태 조회 ===
    UFUNCTION(BlueprintPure, Category = "Server Management")
    EHSServerStatus GetServerStatus() const { return CurrentServerStatus; }

    UFUNCTION(BlueprintPure, Category = "Server Management")
    bool IsServerRunning() const { return CurrentServerStatus == EHSServerStatus::Online; }

    UFUNCTION(BlueprintPure, Category = "Server Management")
    FHSServerPerformanceMetrics GetPerformanceMetrics() const { return CurrentMetrics; }

    // === 게임 세션 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Session Management")
    FString CreateGameSession(const FString& SessionName, const FString& MapName, int32 MaxPlayers = 4, bool bRanked = false);

    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool EndGameSession(const FString& SessionID);

    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool JoinGameSession(const FString& SessionID, const FString& PlayerID);

    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool LeaveGameSession(const FString& SessionID, const FString& PlayerID);

    UFUNCTION(BlueprintPure, Category = "Session Management")
    TArray<FHSGameSessionInfo> GetActiveGameSessions() const;

    UFUNCTION(BlueprintPure, Category = "Session Management")
    FHSGameSessionInfo GetGameSessionInfo(const FString& SessionID) const;

    // === 플레이어 연결 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Connection Management")
    bool AuthenticatePlayer(const FString& PlayerID, const FString& AuthToken);

    UFUNCTION(BlueprintCallable, Category = "Connection Management")
    void DisconnectPlayer(const FString& PlayerID, const FString& Reason = TEXT(""));

    UFUNCTION(BlueprintPure, Category = "Connection Management")
    TArray<FHSPlayerConnectionInfo> GetConnectedPlayers() const;

    UFUNCTION(BlueprintPure, Category = "Connection Management")
    FHSPlayerConnectionInfo GetPlayerConnectionInfo(const FString& PlayerID) const;

    UFUNCTION(BlueprintPure, Category = "Connection Management")
    int32 GetConnectedPlayerCount() const;

    UFUNCTION(BlueprintCallable, Category = "Connection Management")
    void RegisterPlayerConnection(const FString& PlayerID, const FString& PlayerName, const FString& IPAddress, int32 Port);

    UFUNCTION(BlueprintCallable, Category = "Connection Management")
    void UnregisterPlayerConnection(const FString& PlayerID, const FString& Reason = TEXT("Player Logout"));

    // === 서버 설정 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Server Configuration")
    void LoadServerConfig(EHSServerEnvironment Environment);

    UFUNCTION(BlueprintCallable, Category = "Server Configuration")
    void SaveServerConfig();

    UFUNCTION(BlueprintCallable, Category = "Server Configuration")
    UHSServerConfig* GetServerConfig() const { return ServerConfig; }

    // === 성능 모니터링 ===
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void StartPerformanceMonitoring();

    UFUNCTION(BlueprintCallable, Category = "Performance")
    void StopPerformanceMonitoring();

    UFUNCTION(BlueprintCallable, Category = "Performance")
    void UpdatePerformanceMetrics();

    // === 자동 관리 기능 ===
    UFUNCTION(BlueprintCallable, Category = "Automation")
    void EnableAutoSessionCleanup(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Automation")
    void EnableAutoPlayerTimeout(bool bEnabled, float TimeoutSeconds = 300.0f);

    UFUNCTION(BlueprintCallable, Category = "Automation")
    void EnableAutoPerformanceOptimization(bool bEnabled);

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Server Events")
    FOnServerStatusChanged OnServerStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerConnected OnPlayerConnected;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerDisconnected OnPlayerDisconnected;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnGameSessionCreated OnGameSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnGameSessionEnded OnGameSessionEnded;

    UPROPERTY(BlueprintAssignable, Category = "Performance Events")
    FOnPerformanceMetricsUpdated OnPerformanceMetricsUpdated;

protected:
    // === 내부 상태 ===
    UPROPERTY()
    EHSServerStatus CurrentServerStatus;

    UPROPERTY()
    EHSServerEnvironment CurrentEnvironment;

    UPROPERTY()
    TObjectPtr<UHSServerConfig> ServerConfig;

    UPROPERTY()
    FHSServerPerformanceMetrics CurrentMetrics;

    // === 세션 및 연결 관리 ===
    UPROPERTY()
    TMap<FString, FHSGameSessionInfo> ActiveSessions;

    UPROPERTY()
    TMap<FString, FHSPlayerConnectionInfo> ConnectedPlayers;

    // === 서버 관리 함수 ===
    void UpdateServerStatus(EHSServerStatus NewStatus);
    bool InitializeNetworkListener();
    void ShutdownNetworkListener();
    
    // === 세션 관리 내부 함수 ===
    FString GenerateSessionID() const;
    void CleanupInactiveSessions();
    void ValidateSessionIntegrity();
    
    // === 연결 관리 내부 함수 ===
    void HandlePlayerConnection(const FString& PlayerID, const FString& IPAddress, int32 Port, const FString& PlayerName, bool bAuthenticated);
    void HandlePlayerDisconnection(const FString& PlayerID, const FString& Reason);
    void UpdatePlayerConnectionMetrics();
    
    // === 성능 모니터링 ===
    void CollectCPUMetrics();
    void CollectMemoryMetrics();
    void CollectNetworkMetrics();
    void CollectGameplayMetrics();
    
    // === 자동 관리 ===
    void ProcessAutoSessionCleanup();
    void ProcessAutoPlayerTimeout();
    void ProcessAutoPerformanceOptimization();
    
    // === 타이머 핸들 ===
    FTimerHandle PerformanceMonitoringTimerHandle;
    FTimerHandle SessionCleanupTimerHandle;
    FTimerHandle PlayerTimeoutTimerHandle;
    FTimerHandle AutoOptimizationTimerHandle;
    
    // === 네트워킹 ===
    TSharedPtr<FSocket> ServerSocket;
    TSharedPtr<FInternetAddr> ServerAddress;
    
    // === 자동 관리 설정 ===
    bool bAutoSessionCleanupEnabled;
    bool bAutoPlayerTimeoutEnabled;
    bool bAutoPerformanceOptimizationEnabled;
    float PlayerTimeoutSeconds;
    
    // === 성능 최적화 캐싱 ===
    mutable TMap<FString, FHSGameSessionInfo> SessionInfoCache;
    mutable TMap<FString, FHSPlayerConnectionInfo> PlayerInfoCache;
    mutable float LastMetricsUpdateTime;
    mutable uint64 LastRecordedInBytes = 0;
    mutable uint64 LastRecordedOutBytes = 0;
    mutable double LastNetworkSampleSeconds = 0.0;
    
    // === 오브젝트 풀링 ===
    TArray<FHSGameSessionInfo> SessionPool;
    TArray<FHSPlayerConnectionInfo> PlayerPool;
    
    // === 자동 복구 시스템 ===
    int32 ConsecutiveErrorCount;
    FDateTime LastErrorTime;
    
    // === 상수 설정 ===
    static constexpr float PerformanceUpdateInterval = 5.0f;
    static constexpr float SessionCleanupInterval = 60.0f;
    static constexpr float PlayerTimeoutCheckInterval = 30.0f;
    static constexpr float AutoOptimizationInterval = 120.0f;
    static constexpr int32 MaxConsecutiveErrors = 3;
    static constexpr float MetricsCacheDuration = 1.0f;

private:
    // === 내부 유틸리티 ===
    void InitializeServerSocket();
    void ConfigureSocketOptions();
    bool BindServerSocket();
    void StartListening();
    
    // === 에러 처리 및 복구 ===
    void HandleServerError(const FString& ErrorMessage);
    void AttemptAutoRecovery();
    bool ValidateServerState(bool bRequireNetworkReady = true) const;
    
    // === 리소스 관리 ===
    void AllocateServerResources();
    void DeallocateServerResources();
    void OptimizeMemoryUsage();
    
    // === 보안 및 검증 ===
    bool ValidatePlayerAuthentication(const FString& PlayerID, const FString& AuthToken) const;
    bool ValidateSessionAccess(const FString& SessionID, const FString& PlayerID) const;
    void LogSecurityEvent(const FString& Event, const FString& PlayerID = TEXT(""));
    
    // === 내부 상태 검증 ===
    bool IsServerInitialized() const;
    bool IsNetworkReady() const;
    bool IsConfigurationValid() const;
    
    // === 플랫폼별 구현 ===
#if PLATFORM_WINDOWS
    void InitializeWindowsSpecific();
#elif PLATFORM_LINUX
    void InitializeLinuxSpecific();
#endif
    
    // === 스레드 세이프 처리 ===
    mutable FCriticalSection ServerMutex;
    mutable FCriticalSection SessionMutex;
    mutable FCriticalSection PlayerMutex;
};
