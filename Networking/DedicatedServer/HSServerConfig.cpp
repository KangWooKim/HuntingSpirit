#include "HSServerConfig.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UHSServerConfig::UHSServerConfig()
{
    // 기본 서버 정보 설정
    ServerName = TEXT("HuntingSpirit Server");
    ServerDescription = TEXT("Cooperative Roguelike RPG Server");
    ServerVersion = TEXT("1.0.0");
    AdminContact = TEXT("admin@huntingspirit.com");
    Region = TEXT("Global");
    
    // 기본 설정 초기화
    NetworkConfig = FHSNetworkConfig();
    PerformanceConfig = FHSPerformanceConfig();
    SecurityConfig = FHSSecurityConfig();
    LoggingConfig = FHSLoggingConfig();
    GameplayConfig = FHSGameplayConfig();
    MonitoringConfig = FHSMonitoringConfig();
    
    // 내부 상태 초기화
    bConfigurationChanged = false;
    LastModifiedTime = FDateTime::Now();
    bValidationCacheValid = false;
    
    // 기본값 저장
    InitializeDefaults();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 서버 설정 객체 생성 완료"));
}

bool UHSServerConfig::LoadConfigurationFromFile(const FString& ConfigFilePath)
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 설정 파일 로드 중... %s"), *ConfigFilePath);
    
    // 파일 존재 확인
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSServerConfig: 설정 파일이 존재하지 않음 - %s"), *ConfigFilePath);
        return false;
    }
    
    // 파일 읽기
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *ConfigFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 설정 파일 읽기 실패 - %s"), *ConfigFilePath);
        return false;
    }
    
    // JSON 파싱
    if (!DeserializeFromJSON(JsonContent))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 설정 파일 파싱 실패"));
        return false;
    }
    
    // 설정 검증
    if (!ValidateConfiguration())
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 로드된 설정 검증 실패"));
        return false;
    }
    
    ClearConfigurationChanged();
    LastModifiedTime = FDateTime::Now();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 설정 파일 로드 완료"));
    return true;
}

bool UHSServerConfig::SaveConfigurationToFile(const FString& ConfigFilePath) const
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 설정 파일 저장 중... %s"), *ConfigFilePath);
    
    // 설정 검증
    if (!ValidateConfiguration())
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 저장할 설정이 유효하지 않음"));
        return false;
    }
    
    // JSON 직렬화
    FString JsonContent = SerializeToJSON();
    if (JsonContent.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: JSON 직렬화 실패"));
        return false;
    }
    
    // 파일 저장
    if (!FFileHelper::SaveStringToFile(JsonContent, *ConfigFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 설정 파일 저장 실패 - %s"), *ConfigFilePath);
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 설정 파일 저장 완료"));
    return true;
}

bool UHSServerConfig::ValidateConfiguration() const
{
    // 캐시된 검증 결과 사용
    if (bValidationCacheValid)
    {
        return CachedValidationErrors.Num() == 0;
    }
    
    CachedValidationErrors.Empty();
    
    // 각 카테고리별 검증
    if (!ValidateNetworkSettings())
    {
        CachedValidationErrors.Add(TEXT("네트워크 설정 검증 실패"));
    }
    
    if (!ValidatePerformanceSettings())
    {
        CachedValidationErrors.Add(TEXT("성능 설정 검증 실패"));
    }
    
    if (!ValidateSecuritySettings())
    {
        CachedValidationErrors.Add(TEXT("보안 설정 검증 실패"));
    }
    
    if (!ValidateLoggingSettings())
    {
        CachedValidationErrors.Add(TEXT("로깅 설정 검증 실패"));
    }
    
    if (!ValidateGameplaySettings())
    {
        CachedValidationErrors.Add(TEXT("게임플레이 설정 검증 실패"));
    }
    
    if (!ValidateMonitoringSettings())
    {
        CachedValidationErrors.Add(TEXT("모니터링 설정 검증 실패"));
    }
    
    bValidationCacheValid = true;
    
    bool bIsValid = CachedValidationErrors.Num() == 0;
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 설정 검증 완료 - %s"), 
           bIsValid ? TEXT("성공") : TEXT("실패"));
    
    return bIsValid;
}

