#include "HSMatchmakingSystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"

UHSMatchmakingSystem::UHSMatchmakingSystem()
{
    CurrentStatus = EHSMatchmakingStatus::NotSearching;
    CachedWaitTime = 0.0f;
    WaitTimeCacheTimestamp = 0.0f;
    SearchStartTime = 0.0f;
    CurrentSkillTolerance = SkillToleranceBase;
    CurrentMaxPing = 100;
    SessionSearch = nullptr;
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
    CurrentSkillTolerance = SkillToleranceBase;
    CurrentMaxPing = Request.MaxPing;
    
    // 지역 자동 감지
    if (Request.PreferredRegion == EHSRegion::Auto)
    {
        CurrentRequest.PreferredRegion = DetectOptimalRegion();
    }
    
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
        FString RegionString = TEXT("Global");
        switch (CurrentRequest.PreferredRegion)
        {
            case EHSRegion::NorthAmerica: RegionString = TEXT("NA"); break;
            case EHSRegion::Europe: RegionString = TEXT("EU"); break;
            case EHSRegion::Asia: RegionString = TEXT("AS"); break;
            case EHSRegion::Oceania: RegionString = TEXT("OC"); break;
            case EHSRegion::SouthAmerica: RegionString = TEXT("SA"); break;
        }
        SessionSearch->QuerySettings.Set(FName("Region"), RegionString, EOnlineComparisonOp::Equals);
        
        // 세션 검색 델리게이트 바인딩
        SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
            this, &UHSMatchmakingSystem::HandleSessionSearchComplete);
        
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
    
    // 세션 참가 처리 (실제 온라인 서브시스템 연동)
    if (SessionInterface.IsValid())
    {
        // 여기서 실제 세션 참가 로직 구현
        // 현재는 시뮬레이션
        // 매치 정보를 브로드캐스트하는 대신 상태만 업데이트
        UpdateMatchmakingStatus(EHSMatchmakingStatus::InMatch);
    }
    
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
    
    // 시뮬레이션: 일정 확률로 매치 발견
    float MatchProbability = FMath::Min(ElapsedTime / 60.0f, 0.95f); // 시간이 지날수록 확률 증가
    if (FMath::RandRange(0.0f, 1.0f) < MatchProbability * 0.1f) // 매 초마다 10% 확률
    {
        // 가상 매치 생성
        FHSMatchInfo MatchInfo;
        MatchInfo.MatchID = GenerateMatchID();
        MatchInfo.ServerAddress = TEXT("127.0.0.1:7777");
        MatchInfo.CurrentPlayers = FMath::RandRange(1, 3);
        MatchInfo.MaxPlayers = 4;
        MatchInfo.PingMs = EstimatePingToRegion(CurrentRequest.PreferredRegion);
        MatchInfo.Region = CurrentRequest.PreferredRegion;
        MatchInfo.AverageSkillRating = PlayerInfo.SkillRating + FMath::RandRange(-200.0f, 200.0f);
        
        CurrentMatchID = MatchInfo.MatchID;
        
        UpdateMatchmakingStatus(EHSMatchmakingStatus::MatchFound);
        OnMatchFound.Broadcast(MatchInfo);
        
        // 매치 수락 타임아웃 시작
        OnMatchAcceptanceTimeout();
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
    // 실제 구현에서는 IP 기반 지역 감지 또는 핑 테스트 수행
    // 현재는 간단한 시뮬레이션
    return EHSRegion::Asia; // 한국 기본값
}

int32 UHSMatchmakingSystem::EstimatePingToRegion(EHSRegion Region) const
{
    // 지역별 예상 핑 (한국 기준)
    switch (Region)
    {
        case EHSRegion::Asia: return FMath::RandRange(10, 30);
        case EHSRegion::NorthAmerica: return FMath::RandRange(150, 200);
        case EHSRegion::Europe: return FMath::RandRange(250, 300);
        case EHSRegion::Oceania: return FMath::RandRange(80, 120);
        case EHSRegion::SouthAmerica: return FMath::RandRange(300, 350);
        default: return FMath::RandRange(50, 100);
    }
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
    
    if (bWasSuccessful && SessionSearch->SearchResults.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: %d개 세션 발견"), SessionSearch->SearchResults.Num());
        
        // 최적 세션 선택 로직
        for (const auto& SearchResult : SessionSearch->SearchResults)
        {
            if (SearchResult.IsValid())
            {
                // 세션 정보 검증 및 매치 생성
                // 실제 구현에서는 여기서 세션 참가 시도
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMatchmakingSystem: 세션 검색 실패 또는 결과 없음"));
    }
}

void UHSMatchmakingSystem::HandleJoinSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("HSMatchmakingSystem: 세션 참가 성공: %s"), *SessionName.ToString());
        UpdateMatchmakingStatus(EHSMatchmakingStatus::InMatch);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HSMatchmakingSystem: 세션 참가 실패: %s"), *SessionName.ToString());
        HandleMatchmakingError(TEXT("세션 참가 실패"));
    }
}

void UHSMatchmakingSystem::ResetMatchmakingState()
{
    CurrentMatchID.Empty();
    SearchStartTime = 0.0f;
    CurrentSkillTolerance = SkillToleranceBase;
    CurrentMaxPing = CurrentRequest.MaxPing;
    CachedWaitTime = 0.0f;
    WaitTimeCacheTimestamp = 0.0f;
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
