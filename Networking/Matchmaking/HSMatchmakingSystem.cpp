#include "HSMatchmakingSystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UObject/UnrealType.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

UHSMatchmakingSystem::UHSMatchmakingSystem()
{
    CurrentStatus = EHSMatchmakingStatus::NotSearching;
    CachedWaitTime = 0.0f;
    WaitTimeCacheTimestamp = 0.0f;
    SearchStartTime = 0.0f;
    CurrentSkillTolerance = SkillToleranceBase;
    CurrentMaxPing = 100;
    SessionSearch = nullptr;
    bHasPendingSessionResult = false;
    LastSearchRequestTime = 0.0f;
}

void UHSMatchmakingSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 매치메이킹 시스템 초기화 중..."));
    
    // 온라인 서브시스템 초기화
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (OnlineSubsystem)
    {
        SessionInterface = OnlineSubsystem->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 온라인 세션 인터페이스 초기화 완료"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("HSMatchmakingSystem: 온라인 세션 인터페이스 초기화 실패"));
        }
    }
    
    // 오브젝트 풀 초기화 (현업 최적화)
    PlayerPool.Reserve(100);
    MatchPool.Reserve(50);
    
    // 플레이어 정보 초기화
    PlayerInfo.PlayerID = FGuid::NewGuid().ToString();
    PlayerInfo.SkillRating = 1000.0f;
    PlayerInfo.Level = 1;
    PlayerInfo.Region = EHSRegion::Auto;
    
    UpdateMatchmakingStatus(EHSMatchmakingStatus::NotSearching);
}

void UHSMatchmakingSystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 매치메이킹 시스템 종료 중..."));
    
    CancelMatchmaking();
    CleanupMatchmakingResources();
    
    Super::Deinitialize();
}

bool UHSMatchmakingSystem::StartMatchmaking(const FHSMatchmakingRequest& Request)
{
    EHSRegion ResolvedRegion = Request.PreferredRegion;
    if (ResolvedRegion == EHSRegion::Auto)
    {
        ResolvedRegion = DetectOptimalRegion();
    }

    FScopeLock Lock(&MatchmakingMutex);
    
    if (CurrentStatus == EHSMatchmakingStatus::Searching)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMatchmakingSystem: 이미 매치메이킹 진행 중"));
        return false;
    }
    
    if (!ValidateMatchmakingRequest(Request))
    {
        HandleMatchmakingError(TEXT("매치메이킹 요청 검증 실패"));
        return false;
    }
    
    CurrentRequest = Request;
    SearchStartTime = GetWorld()->GetTimeSeconds();
    LastSearchRequestTime = SearchStartTime;
    CurrentSkillTolerance = SkillToleranceBase;
    CurrentMaxPing = Request.MaxPing;
    PendingSessionResult = FOnlineSessionSearchResult();
    bHasPendingSessionResult = false;
    
    CurrentRequest.PreferredRegion = ResolvedRegion;
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 매치메이킹 시작 - 타입: %d, 지역: %d"), 
           (int32)Request.MatchType, (int32)CurrentRequest.PreferredRegion);
    
    UpdateMatchmakingStatus(EHSMatchmakingStatus::Searching);
    
    // 매치메이킹 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(
        MatchmakingTimerHandle,
        this,
        &UHSMatchmakingSystem::ProcessMatchmakingQueue,
        1.0f,
        true
    );
    
    // 대기시간 업데이트 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(
        WaitTimeUpdateTimerHandle,
        this,
        &UHSMatchmakingSystem::UpdateEstimatedWaitTime,
        5.0f,
        true
    );
    
    // 세션 검색 시작
    if (SessionInterface.IsValid())
    {
        SessionSearch = MakeShareable(new FOnlineSessionSearch());
        SessionSearch->bIsLanQuery = false;
        SessionSearch->MaxSearchResults = 50;
        SessionSearch->TimeoutInSeconds = 30.0f;
        
        // 검색 설정
        SessionSearch->QuerySettings.Set(FName("SEARCH_PRESENCE"), true, EOnlineComparisonOp::Equals);
        SessionSearch->QuerySettings.Set(FName("SEARCH_LOBBIES"), true, EOnlineComparisonOp::Equals);
        
        // 지역별 필터
        FString RegionString = GetRegionTag(CurrentRequest.PreferredRegion);
        SessionSearch->QuerySettings.Set(FName("Region"), RegionString, EOnlineComparisonOp::Equals);
        
        // 세션 검색 델리게이트 바인딩
        SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
            this, &UHSMatchmakingSystem::HandleSessionSearchComplete);
        SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
            this, &UHSMatchmakingSystem::HandleJoinSessionComplete);
        
        if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
        {
            HandleMatchmakingError(TEXT("세션 검색 시작 실패"));
            return false;
        }
    }
    
    return true;
}