void UHSServerConfig::ResetToDefaults()
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 설정을 기본값으로 재설정"));
    
    NetworkConfig = DefaultNetworkConfig;
    PerformanceConfig = DefaultPerformanceConfig;
    SecurityConfig = DefaultSecurityConfig;
    LoggingConfig = DefaultLoggingConfig;
    GameplayConfig = DefaultGameplayConfig;
    MonitoringConfig = DefaultMonitoringConfig;
    
    MarkConfigurationChanged();
    ClearValidationCache();
}

void UHSServerConfig::ApplyEnvironmentOverrides(const FString& Environment)
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 환경별 설정 오버라이드 적용 - %s"), *Environment);
    
    if (Environment == TEXT("Development"))
    {
        ApplyDevelopmentOverrides();
    }
    else if (Environment == TEXT("Staging"))
    {
        ApplyStagingOverrides();
    }
    else if (Environment == TEXT("Production"))
    {
        ApplyProductionOverrides();
    }
    else if (Environment == TEXT("LoadTest"))
    {
        ApplyLoadTestOverrides();
    }
    
    MarkConfigurationChanged();
    ClearValidationCache();
}

void UHSServerConfig::SetDevelopmentEnvironment()
{
    ApplyDevelopmentOverrides();
}

void UHSServerConfig::SetStagingEnvironment()
{
    ApplyStagingOverrides();
}

void UHSServerConfig::SetProductionEnvironment()
{
    ApplyProductionOverrides();
}

void UHSServerConfig::SetLoadTestEnvironment()
{
    ApplyLoadTestOverrides();
}

bool UHSServerConfig::UpdateNetworkConfig(const FHSNetworkConfig& NewConfig)
{
    if (!ValidateNetworkConfig(NewConfig))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 네트워크 설정 업데이트 실패 - 검증 오류"));
        return false;
    }
    
    NetworkConfig = NewConfig;
    MarkConfigurationChanged();
    ClearValidationCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 네트워크 설정 업데이트 완료"));
    return true;
}

bool UHSServerConfig::UpdatePerformanceConfig(const FHSPerformanceConfig& NewConfig)
{
    if (!ValidatePerformanceConfig(NewConfig))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 성능 설정 업데이트 실패 - 검증 오류"));
        return false;
    }
    
    PerformanceConfig = NewConfig;
    MarkConfigurationChanged();
    ClearValidationCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 성능 설정 업데이트 완료"));
    return true;
}

bool UHSServerConfig::UpdateSecurityConfig(const FHSSecurityConfig& NewConfig)
{
    if (!ValidateSecurityConfig(NewConfig))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 보안 설정 업데이트 실패 - 검증 오류"));
        return false;
    }
    
    SecurityConfig = NewConfig;
    MarkConfigurationChanged();
    ClearValidationCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 보안 설정 업데이트 완료"));
    return true;
}

bool UHSServerConfig::UpdateLoggingConfig(const FHSLoggingConfig& NewConfig)
{
    if (!ValidateLoggingConfig(NewConfig))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 로깅 설정 업데이트 실패 - 검증 오류"));
        return false;
    }
    
    LoggingConfig = NewConfig;
    MarkConfigurationChanged();
    ClearValidationCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 로깅 설정 업데이트 완료"));
    return true;
}

bool UHSServerConfig::UpdateGameplayConfig(const FHSGameplayConfig& NewConfig)
{
    if (!ValidateGameplayConfig(NewConfig))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 게임플레이 설정 업데이트 실패 - 검증 오류"));
        return false;
    }
    
    GameplayConfig = NewConfig;
    MarkConfigurationChanged();
    ClearValidationCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 게임플레이 설정 업데이트 완료"));
    return true;
}

bool UHSServerConfig::UpdateMonitoringConfig(const FHSMonitoringConfig& NewConfig)
{
    if (!ValidateMonitoringConfig(NewConfig))
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: 모니터링 설정 업데이트 실패 - 검증 오류"));
        return false;
    }
    
    MonitoringConfig = NewConfig;
    MarkConfigurationChanged();
    ClearValidationCache();
    
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 모니터링 설정 업데이트 완료"));
    return true;
}

