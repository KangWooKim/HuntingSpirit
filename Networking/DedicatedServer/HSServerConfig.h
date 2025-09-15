#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DataAsset.h"
#include "HSServerConfig.generated.h"

UENUM(BlueprintType)
enum class EHSLogLevel : uint8
{
    None,
    Fatal,
    Error,
    Warning,
    Info,
    Debug,
    Verbose
};

UENUM(BlueprintType)
enum class EHSAuthenticationMethod : uint8
{
    None,
    Basic,
    Token,
    Steam,
    Epic,
    Custom
};

USTRUCT(BlueprintType)
struct FHSNetworkConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    FString ServerIP = TEXT("0.0.0.0");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 ServerPort = 7777;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 MaxConnections = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 MaxPlayersPerSession = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    float TickRate = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    float ClientTimeout = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    float KeepAliveInterval = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 MaxPacketSize = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    bool bEnableCompression = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    bool bEnableEncryption = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 SendBufferSize = 65536;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 ReceiveBufferSize = 65536;

    FHSNetworkConfig()
    {
        ServerIP = TEXT("0.0.0.0");
        ServerPort = 7777;
        MaxConnections = 100;
        MaxPlayersPerSession = 4;
        TickRate = 60.0f;
        ClientTimeout = 30.0f;
        KeepAliveInterval = 10.0f;
        MaxPacketSize = 1024;
        bEnableCompression = true;
        bEnableEncryption = false;
        SendBufferSize = 65536;
        ReceiveBufferSize = 65536;
    }
};

USTRUCT(BlueprintType)
struct FHSPerformanceConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float MaxCPUUsage = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float MaxMemoryUsage = 4096.0f; // MB

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxActorsPerWorld = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float GarbageCollectionInterval = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableObjectPooling = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableMemoryOptimization = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableCulling = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float CullingDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxConcurrentSessions = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float SessionCleanupInterval = 300.0f;

    FHSPerformanceConfig()
    {
        MaxCPUUsage = 80.0f;
        MaxMemoryUsage = 4096.0f;
        MaxActorsPerWorld = 10000;
        GarbageCollectionInterval = 60.0f;
        bEnableObjectPooling = true;
        bEnableMemoryOptimization = true;
        bEnableCulling = true;
        CullingDistance = 5000.0f;
        MaxConcurrentSessions = 50;
        SessionCleanupInterval = 300.0f;
    }
};

USTRUCT(BlueprintType)
struct FHSSecurityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    EHSAuthenticationMethod AuthMethod = EHSAuthenticationMethod::Token;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    FString AuthServerURL;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    float TokenValidityDuration = 3600.0f; // 1시간

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    bool bEnableAntiCheat = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    bool bEnableRateLimiting = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    int32 MaxRequestsPerMinute = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    bool bBlockPrivateIPs = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    TArray<FString> BlockedIPRanges;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    TArray<FString> AllowedIPRanges;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    bool bEnableSSL = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    FString SSLCertificatePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security")
    FString SSLPrivateKeyPath;

    FHSSecurityConfig()
    {
        AuthMethod = EHSAuthenticationMethod::Token;
        TokenValidityDuration = 3600.0f;
        bEnableAntiCheat = true;
        bEnableRateLimiting = true;
        MaxRequestsPerMinute = 100;
        bBlockPrivateIPs = false;
        bEnableSSL = false;
    }
};

USTRUCT(BlueprintType)
struct FHSLoggingConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    EHSLogLevel LogLevel = EHSLogLevel::Info;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    bool bLogToFile = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    bool bLogToConsole = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    FString LogFilePath = TEXT("Logs/HuntingSpirit_Server.log");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    int32 MaxLogFileSize = 100; // MB

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    int32 MaxLogFiles = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    bool bEnableRemoteLogging = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    FString RemoteLoggingURL;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    bool bLogPlayerActions = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    bool bLogPerformanceMetrics = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
    bool bLogSecurityEvents = true;

    FHSLoggingConfig()
    {
        LogLevel = EHSLogLevel::Info;
        bLogToFile = true;
        bLogToConsole = true;
        LogFilePath = TEXT("Logs/HuntingSpirit_Server.log");
        MaxLogFileSize = 100;
        MaxLogFiles = 10;
        bEnableRemoteLogging = false;
        bLogPlayerActions = true;
        bLogPerformanceMetrics = true;
        bLogSecurityEvents = true;
    }
};