void UHSMatchmakingSystem::CancelMatchmaking()
{
    FScopeLock Lock(&MatchmakingMutex);
    
    if (CurrentStatus != EHSMatchmakingStatus::Searching)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 매치메이킹 취소"));
    
    // 타이머 정리
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(MatchmakingTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(WaitTimeUpdateTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(MatchAcceptanceTimeoutHandle);
    }
    
    // 세션 검색 취소
    if (SessionInterface.IsValid() && SessionSearch.IsValid())
    {
        SessionInterface->CancelFindSessions();
    }
    
    ResetMatchmakingState();
    UpdateMatchmakingStatus(EHSMatchmakingStatus::NotSearching);
}

bool UHSMatchmakingSystem::AcceptMatch(const FString& MatchID)
{
    FScopeLock Lock(&MatchmakingMutex);
    
    if (CurrentStatus != EHSMatchmakingStatus::MatchFound || CurrentMatchID != MatchID)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMatchmakingSystem: 잘못된 매치 수락 요청"));
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 매치 수락 - ID: %s"), *MatchID);
    
    UpdateMatchmakingStatus(EHSMatchmakingStatus::JoiningMatch);
    
    // 매치 수락 타임아웃 타이머 정지
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(MatchAcceptanceTimeoutHandle);
    }
    
    if (!SessionInterface.IsValid())
    {
        HandleMatchmakingError(TEXT("세션 인터페이스가 유효하지 않습니다"));
        return false;
    }

    if (!bHasPendingSessionResult)
    {
        HandleMatchmakingError(TEXT("참가할 세션 정보가 없습니다"));
        return false;
    }

    const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        HandleMatchmakingError(TEXT("로컬 플레이어 정보를 찾을 수 없습니다"));
        return false;
    }

    const FOnlineSessionSearchResult SessionToJoin = PendingSessionResult;

    if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionToJoin))
    {
        HandleMatchmakingError(TEXT("세션 참가 요청이 실패했습니다"));
        return false;
    }

    bHasPendingSessionResult = false;

    // JoinSession 호출이 성공하면 OnJoinSessionComplete 델리게이트에서 최종 처리
    return true;
}

void UHSMatchmakingSystem::DeclineMatch(const FString& MatchID)
{
    FScopeLock Lock(&MatchmakingMutex);
    
    if (CurrentStatus != EHSMatchmakingStatus::MatchFound || CurrentMatchID != MatchID)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 매치 거절 - ID: %s"), *MatchID);
    
    bHasPendingSessionResult = false;
    PendingSessionResult = FOnlineSessionSearchResult();

    // 매칭 재시작
    ResetMatchmakingState();
    
    // 잠시 대기 후 재검색
    if (GetWorld())
    {
        FTimerHandle RetryTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            RetryTimerHandle,
            [this]() { StartMatchmaking(CurrentRequest); },
            2.0f,
            false
        );
    }
}