bool UHSServerConfig::ValidateNetworkConfig(const FHSNetworkConfig& Config) const
{
    if (Config.ServerPort <= 0 || Config.ServerPort > 65535)
    {
        return false;
    }
    
    if (Config.MaxConnections <= 0 || Config.MaxConnections > 10000)
    {
        return false;
    }
    
    if (Config.TickRate <= 0 || Config.TickRate > 120)
    {
        return false;
    }
    
    if (Config.ClientTimeout <= 0 || Config.ClientTimeout > 300)
    {
        return false;
    }
    
    return true;
}

bool UHSServerConfig::ValidatePerformanceConfig(const FHSPerformanceConfig& Config) const
{
    if (Config.MaxCPUUsage <= 0 || Config.MaxCPUUsage > 100)
    {
        return false;
    }
    
    if (Config.MaxMemoryUsage <= 0 || Config.MaxMemoryUsage > 32768) // 32GB 최대
    {
        return false;
    }
    
    if (Config.MaxConcurrentSessions <= 0 || Config.MaxConcurrentSessions > 1000)
    {
        return false;
    }
    
    return true;
}

bool UHSServerConfig::ValidateSecurityConfig(const FHSSecurityConfig& Config) const
{
    if (Config.TokenValidityDuration <= 0 || Config.TokenValidityDuration > 86400) // 24시간 최대
    {
        return false;
    }
    
    if (Config.MaxRequestsPerMinute <= 0 || Config.MaxRequestsPerMinute > 10000)
    {
        return false;
    }
    
    return true;
}

bool UHSServerConfig::ValidateLoggingConfig(const FHSLoggingConfig& Config) const
{
    if (Config.LogFilePath.IsEmpty())
    {
        return false;
    }
    
    if (Config.MaxLogFileSize <= 0 || Config.MaxLogFileSize > 1000) // 1GB 최대
    {
        return false;
    }
    
    if (Config.MaxLogFiles <= 0 || Config.MaxLogFiles > 100)
    {
        return false;
    }
    
    return true;
}

bool UHSServerConfig::ValidateGameplayConfig(const FHSGameplayConfig& Config) const
{
    if (Config.DefaultSessionDuration <= 0 || Config.DefaultSessionDuration > 7200) // 2시간 최대
    {
        return false;
    }
    
    if (Config.MaxSessionDuration <= 0 || Config.MaxSessionDuration > 14400) // 4시간 최대
    {
        return false;
    }
    
    if (Config.PlayerRespawnTime < 0 || Config.PlayerRespawnTime > 300) // 5분 최대
    {
        return false;
    }
    
    return true;
}

bool UHSServerConfig::ValidateMonitoringConfig(const FHSMonitoringConfig& Config) const
{
    if (Config.MonitoringInterval <= 0 || Config.MonitoringInterval > 60)
    {
        return false;
    }
    
    if (Config.HealthCheckPort <= 0 || Config.HealthCheckPort > 65535)
    {
        return false;
    }
    
    if (Config.CPUAlertThreshold <= 0 || Config.CPUAlertThreshold > 100)
    {
        return false;
    }
    
    if (Config.MemoryAlertThreshold <= 0 || Config.MemoryAlertThreshold > 100)
    {
        return false;
    }
    
    return true;
}

FString UHSServerConfig::GetConfigurationSummary() const
{
    FString Summary;
    Summary += FString::Printf(TEXT("서버: %s (%s)\n"), *ServerName, *ServerVersion);
    Summary += FString::Printf(TEXT("네트워크: %s:%d (최대 %d 연결)\n"), 
                              *NetworkConfig.ServerIP, NetworkConfig.ServerPort, NetworkConfig.MaxConnections);
    Summary += FString::Printf(TEXT("성능: CPU %.1f%%, 메모리 %.1fMB\n"), 
                              PerformanceConfig.MaxCPUUsage, PerformanceConfig.MaxMemoryUsage);
    Summary += FString::Printf(TEXT("보안: %s 인증\n"), 
                              SecurityConfig.AuthMethod == EHSAuthenticationMethod::Token ? TEXT("토큰") : TEXT("기본"));
    Summary += FString::Printf(TEXT("로깅: %s 레벨\n"), 
                              LoggingConfig.LogLevel == EHSLogLevel::Info ? TEXT("정보") : TEXT("기타"));
    Summary += FString::Printf(TEXT("마지막 수정: %s\n"), *LastModifiedTime.ToString());
    
    return Summary;
}