USTRUCT(BlueprintType)
struct FHSGameplayConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    float DefaultSessionDuration = 1800.0f; // 30분

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    float MaxSessionDuration = 3600.0f; // 1시간

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    bool bAllowSpectators = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    int32 MaxSpectatorsPerSession = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    bool bEnableFriendlyFire = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    float PlayerRespawnTime = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    bool bAllowTeamSwitch = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    float TeamSwitchCooldown = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    TArray<FString> AvailableMaps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    TArray<FString> AvailableGameModes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    bool bEnableProgression = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    bool bPersistPlayerData = true;

    FHSGameplayConfig()
    {
        DefaultSessionDuration = 1800.0f;
        MaxSessionDuration = 3600.0f;
        bAllowSpectators = true;
        MaxSpectatorsPerSession = 10;
        bEnableFriendlyFire = false;
        PlayerRespawnTime = 10.0f;
        bAllowTeamSwitch = false;
        TeamSwitchCooldown = 300.0f;
        bEnableProgression = true;
        bPersistPlayerData = true;
    }
};

USTRUCT(BlueprintType)
struct FHSMonitoringConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    bool bEnablePerformanceMonitoring = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    float MonitoringInterval = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    bool bEnableHealthCheck = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    int32 HealthCheckPort = 8080;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    FString HealthCheckEndpoint = TEXT("/health");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    bool bEnableMetricsEndpoint = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    FString MetricsEndpoint = TEXT("/metrics");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    bool bEnableAlerts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    FString AlertingURL;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    float CPUAlertThreshold = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring")
    float MemoryAlertThreshold = 90.0f;

    FHSMonitoringConfig()
    {
        bEnablePerformanceMonitoring = true;
        MonitoringInterval = 5.0f;
        bEnableHealthCheck = true;
        HealthCheckPort = 8080;
        HealthCheckEndpoint = TEXT("/health");
        bEnableMetricsEndpoint = true;
        MetricsEndpoint = TEXT("/metrics");
        bEnableAlerts = true;
        CPUAlertThreshold = 90.0f;
        MemoryAlertThreshold = 90.0f;
    }
};