float UHSMatchmakingSystem::GetEstimatedWaitTime() const
{
    FScopeLock Lock(&MatchmakingMutex);
    
    // 캐시된 값 사용 (성능 최적화)
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - WaitTimeCacheTimestamp < WaitTimeCacheDuration)
    {
        return CachedWaitTime;
    }
    
    // 대기시간 계산
    float EstimatedTime = 60.0f; // 기본 1분
    
    if (CurrentStatus == EHSMatchmakingStatus::Searching)
    {
        float ElapsedTime = CurrentTime - SearchStartTime;
        
        // 매치 타입별 가중치
        float TypeMultiplier = 1.0f;
        switch (CurrentRequest.MatchType)
        {
            case EHSMatchType::QuickMatch: TypeMultiplier = 0.5f; break;
            case EHSMatchType::RankedMatch: TypeMultiplier = 1.5f; break;
            case EHSMatchType::CustomMatch: TypeMultiplier = 2.0f; break;
            case EHSMatchType::PrivateMatch: TypeMultiplier = 0.1f; break;
        }
        
        // 지역별 가중치
        float RegionMultiplier = 1.0f;
        switch (CurrentRequest.PreferredRegion)
        {
            case EHSRegion::Auto: RegionMultiplier = 0.8f; break;
            case EHSRegion::NorthAmerica: RegionMultiplier = 0.9f; break;
            case EHSRegion::Europe: RegionMultiplier = 1.0f; break;
            case EHSRegion::Asia: RegionMultiplier = 1.2f; break;
            case EHSRegion::Oceania: RegionMultiplier = 1.8f; break;
            case EHSRegion::SouthAmerica: RegionMultiplier = 1.5f; break;
        }
        
        // 스킬 레이팅 기반 조정
        float SkillMultiplier = 1.0f;
        if (PlayerInfo.SkillRating > 1500.0f) // 고랭크
        {
            SkillMultiplier = 1.5f;
        }
        else if (PlayerInfo.SkillRating < 800.0f) // 저랭크
        {
            SkillMultiplier = 1.2f;
        }
        
        EstimatedTime = (90.0f - ElapsedTime) * TypeMultiplier * RegionMultiplier * SkillMultiplier;
        EstimatedTime = FMath::Clamp(EstimatedTime, 10.0f, MaxWaitTimeSeconds);
    }
    
    // 캐시 업데이트
    CachedWaitTime = EstimatedTime;
    WaitTimeCacheTimestamp = CurrentTime;
    
    return EstimatedTime;
}

void UHSMatchmakingSystem::SetPlayerSkillRating(float NewRating)
{
    PlayerInfo.SkillRating = FMath::Clamp(NewRating, 100.0f, 3000.0f);
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 플레이어 스킬 레이팅 설정: %.1f"), NewRating);
}

void UHSMatchmakingSystem::SetPreferredRegion(EHSRegion Region)
{
    PlayerInfo.Region = Region;
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 선호 지역 설정: %d"), (int32)Region);
}

void UHSMatchmakingSystem::UpdateMatchmakingStatus(EHSMatchmakingStatus NewStatus)
{
    if (CurrentStatus == NewStatus)
    {
        return;
    }
    
    EHSMatchmakingStatus OldStatus = CurrentStatus;
    CurrentStatus = NewStatus;
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 상태 변경 %d -> %d"), (int32)OldStatus, (int32)NewStatus);
    
    OnMatchmakingStatusChanged.Broadcast(NewStatus);
}

