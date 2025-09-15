// 사냥의 영혼(HuntingSpirit) 게임의 세션 관리자
// 게임 세션의 생성, 참여, 관리를 담당하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "TimerManager.h"
#include "HSSessionManager.generated.h"

// 전방 선언
class UHSTeamManager;

// 세션 상태 정의
UENUM(BlueprintType)
enum class EHSSessionState : uint8
{
    SS_None             UMETA(DisplayName = "None"),               // 세션 없음
    SS_Creating         UMETA(DisplayName = "Creating"),          // 생성 중
    SS_Searching        UMETA(DisplayName = "Searching"),         // 검색 중
    SS_Joining          UMETA(DisplayName = "Joining"),           // 참여 중
    SS_InSession        UMETA(DisplayName = "In Session"),        // 세션 내
    SS_Leaving          UMETA(DisplayName = "Leaving"),           // 탈퇴 중
    SS_Destroying       UMETA(DisplayName = "Destroying"),        // 파괴 중
    SS_Error            UMETA(DisplayName = "Error")              // 오류
};

// 세션 타입 정의
UENUM(BlueprintType)
enum class EHSSessionType : uint8
{
    ST_LAN              UMETA(DisplayName = "LAN"),               // LAN 게임
    ST_Online           UMETA(DisplayName = "Online"),            // 온라인 게임
    ST_Private          UMETA(DisplayName = "Private"),           // 비공개 게임
    ST_Public           UMETA(DisplayName = "Public"),            // 공개 게임
    ST_Dedicated        UMETA(DisplayName = "Dedicated")          // 데디케이트 서버
};

// 세션 정보 구조체
USTRUCT(BlueprintType)
struct FHSSessionInfo
{
    GENERATED_BODY()

    // 세션 이름
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    FString SessionName;

    // 호스트 이름
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    FString HostName;

    // 맵 이름
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    FString MapName;

    // 게임 모드
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    FString GameMode;

    // 세션 타입
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    EHSSessionType SessionType;

    // 현재 플레이어 수
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    int32 CurrentPlayers;

    // 최대 플레이어 수
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    int32 MaxPlayers;

    // 핑 (밀리초)
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    int32 Ping;

    // 세션 설정 키-값 쌍
    UPROPERTY(BlueprintReadOnly, Category = "Session Info")
    TMap<FString, FString> SessionSettings;

    // 검색 결과 (엔진 내부 사용)
    TSharedPtr<const FOnlineSessionSearchResult> SearchResult;

    // 기본값 설정
    FHSSessionInfo()
    {
        SessionName = TEXT("Unknown Session");
        HostName = TEXT("Unknown Host");
        MapName = TEXT("DefaultMap");
        GameMode = TEXT("DefaultGameMode");
        SessionType = EHSSessionType::ST_Public;
        CurrentPlayers = 0;
        MaxPlayers = 4;
        Ping = 999;
    }
};

// 세션 생성 설정
USTRUCT(BlueprintType)
struct FHSSessionCreateSettings
{
    GENERATED_BODY()

    // 세션 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    FString SessionName;

    // 최대 플레이어 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    int32 MaxPlayers;

    // 세션 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    EHSSessionType SessionType;

    // 공개 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    bool bIsPublic;

    // LAN 전용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    bool bIsLANMatch;

    // 초대 전용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    bool bIsInviteOnly;

    // 세션 비밀번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    FString Password;

    // 맵 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    FString MapName;

    // 게임 모드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    FString GameMode;

    // 커스텀 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Create Settings")
    TMap<FString, FString> CustomSettings;

    // 기본값 설정
    FHSSessionCreateSettings()
    {
        SessionName = TEXT("HuntingSpirit Game");
        MaxPlayers = 4;
        SessionType = EHSSessionType::ST_Public;
        bIsPublic = true;
        bIsLANMatch = false;
        bIsInviteOnly = false;
        Password = TEXT("");
        MapName = TEXT("/Game/Maps/DefaultMap");
        GameMode = TEXT("HSGameMode");
    }
};

// 세션 검색 필터
USTRUCT(BlueprintType)
struct FHSSessionSearchFilter
{
    GENERATED_BODY()

    // 최대 검색 결과 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    int32 MaxSearchResults;

    // LAN 검색 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    bool bSearchLAN;

    // 공개 세션만 검색
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    bool bPublicOnly;

    // 비어있지 않은 세션만 검색
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    bool bNonEmptyOnly;

    // 가득 찬 세션 제외
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    bool bExcludeFullSessions;