TArray<FString> UHSServerConfig::GetValidationErrors() const
{
    // 검증이 수행되지 않았다면 검증 실행
    if (!bValidationCacheValid)
    {
        ValidateConfiguration();
    }
    
    return CachedValidationErrors;
}

bool UHSServerConfig::IsConfigurationChanged() const
{
    return bConfigurationChanged;
}

bool UHSServerConfig::ValidateNetworkSettings() const
{
    return ValidateNetworkConfig(NetworkConfig);
}

bool UHSServerConfig::ValidatePerformanceSettings() const
{
    return ValidatePerformanceConfig(PerformanceConfig);
}

bool UHSServerConfig::ValidateSecuritySettings() const
{
    return ValidateSecurityConfig(SecurityConfig);
}

bool UHSServerConfig::ValidateLoggingSettings() const
{
    return ValidateLoggingConfig(LoggingConfig);
}

bool UHSServerConfig::ValidateGameplaySettings() const
{
    return ValidateGameplayConfig(GameplayConfig);
}

bool UHSServerConfig::ValidateMonitoringSettings() const
{
    return ValidateMonitoringConfig(MonitoringConfig);
}

void UHSServerConfig::ApplyDevelopmentOverrides()
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 개발 환경 설정 적용"));
    
    // 개발 환경에 최적화된 설정
    NetworkConfig.MaxConnections = 50;
    NetworkConfig.TickRate = 30.0f;
    
    PerformanceConfig.MaxCPUUsage = 70.0f;
    PerformanceConfig.MaxMemoryUsage = 2048.0f;
    PerformanceConfig.MaxConcurrentSessions = 10;
    
    SecurityConfig.bEnableAntiCheat = false;
    SecurityConfig.bEnableRateLimiting = false;
    
    LoggingConfig.LogLevel = EHSLogLevel::Debug;
    LoggingConfig.bLogToConsole = true;
    
    MonitoringConfig.MonitoringInterval = 10.0f;
    MonitoringConfig.bEnableAlerts = false;
}

void UHSServerConfig::ApplyStagingOverrides()
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 스테이징 환경 설정 적용"));
    
    // 스테이징 환경에 최적화된 설정
    NetworkConfig.MaxConnections = 100;
    NetworkConfig.TickRate = 60.0f;
    
    PerformanceConfig.MaxCPUUsage = 80.0f;
    PerformanceConfig.MaxMemoryUsage = 4096.0f;
    PerformanceConfig.MaxConcurrentSessions = 25;
    
    SecurityConfig.bEnableAntiCheat = true;
    SecurityConfig.bEnableRateLimiting = true;
    
    LoggingConfig.LogLevel = EHSLogLevel::Info;
    LoggingConfig.bLogToConsole = false;
    
    MonitoringConfig.MonitoringInterval = 5.0f;
    MonitoringConfig.bEnableAlerts = true;
}

void UHSServerConfig::ApplyProductionOverrides()
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 프로덕션 환경 설정 적용"));
    
    // 프로덕션 환경에 최적화된 설정
    NetworkConfig.MaxConnections = 500;
    NetworkConfig.TickRate = 60.0f;
    NetworkConfig.bEnableCompression = true;
    NetworkConfig.bEnableEncryption = true;
    
    PerformanceConfig.MaxCPUUsage = 85.0f;
    PerformanceConfig.MaxMemoryUsage = 8192.0f;
    PerformanceConfig.MaxConcurrentSessions = 100;
    PerformanceConfig.bEnableObjectPooling = true;
    PerformanceConfig.bEnableMemoryOptimization = true;
    
    SecurityConfig.bEnableAntiCheat = true;
    SecurityConfig.bEnableRateLimiting = true;
    SecurityConfig.bEnableSSL = true;
    SecurityConfig.MaxRequestsPerMinute = 500;
    
    LoggingConfig.LogLevel = EHSLogLevel::Warning;
    LoggingConfig.bLogToConsole = false;
    LoggingConfig.bEnableRemoteLogging = true;
    
    MonitoringConfig.MonitoringInterval = 5.0f;
    MonitoringConfig.bEnableAlerts = true;
    MonitoringConfig.CPUAlertThreshold = 90.0f;
    MonitoringConfig.MemoryAlertThreshold = 90.0f;
}