void UHSMatchmakingSystem::ProcessMatchmakingQueue()
{
    if (CurrentStatus != EHSMatchmakingStatus::Searching)
    {
        return;
    }
    
    float ElapsedTime = GetWorld()->GetTimeSeconds() - SearchStartTime;
    
    // 일정 시간마다 검색 조건 완화
    if (ElapsedTime > 30.0f)
    {
        ExpandSearchCriteria();
    }
    
    // 최대 대기시간 초과 시 에러 처리
    if (ElapsedTime > MaxWaitTimeSeconds)
    {
        HandleMatchmakingError(TEXT("매치메이킹 타임아웃"));
        return;
    }
    
    // 세션 검색이 완료되었으나 조건에 맞는 결과가 없으면 재검색 시도
    if (SessionInterface.IsValid() && SessionSearch.IsValid() && GetWorld())
    {
        const float WorldTime = GetWorld()->GetTimeSeconds();
        if (SessionSearch->SearchState == EOnlineAsyncTaskState::Done && !bHasPendingSessionResult)
        {
            if (SessionSearch->SearchResults.Num() == 0 && (WorldTime - LastSearchRequestTime) >= 10.0f)
            {
                const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
                const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
                if (LocalPlayer && LocalPlayer->GetPreferredUniqueNetId().IsValid())
                {
                    UE_LOG(LogTemp, Verbose, TEXT("HSMatchmakingSystem: 세션 재검색 시도"));
                    SessionSearch->SearchResults.Empty();
                    SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
                    LastSearchRequestTime = WorldTime;
                }
            }
        }
    }
}

bool UHSMatchmakingSystem::ValidateMatchmakingRequest(const FHSMatchmakingRequest& Request)
{
    if (Request.MaxPing <= 0 || Request.MaxPing > 1000)
    {
        return false;
    }
    
    if (Request.SkillRating < 100.0f || Request.SkillRating > 3000.0f)
    {
        return false;
    }
    
    return true;
}

float UHSMatchmakingSystem::CalculateSkillDifference(float PlayerRating, float TargetRating) const
{
    return FMath::Abs(PlayerRating - TargetRating);
}

bool UHSMatchmakingSystem::IsSkillMatchSuitable(const FHSPlayerMatchmakingInfo& Player1, const FHSPlayerMatchmakingInfo& Player2) const
{
    float SkillDiff = CalculateSkillDifference(Player1.SkillRating, Player2.SkillRating);
    return SkillDiff <= CurrentSkillTolerance;
}

EHSRegion UHSMatchmakingSystem::DetectOptimalRegion() const
{
    if (PlayerInfo.Region != EHSRegion::Auto)
    {
        return PlayerInfo.Region;
    }

    {
        FScopeLock Lock(&MatchmakingMutex);
        EHSRegion BestRegion = EHSRegion::Asia;
        int32 BestPing = MAX_int32;

        for (const auto& Pair : RegionPingStats)
        {
            if (Pair.Value.SampleCount == 0)
            {
                continue;
            }

            const int32 AveragePing = Pair.Value.GetAverage();
            if (AveragePing < BestPing)
            {
                BestPing = AveragePing;
                BestRegion = Pair.Key;
            }
        }

        if (BestPing != MAX_int32)
        {
            return BestRegion;
        }
    }

    // 지역 정보가 없는 경우 시스템 로케일 기반으로 추정
    const TSharedRef<const FCulture> Culture = FInternationalization::Get().GetCurrentCulture();
    const FString CultureName = Culture->GetName(); // 예: ko-KR, en-US
    FString RegionCode;
    if (CultureName.Split(TEXT("-"), nullptr, &RegionCode))
    {
        return ParseRegionTag(RegionCode);
    }

    return EHSRegion::Asia;
}

int32 UHSMatchmakingSystem::EstimatePingToRegion(EHSRegion Region) const
{
    if (Region == EHSRegion::Auto)
    {
        Region = DetectOptimalRegion();
    }

    {
        FScopeLock Lock(&MatchmakingMutex);
        if (const FRegionPingStats* Stats = RegionPingStats.Find(Region))
        {
            const int32 Average = Stats->GetAverage();
            if (Average != MAX_int32)
            {
                return Average;
            }
        }
    }

    if (SessionSearch.IsValid())
    {
        double TotalPing = 0.0;
        int32 SampleCount = 0;
        for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
        {
            FString RegionString;
            EHSRegion ResultRegion = Region;
            if (SearchResult.Session.SessionSettings.Get(FName("Region"), RegionString))
            {
                ResultRegion = ParseRegionTag(RegionString);
            }

            if (ResultRegion == Region)
            {
                TotalPing += SearchResult.PingInMs;
                ++SampleCount;
            }
        }

        if (SampleCount > 0)
        {
            return static_cast<int32>(TotalPing / SampleCount);
        }
    }

    return CurrentRequest.MaxPing;
}