    // 최대 핑 제한 (밀리초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    int32 MaxPing;

    // 검색할 게임 모드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    FString GameMode;

    // 검색할 맵
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    FString MapName;

    // 커스텀 필터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search Filter")
    TMap<FString, FString> CustomFilters;

    // 기본값 설정
    FHSSessionSearchFilter()
    {
        MaxSearchResults = 50;
        bSearchLAN = false;
        bPublicOnly = true;
        bNonEmptyOnly = false;
        bExcludeFullSessions = true;
        MaxPing = 200;
        GameMode = TEXT("HSGameMode");
        MapName = TEXT("");
    }
};

// 세션 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionStateChanged, EHSSessionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSessionCreated, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSessionJoined, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSessionSearchCompleted, bool, bSuccess, const TArray<FHSSessionInfo>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSessionDestroyed, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerJoinedSession, const FString&, PlayerName, int32, PlayerCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerLeftSession, const FString&, PlayerName, int32, PlayerCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSessionError, const FString&, ErrorMessage, int32, ErrorCode);

/**
 * 사냥의 영혼 세션 관리자
 * 
 * 주요 기능:
 * - 게임 세션 생성 및 관리
 * - 세션 검색 및 참여
 * - 플레이어 연결 관리
 * - LAN 및 온라인 세션 지원
 * - 데디케이트 서버 연결
 * - 세션 상태 모니터링
 * - 네트워크 오류 처리
 * - 자동 재연결 기능
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSSessionManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 생성자
    UHSSessionManager();

    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 세션 생성 및 관리 ===

    /**
     * 새로운 세션을 생성합니다
     * @param CreateSettings 세션 생성 설정
     * @return 세션 생성 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool CreateSession(const FHSSessionCreateSettings& CreateSettings);

    /**
     * 세션을 파괴합니다
     * @return 세션 파괴 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool DestroySession();

    /**
     * 세션에서 나갑니다
     * @return 세션 탈퇴 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool LeaveSession();

    /**
     * 세션을 시작합니다 (호스트만 가능)
     * @return 세션 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool StartSession();

    /**
     * 세션을 종료합니다 (호스트만 가능)
     * @return 세션 종료 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Management")
    bool EndSession();

    // === 세션 검색 및 참여 ===

    /**
     * 세션을 검색합니다
     * @param SearchFilter 검색 필터
     * @return 검색 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Search")
    bool SearchSessions(const FHSSessionSearchFilter& SearchFilter);

    /**
     * 세션에 참여합니다
     * @param SessionInfo 참여할 세션 정보
     * @return 참여 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Search")
    bool JoinSession(const FHSSessionInfo& SessionInfo);

    /**
     * 세션에 참여합니다 (인덱스 기반)
     * @param SessionIndex 검색 결과에서의 세션 인덱스
     * @return 참여 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Search")
    bool JoinSessionByIndex(int32 SessionIndex);

    /**
     * 빠른 매칭을 수행합니다
     * @param SearchFilter 검색 필터 (기본값 사용 시 빈 구조체)
     * @return 빠른 매칭 시작 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Search")
    bool QuickMatch(const FHSSessionSearchFilter& SearchFilter);

    // === 세션 정보 조회 ===

    /**
     * 현재 세션 상태를 반환합니다
     * @return 현재 세션 상태
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    EHSSessionState GetCurrentSessionState() const { return CurrentSessionState; }

    /**
     * 현재 세션 정보를 반환합니다
     * @return 현재 세션 정보
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    FHSSessionInfo GetCurrentSessionInfo() const { return CurrentSessionInfo; }

    /**
     * 마지막 검색 결과를 반환합니다
     * @return 검색된 세션 목록
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    TArray<FHSSessionInfo> GetLastSearchResults() const { return LastSearchResults; }

    /**
     * 세션에 참여 중인지 확인합니다
     * @return 세션 참여 여부
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    bool IsInSession() const { return CurrentSessionState == EHSSessionState::SS_InSession; }

    /**
     * 세션 호스트인지 확인합니다
     * @return 호스트 여부
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    bool IsSessionHost() const { return bIsSessionHost; }

    /**
     * 현재 세션의 플레이어 수를 반환합니다
     * @return 플레이어 수
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    int32 GetSessionPlayerCount() const;

    /**
     * 세션의 최대 플레이어 수를 반환합니다
     * @return 최대 플레이어 수
     */
    UFUNCTION(BlueprintPure, Category = "Session Info")
    int32 GetSessionMaxPlayers() const;

    // === 세션 설정 관리 ===

    /**
     * 세션 설정을 업데이트합니다 (호스트만 가능)
     * @param SettingKey 설정 키
     * @param SettingValue 설정 값
     * @return 업데이트 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Settings")
    bool UpdateSessionSetting(const FString& SettingKey, const FString& SettingValue);

    /**
     * 세션 설정을 가져옵니다
     * @param SettingKey 설정 키
     * @return 설정 값 (없으면 빈 문자열)
     */
    UFUNCTION(BlueprintPure, Category = "Session Settings")
    FString GetSessionSetting(const FString& SettingKey) const;

    /**
     * 세션 최대 플레이어 수를 변경합니다 (호스트만 가능)
     * @param NewMaxPlayers 새로운 최대 플레이어 수
     * @return 변경 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Session Settings")
    bool ChangeMaxPlayers(int32 NewMaxPlayers);

    // === 플레이어 관리 ===

    /**
     * 플레이어를 세션에서 추방합니다 (호스트만 가능)
     * @param PlayerName 추방할 플레이어 이름
     * @return 추방 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Player Management")
    bool KickPlayer(const FString& PlayerName);

    /**
     * 플레이어를 세션에서 금지합니다 (호스트만 가능)
     * @param PlayerName 금지할 플레이어 이름
     * @return 금지 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Player Management")
    bool BanPlayer(const FString& PlayerName);

    /**
     * 현재 세션의 모든 플레이어 목록을 반환합니다
     * @return 플레이어 이름 목록
     */
    UFUNCTION(BlueprintPure, Category = "Player Management")
    TArray<FString> GetSessionPlayerNames() const;

    // === 네트워크 진단 ===

    /**
     * 세션 연결 품질을 반환합니다 (0-4 스케일)
     * @return 연결 품질
     */
    UFUNCTION(BlueprintPure, Category = "Network Diagnostics")
    int32 GetSessionConnectionQuality() const;

    /**
     * 세션 핑을 반환합니다 (밀리초)
     * @return 평균 핑
     */
    UFUNCTION(BlueprintPure, Category = "Network Diagnostics")
    int32 GetSessionPing() const;

    /**
     * 네트워크 통계를 문자열로 반환합니다
     * @return 네트워크 통계 정보
     */
    UFUNCTION(BlueprintCallable, Category = "Network Diagnostics")
    FString GetNetworkStatsString() const;

    // === 유틸리티 함수들 ===

    /**
     * 자동 재연결을 활성화/비활성화합니다
     * @param bEnable 자동 재연결 활성화 여부
     * @param MaxRetries 최대 재시도 횟수
     */
    UFUNCTION(BlueprintCallable, Category = "Session Utilities")
    void SetAutoReconnect(bool bEnable, int32 MaxRetries = 3);

    /**
     * 세션 매니저 상태를 문자열로 반환합니다 (디버그용)
     * @return 상태 정보 문자열
     */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    FString GetSessionManagerStatusString() const;

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnSessionStateChanged OnSessionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnSessionJoined OnSessionJoined;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnSessionSearchCompleted OnSessionSearchCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnSessionDestroyed OnSessionDestroyed;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnPlayerJoinedSession OnPlayerJoinedSession;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnPlayerLeftSession OnPlayerLeftSession;

    UPROPERTY(BlueprintAssignable, Category = "Session Events")
    FOnSessionError OnSessionError;

