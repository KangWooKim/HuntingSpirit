#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "HSMatchmakingSystem.generated.h"

UENUM(BlueprintType)
enum class EHSMatchmakingStatus : uint8
{
    NotSearching,
    Searching,
    MatchFound,
    JoiningMatch,
    InMatch,
    Error
};

UENUM(BlueprintType)
enum class EHSMatchType : uint8
{
    QuickMatch,
    RankedMatch,
    CustomMatch,
    PrivateMatch
};

UENUM(BlueprintType)
enum class EHSRegion : uint8
{
    Auto,
    NorthAmerica,
    Europe,
    Asia,
    Oceania,
    SouthAmerica
};

USTRUCT(BlueprintType)
struct FHSMatchmakingRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
    EHSMatchType MatchType = EHSMatchType::QuickMatch;

    UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
    EHSRegion PreferredRegion = EHSRegion::Auto;

    UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
    int32 MaxPing = 100;

    UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
    float SkillRating = 1000.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
    bool bAllowCrossPlatform = true;

    FHSMatchmakingRequest()
    {
        MatchType = EHSMatchType::QuickMatch;
        PreferredRegion = EHSRegion::Auto;
        MaxPing = 100;
        SkillRating = 1000.0f;
        bAllowCrossPlatform = true;
    }
};

USTRUCT(BlueprintType)
struct FHSMatchInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    FString MatchID;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    FString ServerAddress;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    int32 CurrentPlayers = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    int32 MaxPlayers = 4;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    int32 PingMs = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    EHSRegion Region = EHSRegion::Auto;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    float AverageSkillRating = 1000.0f;

    FHSMatchInfo()
    {
        CurrentPlayers = 0;
        MaxPlayers = 4;
        PingMs = 0;
        Region = EHSRegion::Auto;
        AverageSkillRating = 1000.0f;
    }
};

USTRUCT(BlueprintType)
struct FHSPlayerMatchmakingInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Player")
    FString PlayerID;

    UPROPERTY(BlueprintReadWrite, Category = "Player")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "Player")
    float SkillRating = 1000.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Player")
    int32 Level = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Player")
    EHSRegion Region = EHSRegion::Auto;

    UPROPERTY(BlueprintReadWrite, Category = "Player")
    float SearchStartTime = 0.0f;

    FHSPlayerMatchmakingInfo()
    {
        SkillRating = 1000.0f;
        Level = 1;
        Region = EHSRegion::Auto;
        SearchStartTime = 0.0f;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchmakingStatusChanged, EHSMatchmakingStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchFound, const FHSMatchInfo&, MatchInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchmakingError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEstimatedWaitTimeUpdated, float, WaitTimeSeconds);