void UHSMatchmakingSystem::UpdateEstimatedWaitTime()
{
    float NewWaitTime = GetEstimatedWaitTime();
    OnEstimatedWaitTimeUpdated.Broadcast(NewWaitTime);
}

void UHSMatchmakingSystem::ExpandSearchCriteria()
{
    // 스킬 허용 범위 확장
    CurrentSkillTolerance += SkillToleranceGrowthRate;
    CurrentSkillTolerance = FMath::Min(CurrentSkillTolerance, 500.0f);
    
    // 핑 허용 범위 확장
    CurrentMaxPing = FMath::Min(CurrentMaxPing + 20, MaxPingThreshold);
    
    UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 검색 조건 완화 - 스킬 허용범위: %.1f, 최대 핑: %d"), 
           CurrentSkillTolerance, CurrentMaxPing);
}

void UHSMatchmakingSystem::HandleSessionSearchComplete(bool bWasSuccessful)
{
    if (!SessionSearch.IsValid())
    {
        return;
    }

    FScopeLock Lock(&MatchmakingMutex);
    
    if (!bWasSuccessful)
    {
        UE_LOG(LogTemp, Error, TEXT("HSMatchmakingSystem: 세션 검색이 실패했습니다"));
        return;
    }

    if (SessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMatchmakingSystem: 세션 검색 결과가 없습니다"));
        ExpandSearchCriteria();
        return;
    }

    const FOnlineSessionSearchResult* BestResult = nullptr;
    EHSRegion BestRegion = EHSRegion::Auto;
    int32 BestPing = MAX_int32;

    for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
    {
        if (!SearchResult.IsValid())
        {
            continue;
        }

        FString RegionString;
        EHSRegion ResultRegion = EHSRegion::Auto;
        if (SearchResult.Session.SessionSettings.Get(FName("Region"), RegionString))
        {
            ResultRegion = ParseRegionTag(RegionString);
        }

        RecordRegionPingSample(ResultRegion, SearchResult.PingInMs);

        if (ResultRegion == EHSRegion::Auto)
        {
            if (CurrentRequest.PreferredRegion != EHSRegion::Auto)
            {
                ResultRegion = CurrentRequest.PreferredRegion;
            }
            else if (PlayerInfo.Region != EHSRegion::Auto)
            {
                ResultRegion = PlayerInfo.Region;
            }
            else
            {
                ResultRegion = EHSRegion::Asia;
            }
        }

        if (CurrentRequest.PreferredRegion != EHSRegion::Auto && ResultRegion != CurrentRequest.PreferredRegion)
        {
            continue;
        }

        const int32 MaxConnections = SearchResult.Session.SessionSettings.NumPublicConnections + SearchResult.Session.SessionSettings.NumPrivateConnections;
        const int32 OpenConnections = SearchResult.Session.NumOpenPublicConnections + SearchResult.Session.NumOpenPrivateConnections;
        if (MaxConnections > 0 && OpenConnections <= 0)
        {
            continue;
        }

        if (SearchResult.PingInMs > CurrentMaxPing)
        {
            continue;
        }

        if (!BestResult || SearchResult.PingInMs < BestPing)
        {
            BestResult = &SearchResult;
            BestRegion = ResultRegion;
            BestPing = SearchResult.PingInMs;
        }
    }

    if (BestResult)
    {
        PendingSessionResult = *BestResult;
        bHasPendingSessionResult = true;
        CurrentMatchID = BestResult->GetSessionIdStr();

        FHSMatchInfo MatchInfo = BuildMatchInfoFromResult(*BestResult, BestRegion);
        UpdateMatchmakingStatus(EHSMatchmakingStatus::MatchFound);
        OnMatchFound.Broadcast(MatchInfo);
        OnMatchAcceptanceTimeout();
        
        UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 조건에 맞는 세션 발견 - ID: %s, 핑: %dms"),
               *CurrentMatchID, MatchInfo.PingMs);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMatchmakingSystem: 조건에 맞는 세션을 찾지 못했습니다"));
        ExpandSearchCriteria();
    }
}

void UHSMatchmakingSystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    bHasPendingSessionResult = false;
    PendingSessionResult = FOnlineSessionSearchResult();

    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 세션 참가 성공: %s"), *SessionName.ToString());
        UpdateMatchmakingStatus(EHSMatchmakingStatus::InMatch);
    }
    else
    {
        const TCHAR* ResultString = LexToString(Result);
        UE_LOG(LogTemp, Error, TEXT("HSMatchmakingSystem: 세션 참가 실패: %s (%s)"), *SessionName.ToString(), ResultString);
        HandleMatchmakingError(FString::Printf(TEXT("세션 참가 실패 (%s)"), ResultString));
    }
}

void UHSMatchmakingSystem::ResetMatchmakingState()
{
    CurrentMatchID.Empty();
    SearchStartTime = 0.0f;
    LastSearchRequestTime = 0.0f;
    CurrentSkillTolerance = SkillToleranceBase;
    CurrentMaxPing = CurrentRequest.MaxPing;
    CachedWaitTime = 0.0f;
    WaitTimeCacheTimestamp = 0.0f;
    bHasPendingSessionResult = false;
    PendingSessionResult = FOnlineSessionSearchResult();
}

void UHSMatchmakingSystem::CleanupMatchmakingResources()
{
    // 타이머 정리
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(MatchmakingTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(WaitTimeUpdateTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(MatchAcceptanceTimeoutHandle);
    }
    
    // 세션 검색 정리
    if (SessionInterface.IsValid())
    {
        SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
        SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
    }
    
    SessionSearch.Reset();
    bHasPendingSessionResult = false;
    PendingSessionResult = FOnlineSessionSearchResult();
}

FString UHSMatchmakingSystem::GetRegionTag(EHSRegion Region)
{
    switch (Region)
    {
        case EHSRegion::NorthAmerica: return TEXT("NA");
        case EHSRegion::Europe: return TEXT("EU");
        case EHSRegion::Asia: return TEXT("AS");
        case EHSRegion::Oceania: return TEXT("OC");
        case EHSRegion::SouthAmerica: return TEXT("SA");
        default: return TEXT("Global");
    }
}

EHSRegion UHSMatchmakingSystem::ParseRegionTag(const FString& RegionString)
{
    FString Normalized = RegionString.ToUpper();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));

    if (Normalized.IsEmpty() || Normalized == TEXT("GLOBAL") || Normalized == TEXT("AUTO"))
    {
        return EHSRegion::Auto;
    }

    if (Normalized == TEXT("NA") || Normalized == TEXT("NORTHAMERICA") || Normalized == TEXT("US") || Normalized == TEXT("USA") || Normalized == TEXT("CA") || Normalized == TEXT("CANADA"))
    {
        return EHSRegion::NorthAmerica;
    }

    if (Normalized == TEXT("EU") || Normalized == TEXT("EUROPE") || Normalized == TEXT("UK") || Normalized == TEXT("DE") || Normalized == TEXT("FR") || Normalized == TEXT("IT") || Normalized == TEXT("ES"))
    {
        return EHSRegion::Europe;
    }

    if (Normalized == TEXT("AS") || Normalized == TEXT("ASIA") || Normalized == TEXT("KR") || Normalized == TEXT("JPN") || Normalized == TEXT("JP") || Normalized == TEXT("CN") || Normalized == TEXT("SG"))
    {
        return EHSRegion::Asia;
    }

    if (Normalized == TEXT("OC") || Normalized == TEXT("OCEANIA") || Normalized == TEXT("AUS") || Normalized == TEXT("AU") || Normalized == TEXT("NZ"))
    {
        return EHSRegion::Oceania;
    }

    if (Normalized == TEXT("SA") || Normalized == TEXT("SOUTHAMERICA") || Normalized == TEXT("BR") || Normalized == TEXT("BRAZIL") || Normalized == TEXT("AR") || Normalized == TEXT("ARGENTINA") || Normalized == TEXT("CL"))
    {
        return EHSRegion::SouthAmerica;
    }

    return EHSRegion::Auto;
}