protected:
    // === 현재 세션 상태 ===

    // 현재 세션 상태
    UPROPERTY(BlueprintReadOnly, Category = "Session State")
    EHSSessionState CurrentSessionState;

    // 현재 세션 정보
    UPROPERTY(BlueprintReadOnly, Category = "Session State")
    FHSSessionInfo CurrentSessionInfo;

    // 세션 호스트 여부
    UPROPERTY(BlueprintReadOnly, Category = "Session State")
    bool bIsSessionHost;

    // 마지막 검색 결과
    UPROPERTY(BlueprintReadOnly, Category = "Session State")
    TArray<FHSSessionInfo> LastSearchResults;

    // === 설정 변수들 ===

    // 기본 세션 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Configuration")
    FHSSessionCreateSettings DefaultCreateSettings;

    // 기본 검색 필터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Configuration")
    FHSSessionSearchFilter DefaultSearchFilter;

    // 자동 재연결 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Configuration")
    bool bAutoReconnectEnabled;

    // 최대 재연결 시도 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Configuration")
    int32 MaxReconnectRetries;

    // 세션 하트비트 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Configuration")
    float SessionHeartbeatInterval;

    // 연결 타임아웃 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Configuration")
    float ConnectionTimeout;

    // === 런타임 변수들 ===

    // 온라인 서브시스템 참조
    IOnlineSubsystem* OnlineSubsystem;

    // 세션 인터페이스 참조
    IOnlineSessionPtr SessionInterface;

    // 현재 세션 검색 설정
    TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;

    // 재연결 시도 횟수
    int32 CurrentReconnectAttempts;

    // 금지된 플레이어 목록
    UPROPERTY()
    TArray<FString> BannedPlayers;

    // === 타이머 핸들들 ===

    // 세션 하트비트 타이머
    FTimerHandle SessionHeartbeatTimer;

    // 재연결 타이머
    FTimerHandle ReconnectTimer;

    // 연결 타임아웃 타이머
    FTimerHandle ConnectionTimeoutTimer;

    // 세션 정리 타이머
    FTimerHandle SessionCleanupTimer;