void UHSServerConfig::ApplyLoadTestOverrides()
{
    UE_LOG(LogTemp, Log, TEXT("HSServerConfig: 로드 테스트 환경 설정 적용"));
    
    // 로드 테스트에 최적화된 설정
    NetworkConfig.MaxConnections = 1000;
    NetworkConfig.TickRate = 120.0f;
    NetworkConfig.bEnableCompression = true;
    
    PerformanceConfig.MaxCPUUsage = 95.0f;
    PerformanceConfig.MaxMemoryUsage = 16384.0f; // 16GB
    PerformanceConfig.MaxConcurrentSessions = 500;
    PerformanceConfig.bEnableObjectPooling = true;
    PerformanceConfig.bEnableMemoryOptimization = true;
    PerformanceConfig.bEnableCulling = true;
    
    SecurityConfig.bEnableAntiCheat = false; // 테스트 성능을 위해 비활성화
    SecurityConfig.bEnableRateLimiting = false;
    
    LoggingConfig.LogLevel = EHSLogLevel::Error; // 최소 로깅
    LoggingConfig.bLogToConsole = false;
    LoggingConfig.bLogPlayerActions = false;
    
    MonitoringConfig.MonitoringInterval = 1.0f; // 더 자주 모니터링
    MonitoringConfig.bEnableAlerts = true;
    MonitoringConfig.CPUAlertThreshold = 95.0f;
    MonitoringConfig.MemoryAlertThreshold = 95.0f;
}

FString UHSServerConfig::SerializeToJSON() const
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
    // 기본 서버 정보
    RootObject->SetStringField(TEXT("ServerName"), ServerName);
    RootObject->SetStringField(TEXT("ServerDescription"), ServerDescription);
    RootObject->SetStringField(TEXT("ServerVersion"), ServerVersion);
    RootObject->SetStringField(TEXT("AdminContact"), AdminContact);
    RootObject->SetStringField(TEXT("Region"), Region);
    
    // 네트워크 설정
    TSharedPtr<FJsonObject> NetworkObj = MakeShareable(new FJsonObject);
    NetworkObj->SetStringField(TEXT("ServerIP"), NetworkConfig.ServerIP);
    NetworkObj->SetNumberField(TEXT("ServerPort"), NetworkConfig.ServerPort);
    NetworkObj->SetNumberField(TEXT("MaxConnections"), NetworkConfig.MaxConnections);
    NetworkObj->SetNumberField(TEXT("TickRate"), NetworkConfig.TickRate);
    NetworkObj->SetBoolField(TEXT("EnableCompression"), NetworkConfig.bEnableCompression);
    RootObject->SetObjectField(TEXT("NetworkConfig"), NetworkObj);
    
    // 성능 설정
    TSharedPtr<FJsonObject> PerformanceObj = MakeShareable(new FJsonObject);
    PerformanceObj->SetNumberField(TEXT("MaxCPUUsage"), PerformanceConfig.MaxCPUUsage);
    PerformanceObj->SetNumberField(TEXT("MaxMemoryUsage"), PerformanceConfig.MaxMemoryUsage);
    PerformanceObj->SetNumberField(TEXT("MaxConcurrentSessions"), PerformanceConfig.MaxConcurrentSessions);
    PerformanceObj->SetBoolField(TEXT("EnableObjectPooling"), PerformanceConfig.bEnableObjectPooling);
    RootObject->SetObjectField(TEXT("PerformanceConfig"), PerformanceObj);
    
    // 보안 설정
    TSharedPtr<FJsonObject> SecurityObj = MakeShareable(new FJsonObject);
    SecurityObj->SetNumberField(TEXT("AuthMethod"), (int32)SecurityConfig.AuthMethod);
    SecurityObj->SetBoolField(TEXT("EnableAntiCheat"), SecurityConfig.bEnableAntiCheat);
    SecurityObj->SetBoolField(TEXT("EnableRateLimiting"), SecurityConfig.bEnableRateLimiting);
    SecurityObj->SetNumberField(TEXT("MaxRequestsPerMinute"), SecurityConfig.MaxRequestsPerMinute);
    RootObject->SetObjectField(TEXT("SecurityConfig"), SecurityObj);
    
    // 로깅 설정
    TSharedPtr<FJsonObject> LoggingObj = MakeShareable(new FJsonObject);
    LoggingObj->SetNumberField(TEXT("LogLevel"), (int32)LoggingConfig.LogLevel);
    LoggingObj->SetBoolField(TEXT("LogToFile"), LoggingConfig.bLogToFile);
    LoggingObj->SetBoolField(TEXT("LogToConsole"), LoggingConfig.bLogToConsole);
    LoggingObj->SetStringField(TEXT("LogFilePath"), LoggingConfig.LogFilePath);
    RootObject->SetObjectField(TEXT("LoggingConfig"), LoggingObj);
    
    // JSON 문자열 생성
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    return OutputString;
}