void UHSMatchmakingSystem::RecordRegionPingSample(EHSRegion Region, int32 Ping)
{
    if (Region == EHSRegion::Auto)
    {
        Region = (CurrentRequest.PreferredRegion != EHSRegion::Auto) ? CurrentRequest.PreferredRegion : EHSRegion::Asia;
    }

    FRegionPingStats& Stats = RegionPingStats.FindOrAdd(Region);
    Stats.AddSample(Ping);
}

FHSMatchInfo UHSMatchmakingSystem::BuildMatchInfoFromResult(const FOnlineSessionSearchResult& SearchResult, EHSRegion Region) const
{
    FHSMatchInfo MatchInfo;
    MatchInfo.MatchID = SearchResult.GetSessionIdStr();
    
    EHSRegion ResolvedRegion = Region;
    if (ResolvedRegion == EHSRegion::Auto)
    {
        if (CurrentRequest.PreferredRegion != EHSRegion::Auto)
        {
            ResolvedRegion = CurrentRequest.PreferredRegion;
        }
        else if (PlayerInfo.Region != EHSRegion::Auto)
        {
            ResolvedRegion = PlayerInfo.Region;
        }
        else
        {
            ResolvedRegion = EHSRegion::Asia;
        }
    }
    MatchInfo.Region = ResolvedRegion;
    MatchInfo.PingMs = SearchResult.PingInMs;

    const int32 MaxConnections = SearchResult.Session.SessionSettings.NumPublicConnections + SearchResult.Session.SessionSettings.NumPrivateConnections;
    const int32 OpenConnections = SearchResult.Session.NumOpenPublicConnections + SearchResult.Session.NumOpenPrivateConnections;
    MatchInfo.MaxPlayers = MaxConnections;
    MatchInfo.CurrentPlayers = FMath::Clamp(MaxConnections - OpenConnections, 0, MaxConnections);

    float AverageSkill = PlayerInfo.SkillRating;
    SearchResult.Session.SessionSettings.Get(TEXT("AVERAGE_SKILL"), AverageSkill);
    SearchResult.Session.SessionSettings.Get(TEXT("AVERAGE_SKILL_RATING"), AverageSkill);
    MatchInfo.AverageSkillRating = AverageSkill;

    FString ServerAddress;
    if (SessionInterface.IsValid() && SessionInterface->GetResolvedConnectString(SearchResult, NAME_GamePort, ServerAddress))
    {
        MatchInfo.ServerAddress = ServerAddress;
    }
    else
    {
        MatchInfo.ServerAddress = SearchResult.Session.OwningUserName;
    }

    return MatchInfo;
}

FString UHSMatchmakingSystem::GenerateMatchID() const
{
    return FGuid::NewGuid().ToString();
}

void UHSMatchmakingSystem::HandleMatchmakingError(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("HSMatchmakingSystem: 매치메이킹 에러 - %s"), *ErrorMessage);
    
    UpdateMatchmakingStatus(EHSMatchmakingStatus::Error);
    OnMatchmakingError.Broadcast(ErrorMessage);
    
    ResetMatchmakingState();
}

void UHSMatchmakingSystem::OnMatchAcceptanceTimeout()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            MatchAcceptanceTimeoutHandle,
            [this]() {
                if (CurrentStatus == EHSMatchmakingStatus::MatchFound)
                {
                    DeclineMatch(CurrentMatchID);
                }
            },
            MatchAcceptanceTimeoutSeconds,
            false
        );
    }
}