private:
    // === 내부 함수들 ===

    // 온라인 서브시스템 초기화
    void InitializeOnlineSubsystem();

    // 세션 상태 변경 처리
    void ChangeSessionState(EHSSessionState NewState);

    // 세션 설정을 엔진 형식으로 변환
    TSharedPtr<FOnlineSessionSettings> ConvertToOnlineSessionSettings(const FHSSessionCreateSettings& CreateSettings) const;

    // 검색 필터를 엔진 형식으로 변환
    void ApplySearchFilter(const FHSSessionSearchFilter& Filter, TSharedPtr<FOnlineSessionSearch> SessionSearch) const;

    // 검색 결과를 내부 형식으로 변환
    FHSSessionInfo ConvertFromSearchResult(const FOnlineSessionSearchResult& SearchResult) const;

    // 세션 하트비트 처리
    UFUNCTION()
    void ProcessSessionHeartbeat();

    // 자동 재연결 시도
    UFUNCTION()
    void AttemptReconnect();

    // 연결 타임아웃 처리
    UFUNCTION()
    void HandleConnectionTimeout();

    // 세션 정리
    UFUNCTION()
    void CleanupSession();

    // === 온라인 서브시스템 콜백 함수들 ===

    // 세션 생성 완료 콜백
    void OnCreateSessionComplete(FName SessionName, bool bSuccess);

    // 세션 시작 완료 콜백
    void OnStartSessionComplete(FName SessionName, bool bSuccess);

    // 세션 검색 완료 콜백
    void OnFindSessionsComplete(bool bSuccess);

    // 세션 참여 완료 콜백
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    // 세션 파괴 완료 콜백
    void OnDestroySessionComplete(FName SessionName, bool bSuccess);

    // 세션 종료 완료 콜백
    void OnEndSessionComplete(FName SessionName, bool bSuccess);

    // 플레이어 세션 참여 콜백
    void OnSessionUserInviteAccepted(bool bSuccess, int32 ControllerId, TSharedPtr<const FUniqueNetId> UserId, const FOnlineSessionSearchResult& InviteResult);

    // === 델리게이트 핸들들 ===

    FDelegateHandle OnCreateSessionCompleteDelegateHandle;
    FDelegateHandle OnStartSessionCompleteDelegateHandle;
    FDelegateHandle OnFindSessionsCompleteDelegateHandle;
    FDelegateHandle OnJoinSessionCompleteDelegateHandle;
    FDelegateHandle OnDestroySessionCompleteDelegateHandle;
    FDelegateHandle OnEndSessionCompleteDelegateHandle;
    FDelegateHandle OnSessionUserInviteAcceptedDelegateHandle;
    
    // 델리게이트 인스턴스들
    FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
    FOnStartSessionCompleteDelegate OnStartSessionCompleteDelegate;
    FOnFindSessionsCompleteDelegate OnFindSessionsCompleteDelegate;
    FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;
    FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
    FOnEndSessionCompleteDelegate OnEndSessionCompleteDelegate;

    // === 디버그 및 로깅 함수들 ===

    // 세션 상태 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogSessionState() const;

    // 검색 결과 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogSearchResults() const;

    // === 에러 처리 ===

    // 세션 에러 처리
    void HandleSessionError(const FString& ErrorMessage, int32 ErrorCode = -1);

    // 네트워크 에러 복구
    void RecoverFromNetworkError();

    // 세션 무결성 확인
    bool ValidateSessionIntegrity() const;

    // 초기화 완료 플래그
    bool bIsInitialized;

    // 스레드 안전성을 위한 뮤텍스
    mutable FCriticalSection SessionStateMutex;
};