/**
 * 서버 설정 관리자 - 환경별 서버 구성 및 실시간 업데이트
 * 현업 최적화: 환경별 설정, 검증 시스템, 핫 리로드
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSServerConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UHSServerConfig();

    // === 기본 서버 정보 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Info")
    FString ServerName = TEXT("HuntingSpirit Server");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Info")
    FString ServerDescription = TEXT("Cooperative Roguelike RPG Server");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Info")
    FString ServerVersion = TEXT("1.0.0");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Info")
    FString AdminContact = TEXT("admin@huntingspirit.com");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Info")
    FString Region = TEXT("Global");

    // === 구성 카테고리 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Configuration")
    FHSNetworkConfig NetworkConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    FHSPerformanceConfig PerformanceConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Security Configuration")
    FHSSecurityConfig SecurityConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging Configuration")
    FHSLoggingConfig LoggingConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Configuration")
    FHSGameplayConfig GameplayConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monitoring Configuration")
    FHSMonitoringConfig MonitoringConfig;

    // === 설정 관리 함수 ===
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool LoadConfigurationFromFile(const FString& ConfigFilePath);

    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool SaveConfigurationToFile(const FString& ConfigFilePath) const;

    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool ValidateConfiguration() const;

    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void ResetToDefaults();

    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void ApplyEnvironmentOverrides(const FString& Environment);

    // === 환경별 설정 ===
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SetDevelopmentEnvironment();

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SetStagingEnvironment();

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SetProductionEnvironment();

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SetLoadTestEnvironment();

    // === 실시간 설정 업데이트 ===
    UFUNCTION(BlueprintCallable, Category = "Runtime")
    bool UpdateNetworkConfig(const FHSNetworkConfig& NewConfig);

    UFUNCTION(BlueprintCallable, Category = "Runtime")
    bool UpdatePerformanceConfig(const FHSPerformanceConfig& NewConfig);

    UFUNCTION(BlueprintCallable, Category = "Runtime")
    bool UpdateSecurityConfig(const FHSSecurityConfig& NewConfig);

    UFUNCTION(BlueprintCallable, Category = "Runtime")
    bool UpdateLoggingConfig(const FHSLoggingConfig& NewConfig);

    UFUNCTION(BlueprintCallable, Category = "Runtime")
    bool UpdateGameplayConfig(const FHSGameplayConfig& NewConfig);

    UFUNCTION(BlueprintCallable, Category = "Runtime")
    bool UpdateMonitoringConfig(const FHSMonitoringConfig& NewConfig);

    // === 설정 조회 ===
    UFUNCTION(BlueprintPure, Category = "Configuration")
    FHSNetworkConfig GetNetworkConfig() const { return NetworkConfig; }

    UFUNCTION(BlueprintPure, Category = "Configuration")
    FHSPerformanceConfig GetPerformanceConfig() const { return PerformanceConfig; }

    UFUNCTION(BlueprintPure, Category = "Configuration")
    FHSSecurityConfig GetSecurityConfig() const { return SecurityConfig; }

    UFUNCTION(BlueprintPure, Category = "Configuration")
    FHSLoggingConfig GetLoggingConfig() const { return LoggingConfig; }

    UFUNCTION(BlueprintPure, Category = "Configuration")
    FHSGameplayConfig GetGameplayConfig() const { return GameplayConfig; }

    UFUNCTION(BlueprintPure, Category = "Configuration")
    FHSMonitoringConfig GetMonitoringConfig() const { return MonitoringConfig; }

    // === 설정 검증 함수 ===
    UFUNCTION(BlueprintPure, Category = "Validation")
    bool ValidateNetworkConfig(const FHSNetworkConfig& Config) const;

    UFUNCTION(BlueprintPure, Category = "Validation")
    bool ValidatePerformanceConfig(const FHSPerformanceConfig& Config) const;

    UFUNCTION(BlueprintPure, Category = "Validation")
    bool ValidateSecurityConfig(const FHSSecurityConfig& Config) const;

    UFUNCTION(BlueprintPure, Category = "Validation")
    bool ValidateLoggingConfig(const FHSLoggingConfig& Config) const;

    UFUNCTION(BlueprintPure, Category = "Validation")
    bool ValidateGameplayConfig(const FHSGameplayConfig& Config) const;

    UFUNCTION(BlueprintPure, Category = "Validation")
    bool ValidateMonitoringConfig(const FHSMonitoringConfig& Config) const;

    // === 설정 정보 ===
    UFUNCTION(BlueprintPure, Category = "Info")
    FString GetConfigurationSummary() const;

    UFUNCTION(BlueprintPure, Category = "Info")
    TArray<FString> GetValidationErrors() const;

    UFUNCTION(BlueprintPure, Category = "Info")
    bool IsConfigurationChanged() const;

protected:
    // === 내부 검증 함수 ===
    bool ValidateNetworkSettings() const;
    bool ValidatePerformanceSettings() const;
    bool ValidateSecuritySettings() const;
    bool ValidateLoggingSettings() const;
    bool ValidateGameplaySettings() const;
    bool ValidateMonitoringSettings() const;

    // === 환경별 오버라이드 ===
    void ApplyDevelopmentOverrides();
    void ApplyStagingOverrides();
    void ApplyProductionOverrides();
    void ApplyLoadTestOverrides();

    // === 파일 I/O ===
    FString SerializeToJSON() const;
    bool DeserializeFromJSON(const FString& JSONString);

    // === 설정 변경 추적 ===
    void MarkConfigurationChanged();
    void ClearConfigurationChanged();

private:
    // === 내부 상태 ===
    bool bConfigurationChanged;
    FDateTime LastModifiedTime;
    
    // === 검증 결과 캐시 ===
    mutable TArray<FString> CachedValidationErrors;
    mutable bool bValidationCacheValid;
    
    // === 기본값 저장 ===
    FHSNetworkConfig DefaultNetworkConfig;
    FHSPerformanceConfig DefaultPerformanceConfig;
    FHSSecurityConfig DefaultSecurityConfig;
    FHSLoggingConfig DefaultLoggingConfig;
    FHSGameplayConfig DefaultGameplayConfig;
    FHSMonitoringConfig DefaultMonitoringConfig;
    
    // === 유틸리티 함수 ===
    void InitializeDefaults();
    void ClearValidationCache() const;
    FString ConfigToString(const FHSNetworkConfig& Config) const;
    FString ConfigToString(const FHSPerformanceConfig& Config) const;
    FString ConfigToString(const FHSSecurityConfig& Config) const;
    FString ConfigToString(const FHSLoggingConfig& Config) const;
    FString ConfigToString(const FHSGameplayConfig& Config) const;
    FString ConfigToString(const FHSMonitoringConfig& Config) const;
};