/**
 * 매치메이킹 시스템 - 스킬 기반 매칭 및 지역 최적화 지원
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSMatchmakingSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSMatchmakingSystem();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 매치메이킹 핵심 기능
    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    bool StartMatchmaking(const FHSMatchmakingRequest& Request);

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void CancelMatchmaking();

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    bool AcceptMatch(const FString& MatchID);

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void DeclineMatch(const FString& MatchID);

    // 상태 및 정보 조회
    UFUNCTION(BlueprintPure, Category = "Matchmaking")
    EHSMatchmakingStatus GetCurrentStatus() const { return CurrentStatus; }

    UFUNCTION(BlueprintPure, Category = "Matchmaking")
    float GetEstimatedWaitTime() const;

    UFUNCTION(BlueprintPure, Category = "Matchmaking")
    FString GetCurrentMatchID() const { return CurrentMatchID; }

    UFUNCTION(BlueprintPure, Category = "Matchmaking")
    bool IsSearching() const { return CurrentStatus == EHSMatchmakingStatus::Searching; }

    // 설정 관리
    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void SetPlayerSkillRating(float NewRating);

    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void SetPreferredRegion(EHSRegion Region);

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Matchmaking")
    FOnMatchmakingStatusChanged OnMatchmakingStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Matchmaking")
    FOnMatchFound OnMatchFound;

    UPROPERTY(BlueprintAssignable, Category = "Matchmaking")
    FOnMatchmakingError OnMatchmakingError;

    UPROPERTY(BlueprintAssignable, Category = "Matchmaking")
    FOnEstimatedWaitTimeUpdated OnEstimatedWaitTimeUpdated;

protected:
    // 내부 상태
    UPROPERTY()
    EHSMatchmakingStatus CurrentStatus;

    UPROPERTY()
    FHSMatchmakingRequest CurrentRequest;

    UPROPERTY()
    FString CurrentMatchID;

    UPROPERTY()
    FHSPlayerMatchmakingInfo PlayerInfo;

    // 매치메이킹 로직
    void UpdateMatchmakingStatus(EHSMatchmakingStatus NewStatus);
    void ProcessMatchmakingQueue();
    bool ValidateMatchmakingRequest(const FHSMatchmakingRequest& Request);
    
    // 스킬 기반 매칭
    float CalculateSkillDifference(float PlayerRating, float TargetRating) const;
    bool IsSkillMatchSuitable(const FHSPlayerMatchmakingInfo& Player1, const FHSPlayerMatchmakingInfo& Player2) const;
    
    // 지역 및 핑 최적화
    EHSRegion DetectOptimalRegion() const;
    int32 EstimatePingToRegion(EHSRegion Region) const;
    
    // 대기열 관리
    void UpdateEstimatedWaitTime();
    void ExpandSearchCriteria();
    
    // 온라인 서브시스템 연동
    void HandleSessionSearchComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(FName SessionName, bool bWasSuccessful);
    
    // 타이머 및 캐싱
    FTimerHandle MatchmakingTimerHandle;
    FTimerHandle WaitTimeUpdateTimerHandle;
    
    // 성능 캐싱
    mutable float CachedWaitTime;
    mutable float WaitTimeCacheTimestamp;
    
    // 스레드 세이프 처리
    mutable FCriticalSection MatchmakingMutex;
    
    // 현업 최적화: 메모리 풀링
    TArray<FHSPlayerMatchmakingInfo> PlayerPool;
    TArray<FHSMatchInfo> MatchPool;
    
    // 매치메이킹 알고리즘 설정
    static constexpr float SkillToleranceBase = 100.0f;
    static constexpr float SkillToleranceGrowthRate = 20.0f;
    static constexpr float MaxWaitTimeSeconds = 300.0f; // 5분
    static constexpr int32 MaxPingThreshold = 150;
    
    // 캐시 무효화 시간 (초)
    static constexpr float WaitTimeCacheDuration = 5.0f;

private:
    // 온라인 서브시스템 인터페이스
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    
    struct FRegionPingStats
    {
        int32 SampleCount = 0;
        double TotalPing = 0.0;
        int32 LowestPing = MAX_int32;

        void AddSample(int32 Ping)
        {
            ++SampleCount;
            TotalPing += Ping;
            LowestPing = FMath::Min(LowestPing, Ping);
        }

        int32 GetAverage() const
        {
            return SampleCount > 0 ? static_cast<int32>(TotalPing / SampleCount) : MAX_int32;
        }
    };

    mutable TMap<EHSRegion, FRegionPingStats> RegionPingStats;
    FOnlineSessionSearchResult PendingSessionResult;
    bool bHasPendingSessionResult = false;
    
    // 매치 수락 타임아웃
    FTimerHandle MatchAcceptanceTimeoutHandle;
    
    // 지능형 매칭 알고리즘 상태
    float SearchStartTime;
    float LastSearchRequestTime;
    float CurrentSkillTolerance;
    int32 CurrentMaxPing;
    
    // 내부 유틸리티 함수
    static FString GetRegionTag(EHSRegion Region);
    static EHSRegion ParseRegionTag(const FString& RegionString);
    void RecordRegionPingSample(EHSRegion Region, int32 Ping);
    FHSMatchInfo BuildMatchInfoFromResult(const FOnlineSessionSearchResult& SearchResult, EHSRegion Region) const;
    void ResetMatchmakingState();
    void CleanupMatchmakingResources();
    FString GenerateMatchID() const;
    
    // 에러 처리
    void HandleMatchmakingError(const FString& ErrorMessage);
    
    // 매치 수락/거절 처리
    void OnMatchAcceptanceTimeout();
    static constexpr float MatchAcceptanceTimeoutSeconds = 30.0f;
};