bool UHSServerConfig::DeserializeFromJSON(const FString& JSONString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HSServerConfig: JSON 파싱 실패"));
        return false;
    }
    
    // 기본 서버 정보
    JsonObject->TryGetStringField(TEXT("ServerName"), ServerName);
    JsonObject->TryGetStringField(TEXT("ServerDescription"), ServerDescription);
    JsonObject->TryGetStringField(TEXT("ServerVersion"), ServerVersion);
    JsonObject->TryGetStringField(TEXT("AdminContact"), AdminContact);
    JsonObject->TryGetStringField(TEXT("Region"), Region);
    
    // 네트워크 설정
    const TSharedPtr<FJsonObject>* NetworkObj;
    if (JsonObject->TryGetObjectField(TEXT("NetworkConfig"), NetworkObj) && NetworkObj->IsValid())
    {
        (*NetworkObj)->TryGetStringField(TEXT("ServerIP"), NetworkConfig.ServerIP);
        (*NetworkObj)->TryGetNumberField(TEXT("ServerPort"), NetworkConfig.ServerPort);
        (*NetworkObj)->TryGetNumberField(TEXT("MaxConnections"), NetworkConfig.MaxConnections);
        (*NetworkObj)->TryGetNumberField(TEXT("TickRate"), NetworkConfig.TickRate);
        (*NetworkObj)->TryGetBoolField(TEXT("EnableCompression"), NetworkConfig.bEnableCompression);
    }
    
    // 성능 설정
    const TSharedPtr<FJsonObject>* PerformanceObj;
    if (JsonObject->TryGetObjectField(TEXT("PerformanceConfig"), PerformanceObj) && PerformanceObj->IsValid())
    {
        (*PerformanceObj)->TryGetNumberField(TEXT("MaxCPUUsage"), PerformanceConfig.MaxCPUUsage);
        (*PerformanceObj)->TryGetNumberField(TEXT("MaxMemoryUsage"), PerformanceConfig.MaxMemoryUsage);
        (*PerformanceObj)->TryGetNumberField(TEXT("MaxConcurrentSessions"), PerformanceConfig.MaxConcurrentSessions);
        (*PerformanceObj)->TryGetBoolField(TEXT("EnableObjectPooling"), PerformanceConfig.bEnableObjectPooling);
    }
    
    // 보안 설정
    const TSharedPtr<FJsonObject>* SecurityObj;
    if (JsonObject->TryGetObjectField(TEXT("SecurityConfig"), SecurityObj) && SecurityObj->IsValid())
    {
        int32 AuthMethodInt;
        if ((*SecurityObj)->TryGetNumberField(TEXT("AuthMethod"), AuthMethodInt))
        {
            SecurityConfig.AuthMethod = (EHSAuthenticationMethod)AuthMethodInt;
        }
        (*SecurityObj)->TryGetBoolField(TEXT("EnableAntiCheat"), SecurityConfig.bEnableAntiCheat);
        (*SecurityObj)->TryGetBoolField(TEXT("EnableRateLimiting"), SecurityConfig.bEnableRateLimiting);
        (*SecurityObj)->TryGetNumberField(TEXT("MaxRequestsPerMinute"), SecurityConfig.MaxRequestsPerMinute);
    }
    
    // 로깅 설정
    const TSharedPtr<FJsonObject>* LoggingObj;
    if (JsonObject->TryGetObjectField(TEXT("LoggingConfig"), LoggingObj) && LoggingObj->IsValid())
    {
        int32 LogLevelInt;
        if ((*LoggingObj)->TryGetNumberField(TEXT("LogLevel"), LogLevelInt))
        {
            LoggingConfig.LogLevel = (EHSLogLevel)LogLevelInt;
        }
        (*LoggingObj)->TryGetBoolField(TEXT("LogToFile"), LoggingConfig.bLogToFile);
        (*LoggingObj)->TryGetBoolField(TEXT("LogToConsole"), LoggingConfig.bLogToConsole);
        (*LoggingObj)->TryGetStringField(TEXT("LogFilePath"), LoggingConfig.LogFilePath);
    }
    
    return true;
}

void UHSServerConfig::MarkConfigurationChanged()
{
    bConfigurationChanged = true;
    LastModifiedTime = FDateTime::Now();
    ClearValidationCache();
}

void UHSServerConfig::ClearConfigurationChanged()
{
    bConfigurationChanged = false;
}

void UHSServerConfig::InitializeDefaults()
{
    DefaultNetworkConfig = NetworkConfig;
    DefaultPerformanceConfig = PerformanceConfig;
    DefaultSecurityConfig = SecurityConfig;
    DefaultLoggingConfig = LoggingConfig;
    DefaultGameplayConfig = GameplayConfig;
    DefaultMonitoringConfig = MonitoringConfig;
}

void UHSServerConfig::ClearValidationCache() const
{
    bValidationCacheValid = false;
    CachedValidationErrors.Empty();
}

FString UHSServerConfig::ConfigToString(const FHSNetworkConfig& Config) const
{
    return FString::Printf(TEXT("Network: %s:%d (Max: %d)"), 
                          *Config.ServerIP, Config.ServerPort, Config.MaxConnections);
}

FString UHSServerConfig::ConfigToString(const FHSPerformanceConfig& Config) const
{
    return FString::Printf(TEXT("Performance: CPU %.1f%%, Memory %.1fMB"), 
                          Config.MaxCPUUsage, Config.MaxMemoryUsage);
}

FString UHSServerConfig::ConfigToString(const FHSSecurityConfig& Config) const
{
    return FString::Printf(TEXT("Security: Auth %d, AntiCheat %s"), 
                          (int32)Config.AuthMethod, 
                          Config.bEnableAntiCheat ? TEXT("On") : TEXT("Off"));
}

FString UHSServerConfig::ConfigToString(const FHSLoggingConfig& Config) const
{
    return FString::Printf(TEXT("Logging: Level %d, File %s"), 
                          (int32)Config.LogLevel, 
                          Config.bLogToFile ? TEXT("On") : TEXT("Off"));
}

FString UHSServerConfig::ConfigToString(const FHSGameplayConfig& Config) const
{
    return FString::Printf(TEXT("Gameplay: Session %.1fs, Spectators %s"), 
                          Config.DefaultSessionDuration,
                          Config.bAllowSpectators ? TEXT("On") : TEXT("Off"));
}

FString UHSServerConfig::ConfigToString(const FHSMonitoringConfig& Config) const
{
    return FString::Printf(TEXT("Monitoring: Interval %.1fs, Health Port %d"), 
                          Config.MonitoringInterval, Config.HealthCheckPort);
}
