// HuntingSpirit 게임의 보상 분배 시스템 구현

#include "HSRewardsSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFilemanager.h"
#include "../Inventory/HSInventoryComponent.h"
#include "../../Items/HSItemBase.h"
#include "../../Characters/Player/HSPlayerCharacter.h"
#include "../../Characters/Stats/HSStatsComponent.h"
#include "../../Characters/Stats/HSLevelSystem.h"

UHSRewardsSystem::UHSRewardsSystem()
{
    // 기본 설정 초기화
    bSessionActive = false;
    ContributionWeight = 0.6f;
    NeedWeight = 0.3f;
    RandomWeight = 0.1f;
    VoteTimeLimit = 120.0f; // 2분
    
    // 풀링 초기화
    ResultPool.Reserve(50);
    ContributionPool.Reserve(20);
    
    // 캐시 초기화
    LastContributionUpdate = FDateTime::MinValue();
}

void UHSRewardsSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 시스템 초기화 시작"));
    
    // 기여도 가중치 초기화
    InitializeContributionWeights();
    
    // 정기 업데이트 타이머 설정 (5초마다)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            UpdateTimer,
            this,
            &UHSRewardsSystem::PerformPeriodicUpdate,
            5.0f,
            true
        );
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 시스템 초기화 완료"));
}

void UHSRewardsSystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 시스템 정리 시작"));
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(UpdateTimer);
    }
    
    // 활성 세션 종료
    if (bSessionActive)
    {
        EndRewardSession(CurrentSession.SessionID);
    }
    
    // 데이터 정리
    PlayerContributions.Empty();
    CurrentVotes.Empty();
    DistributionHistory.Empty();
    ContributionCache.Empty();
    RewardValueCache.Empty();
    NeedAnalysisCache.Empty();
    ResultPool.Empty();
    ContributionPool.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 시스템 정리 완료"));
    
    Super::Deinitialize();
}

// === 기여도 추적 시스템 구현 ===

void UHSRewardsSystem::AddContribution(int32 PlayerID, EHSContributionType ContributionType, float Amount)
{
    if (!IsValidPlayer(PlayerID) || Amount <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 잘못된 기여도 추가 시도 - PlayerID: %d, Amount: %f"), 
               PlayerID, Amount);
        return;
    }
    
    // 기여도 가중치 적용
    const float* Weight = ContributionWeights.Find(ContributionType);
    float WeightedAmount = Weight ? Amount * (*Weight) : Amount;
    
    // 플레이어 기여도 정보 가져오기 또는 생성
    FHSPlayerContribution& Contribution = PlayerContributions.FindOrAdd(PlayerID);
    
    // 플레이어 정보 설정 (처음 추가하는 경우)
    if (Contribution.PlayerID == -1)
    {
        Contribution.PlayerID = PlayerID;
        
        // 플레이어 이름 가져오기
        if (UWorld* World = GetWorld())
        {
            if (AGameStateBase* GameState = World->GetGameState())
            {
                for (APlayerState* PS : GameState->PlayerArray)
                {
                    if (PS && PS->GetPlayerId() == PlayerID)
                    {
                        Contribution.PlayerName = PS->GetPlayerName();
                        break;
                    }
                }
            }
        }
        
        if (Contribution.PlayerName.IsEmpty())
        {
            Contribution.PlayerName = FString::Printf(TEXT("Player_%d"), PlayerID);
        }
    }
    
    // 기여도 점수 업데이트
    float& CurrentScore = Contribution.ContributionScores.FindOrAdd(ContributionType, 0.0f);
    CurrentScore += WeightedAmount;
    
    // 총 점수 재계산
    Contribution.TotalScore = 0.0f;
    for (const auto& ScorePair : Contribution.ContributionScores)
    {
        Contribution.TotalScore += ScorePair.Value;
    }
    
    Contribution.LastUpdateTime = FDateTime::Now();
    
    // 캐시 무효화
    ContributionCache.Remove(PlayerID);
    LastContributionUpdate = FDateTime::Now();
    
    // 델리게이트 호출
    OnContributionUpdated.Broadcast(PlayerID, Contribution);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSRewardsSystem: 기여도 추가됨 - Player: %d, Type: %d, Amount: %f, Total: %f"), 
           PlayerID, (int32)ContributionType, WeightedAmount, Contribution.TotalScore);
}

FHSPlayerContribution UHSRewardsSystem::GetPlayerContribution(int32 PlayerID) const
{
    // 캐시 먼저 확인
    if (const FHSPlayerContribution* CachedContribution = ContributionCache.Find(PlayerID))
    {
        FDateTime CurrentTime = FDateTime::Now();
        if ((CurrentTime - LastContributionUpdate).GetTotalSeconds() < 5.0f)
        {
            return *CachedContribution;
        }
    }
    
    // 캐시 미스 또는 만료된 경우 데이터 조회
    if (const FHSPlayerContribution* Contribution = PlayerContributions.Find(PlayerID))
    {
        // 캐시 업데이트 (mutable 변수 직접 수정)
        ContributionCache.Add(PlayerID, *Contribution);
        return *Contribution;
    }
    
    return FHSPlayerContribution();
}

TArray<FHSPlayerContribution> UHSRewardsSystem::GetAllContributions() const
{
    TArray<FHSPlayerContribution> Result;
    
    for (const auto& ContributionPair : PlayerContributions)
    {
        Result.Add(ContributionPair.Value);
    }
    
    // 총 점수순으로 정렬
    Result.Sort([](const FHSPlayerContribution& A, const FHSPlayerContribution& B)
    {
        return A.TotalScore > B.TotalScore;
    });
    
    return Result;
}

void UHSRewardsSystem::ResetContributions()
{
    // 기여도를 풀로 반환
    for (const auto& ContributionPair : PlayerContributions)
    {
        ContributionPool.Add(ContributionPair.Value);
    }
    
    PlayerContributions.Empty();
    ContributionCache.Empty();
    LastContributionUpdate = FDateTime::Now();
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 모든 기여도 초기화됨"));
}

void UHSRewardsSystem::CalculateContributionPercentages()
{
    // 총 기여도 계산
    float TotalContribution = 0.0f;
    for (const auto& ContributionPair : PlayerContributions)
    {
        TotalContribution += ContributionPair.Value.TotalScore;
    }
    
    if (TotalContribution <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 총 기여도가 0 이하입니다"));
        return;
    }
    
    // 각 플레이어의 기여도 백분율 계산
    for (auto& ContributionPair : PlayerContributions)
    {
        FHSPlayerContribution& Contribution = ContributionPair.Value;
        Contribution.ContributionPercentage = (Contribution.TotalScore / TotalContribution) * 100.0f;
    }
    
    // 캐시 무효화
    ContributionCache.Empty();
    LastContributionUpdate = FDateTime::Now();
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 기여도 백분율 계산 완료 - 총 기여도: %f"), TotalContribution);
}

// === 보상 세션 관리 구현 ===

FString UHSRewardsSystem::StartRewardSession(const TArray<FHSRewardItem>& Rewards, EHSDistributionType DistributionType)
{
    if (bSessionActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 이미 활성 세션이 존재합니다"));
        return TEXT("");
    }
    
    if (Rewards.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 보상이 없어 세션을 시작할 수 없습니다"));
        return TEXT("");
    }
    
    // 새 세션 생성
    CurrentSession = FHSRewardSession();
    CurrentSession.SessionID = GenerateSessionID();
    CurrentSession.AvailableRewards = Rewards;
    CurrentSession.DistributionType = DistributionType;
    CurrentSession.StartTime = FDateTime::Now();
    CurrentSession.bDistributionComplete = false;
    
    // 현재 기여도 스냅샷 저장
    CurrentSession.PlayerContributions = GetAllContributions();
    
    bSessionActive = true;
    
    // 투표 초기화
    ClearVotes();
    
    // 델리게이트 호출
    OnRewardSessionStarted.Broadcast(CurrentSession);
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 세션 시작됨 - ID: %s, 보상 수: %d, 분배 방식: %d"), 
           *CurrentSession.SessionID, Rewards.Num(), (int32)DistributionType);
    
    return CurrentSession.SessionID;
}

bool UHSRewardsSystem::EndRewardSession(const FString& SessionID)
{
    if (!bSessionActive || CurrentSession.SessionID != SessionID)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 잘못된 세션 ID 또는 비활성 세션: %s"), *SessionID);
        return false;
    }
    
    CurrentSession.EndTime = FDateTime::Now();
    CurrentSession.bDistributionComplete = true;
    bSessionActive = false;
    
    // 투표 정리
    ClearVotes();
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 세션 종료됨 - ID: %s"), *SessionID);
    
    return true;
}

FHSRewardSession UHSRewardsSystem::GetActiveSession() const
{
    if (bSessionActive)
    {
        return CurrentSession;
    }
    
    return FHSRewardSession();
}

bool UHSRewardsSystem::IsSessionActive() const
{
    return bSessionActive;
}

// === 보상 분배 구현 ===

TArray<FHSDistributionResult> UHSRewardsSystem::DistributeRewards(const FString& SessionID)
{
    if (!bSessionActive || CurrentSession.SessionID != SessionID)
    {
        UE_LOG(LogTemp, Error, TEXT("HSRewardsSystem: 잘못된 세션 ID 또는 비활성 세션: %s"), *SessionID);
        return TArray<FHSDistributionResult>();
    }
    
    TArray<FHSDistributionResult> Results;
    
    // 분배 방식에 따른 분배 실행
    switch (CurrentSession.DistributionType)
    {
        case EHSDistributionType::Equal:
            Results = DistributeEqually(CurrentSession.AvailableRewards);
            break;
            
        case EHSDistributionType::Contribution:
            Results = DistributeByContribution(CurrentSession.AvailableRewards);
            break;
            
        case EHSDistributionType::Need:
            Results = DistributeByNeed(CurrentSession.AvailableRewards);
            break;
            
        case EHSDistributionType::Random:
            Results = DistributeRandomly(CurrentSession.AvailableRewards);
            break;
            
        case EHSDistributionType::Vote:
            // 투표 기반 분배는 각 아이템별로 개별 처리
            for (const FHSRewardItem& Reward : CurrentSession.AvailableRewards)
            {
                FHSDistributionResult VoteResult = DistributeByVote(Reward.ItemID);
                if (VoteResult.RecipientPlayerID != -1)
                {
                    Results.Add(VoteResult);
                }
            }
            break;
            
        default:
            UE_LOG(LogTemp, Error, TEXT("HSRewardsSystem: 지원되지 않는 분배 방식: %d"), 
                   (int32)CurrentSession.DistributionType);
            return Results;
    }
    
    // 분배 결과 검증
    if (!ValidateDistributionResults(Results))
    {
        UE_LOG(LogTemp, Error, TEXT("HSRewardsSystem: 분배 결과 검증 실패"));
        return TArray<FHSDistributionResult>();
    }
    
    // 분배 기록 저장
    DistributionHistory.Append(Results);
    
    // 세션 완료 상태 업데이트
    CurrentSession.bDistributionComplete = true;
    
    // 델리게이트 호출
    OnRewardDistributed.Broadcast(Results);
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 보상 분배 완료 - 결과 수: %d"), Results.Num());
    
    return Results;
}

TArray<FHSDistributionResult> UHSRewardsSystem::DistributeEqually(const TArray<FHSRewardItem>& Rewards)
{
    TArray<FHSDistributionResult> Results;
    
    if (PlayerContributions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 참여 플레이어가 없어 균등 분배할 수 없습니다"));
        return Results;
    }
    
    // 플레이어 목록 생성
    TArray<int32> PlayerIDs;
    for (const auto& ContributionPair : PlayerContributions)
    {
        PlayerIDs.Add(ContributionPair.Key);
    }
    
    int32 PlayerCount = PlayerIDs.Num();
    
    // 각 보상에 대해 균등 분배
    for (int32 RewardIndex = 0; RewardIndex < Rewards.Num(); RewardIndex++)
    {
        const FHSRewardItem& Reward = Rewards[RewardIndex];
        int32 RecipientIndex = RewardIndex % PlayerCount;
        int32 RecipientPlayerID = PlayerIDs[RecipientIndex];
        
        // 결과 객체 재사용을 위해 오브젝트 풀링 활용
        FHSDistributionResult* Result = nullptr;
        if (ResultPool.Num() > 0)
        {
            FHSDistributionResult TempResult = ResultPool.Pop();
            Result = &TempResult;
            *Result = FHSDistributionResult();
            Results.Add(*Result);
            Result = &Results.Last();
        }
        else
        {
            Results.Add(FHSDistributionResult());
            Result = &Results.Last();
        }
        
        Result->RecipientPlayerID = RecipientPlayerID;
        Result->RewardItems.Add(Reward);
        Result->TotalValue = CalculateRewardValue(Reward);
        Result->Reason = TEXT("균등 분배");
        
        if (ResultPool.Num() == 0)
        {
            Results.Add(*Result);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 균등 분배 완료 - 플레이어 수: %d, 보상 수: %d"), 
           PlayerCount, Rewards.Num());
    
    return Results;
}

TArray<FHSDistributionResult> UHSRewardsSystem::DistributeByContribution(const TArray<FHSRewardItem>& Rewards)
{
    TArray<FHSDistributionResult> Results;
    
    // 기여도 백분율 계산
    CalculateContributionPercentages();
    
    // 기여도 순으로 정렬된 플레이어 목록
    TArray<FHSPlayerContribution> SortedContributions = GetAllContributions();
    
    if (SortedContributions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 기여도 데이터가 없어 분배할 수 없습니다"));
        return Results;
    }
    
    // 보상 가치별로 정렬 (높은 가치부터)
    TArray<FHSRewardItem> SortedRewards = Rewards;
    SortedRewards.Sort([this](const FHSRewardItem& A, const FHSRewardItem& B)
    {
        return CalculateRewardValue(A) > CalculateRewardValue(B);
    });
    
    // 높은 기여도 플레이어에게 높은 가치 보상 우선 분배
    for (int32 i = 0; i < SortedRewards.Num() && i < SortedContributions.Num(); i++)
    {
        const FHSRewardItem& Reward = SortedRewards[i];
        const FHSPlayerContribution& Contribution = SortedContributions[i];
        
        FHSDistributionResult Result;
        Result.RecipientPlayerID = Contribution.PlayerID;
        Result.RewardItems.Add(Reward);
        Result.TotalValue = CalculateRewardValue(Reward);
        Result.Reason = FString::Printf(TEXT("기여도 %.1f%% (순위 %d)"), 
                                       Contribution.ContributionPercentage, i + 1);
        
        Results.Add(Result);
    }
    
    // 남은 보상이 있으면 기여도 비율에 따라 추가 분배
    for (int32 i = SortedContributions.Num(); i < SortedRewards.Num(); i++)
    {
        const FHSRewardItem& Reward = SortedRewards[i];
        
        // 가중 랜덤 선택 (기여도 비율 기반)
        float TotalWeight = 0.0f;
        for (const FHSPlayerContribution& Contribution : SortedContributions)
        {
            TotalWeight += Contribution.ContributionPercentage;
        }
        
        float RandomValue = FMath::FRandRange(0.0f, TotalWeight);
        float CurrentWeight = 0.0f;
        
        for (const FHSPlayerContribution& Contribution : SortedContributions)
        {
            CurrentWeight += Contribution.ContributionPercentage;
            if (RandomValue <= CurrentWeight)
            {
                FHSDistributionResult Result;
                Result.RecipientPlayerID = Contribution.PlayerID;
                Result.RewardItems.Add(Reward);
                Result.TotalValue = CalculateRewardValue(Reward);
                Result.Reason = FString::Printf(TEXT("기여도 비율 선택 (%.1f%%)"), 
                                               Contribution.ContributionPercentage);
                
                Results.Add(Result);
                break;
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 기여도 비례 분배 완료 - 결과 수: %d"), Results.Num());
    
    return Results;
}

TArray<FHSDistributionResult> UHSRewardsSystem::DistributeByNeed(const TArray<FHSRewardItem>& Rewards)
{
    TArray<FHSDistributionResult> Results;
    
    // 각 보상에 대해 가장 필요도가 높은 플레이어에게 분배
    for (const FHSRewardItem& Reward : Rewards)
    {
        int32 BestPlayerID = -1;
        float HighestNeed = 0.0f;
        
        for (const auto& ContributionPair : PlayerContributions)
        {
            int32 PlayerID = ContributionPair.Key;
            float NeedScore = AnalyzePlayerNeed(PlayerID, Reward);
            
            if (NeedScore > HighestNeed)
            {
                HighestNeed = NeedScore;
                BestPlayerID = PlayerID;
            }
        }
        
        if (BestPlayerID != -1)
        {
            FHSDistributionResult Result;
            Result.RecipientPlayerID = BestPlayerID;
            Result.RewardItems.Add(Reward);
            Result.TotalValue = CalculateRewardValue(Reward);
            Result.Reason = FString::Printf(TEXT("필요도 기반 (점수: %.2f)"), HighestNeed);
            
            Results.Add(Result);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 필요도 기반 분배 완료 - 결과 수: %d"), Results.Num());
    
    return Results;
}

TArray<FHSDistributionResult> UHSRewardsSystem::DistributeRandomly(const TArray<FHSRewardItem>& Rewards)
{
    TArray<FHSDistributionResult> Results;
    
    if (PlayerContributions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 참여 플레이어가 없어 랜덤 분배할 수 없습니다"));
        return Results;
    }
    
    // 플레이어 ID 배열 생성
    TArray<int32> PlayerIDs;
    for (const auto& ContributionPair : PlayerContributions)
    {
        PlayerIDs.Add(ContributionPair.Key);
    }
    
    // 각 보상을 랜덤한 플레이어에게 분배
    for (const FHSRewardItem& Reward : Rewards)
    {
        int32 RandomIndex = FMath::RandRange(0, PlayerIDs.Num() - 1);
        int32 RandomPlayerID = PlayerIDs[RandomIndex];
        
        FHSDistributionResult Result;
        Result.RecipientPlayerID = RandomPlayerID;
        Result.RewardItems.Add(Reward);
        Result.TotalValue = CalculateRewardValue(Reward);
        Result.Reason = TEXT("랜덤 선택");
        
        Results.Add(Result);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 랜덤 분배 완료 - 결과 수: %d"), Results.Num());
    
    return Results;
}

// === 투표 시스템 구현 ===

bool UHSRewardsSystem::SubmitVote(int32 VoterPlayerID, int32 CandidatePlayerID, FName ItemID)
{
    if (!IsValidPlayer(VoterPlayerID) || !IsValidPlayer(CandidatePlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 잘못된 플레이어 ID - Voter: %d, Candidate: %d"), 
               VoterPlayerID, CandidatePlayerID);
        return false;
    }
    
    // 기존 투표 확인 및 제거
    CurrentVotes.RemoveAll([VoterPlayerID, ItemID](const FHSRewardVote& Vote)
    {
        return Vote.VoterPlayerID == VoterPlayerID && Vote.ItemID == ItemID;
    });
    
    // 새 투표 추가
    FHSRewardVote NewVote;
    NewVote.VoterPlayerID = VoterPlayerID;
    NewVote.CandidatePlayerID = CandidatePlayerID;
    NewVote.ItemID = ItemID;
    NewVote.VoteTime = FDateTime::Now();
    
    CurrentVotes.Add(NewVote);
    
    // 델리게이트 호출
    OnVoteSubmitted.Broadcast(VoterPlayerID, CandidatePlayerID, ItemID);
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 투표 제출됨 - Voter: %d, Candidate: %d, Item: %s"), 
           VoterPlayerID, CandidatePlayerID, *ItemID.ToString());
    
    return true;
}

TArray<FHSRewardVote> UHSRewardsSystem::GetVotesForItem(FName ItemID) const
{
    TArray<FHSRewardVote> ItemVotes;
    
    for (const FHSRewardVote& Vote : CurrentVotes)
    {
        if (Vote.ItemID == ItemID)
        {
            ItemVotes.Add(Vote);
        }
    }
    
    return ItemVotes;
}

FHSDistributionResult UHSRewardsSystem::DistributeByVote(FName ItemID)
{
    FHSDistributionResult Result;
    
    // 해당 아이템에 대한 투표 가져오기
    TArray<FHSRewardVote> ItemVotes = GetVotesForItem(ItemID);
    
    if (ItemVotes.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 아이템 %s에 대한 투표가 없습니다"), *ItemID.ToString());
        return Result;
    }
    
    // 후보별 득표 수 계산
    TMap<int32, int32> VoteCounts;
    for (const FHSRewardVote& Vote : ItemVotes)
    {
        int32& Count = VoteCounts.FindOrAdd(Vote.CandidatePlayerID, 0);
        Count++;
    }
    
    // 최다 득표자 찾기
    int32 WinnerPlayerID = -1;
    int32 MaxVotes = 0;
    
    for (const auto& VoteCountPair : VoteCounts)
    {
        if (VoteCountPair.Value > MaxVotes)
        {
            MaxVotes = VoteCountPair.Value;
            WinnerPlayerID = VoteCountPair.Key;
        }
    }
    
    if (WinnerPlayerID != -1)
    {
        // 보상 아이템 찾기
        const FHSRewardItem* RewardItem = nullptr;
        for (const FHSRewardItem& Reward : CurrentSession.AvailableRewards)
        {
            if (Reward.ItemID == ItemID)
            {
                RewardItem = &Reward;
                break;
            }
        }
        
        if (RewardItem)
        {
            Result.RecipientPlayerID = WinnerPlayerID;
            Result.RewardItems.Add(*RewardItem);
            Result.TotalValue = CalculateRewardValue(*RewardItem);
            Result.Reason = FString::Printf(TEXT("투표 당선 (%d표)"), MaxVotes);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 투표 분배 완료 - Item: %s, Winner: %d, Votes: %d"), 
           *ItemID.ToString(), WinnerPlayerID, MaxVotes);
    
    return Result;
}

void UHSRewardsSystem::ClearVotes()
{
    CurrentVotes.Empty();
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 모든 투표 초기화됨"));
}

// === 보상 분석 구현 ===

float UHSRewardsSystem::GetPlayerRewardValue(int32 PlayerID) const
{
    float TotalValue = 0.0f;
    
    for (const FHSDistributionResult& Result : DistributionHistory)
    {
        if (Result.RecipientPlayerID == PlayerID)
        {
            TotalValue += Result.TotalValue;
        }
    }
    
    return TotalValue;
}

float UHSRewardsSystem::GetSessionTotalValue() const
{
    float TotalValue = 0.0f;
    
    for (const FHSRewardItem& Reward : CurrentSession.AvailableRewards)
    {
        TotalValue += CalculateRewardValue(Reward);
    }
    
    return TotalValue;
}

float UHSRewardsSystem::CalculateFairnessIndex() const
{
    if (PlayerContributions.Num() <= 1)
    {
        return 1.0f; // 플레이어가 1명 이하면 완벽히 공정
    }
    
    // 각 플레이어의 보상 가치 계산
    TArray<float> PlayerRewards;
    for (const auto& ContributionPair : PlayerContributions)
    {
        float RewardValue = GetPlayerRewardValue(ContributionPair.Key);
        PlayerRewards.Add(RewardValue);
    }
    
    // 지니 계수 계산 (불평등 지수)
    // 0에 가까울수록 공정함
    PlayerRewards.Sort();
    
    float Sum = 0.0f;
    float WeightedSum = 0.0f;
    
    for (int32 i = 0; i < PlayerRewards.Num(); i++)
    {
        Sum += PlayerRewards[i];
        WeightedSum += (i + 1) * PlayerRewards[i];
    }
    
    if (Sum <= 0.0f)
    {
        return 1.0f;
    }
    
    float GiniCoeff = (2.0f * WeightedSum) / (PlayerRewards.Num() * Sum) - (PlayerRewards.Num() + 1.0f) / PlayerRewards.Num();
    
    // 공정성 지수로 변환 (1 - 지니계수)
    return FMath::Clamp(1.0f - GiniCoeff, 0.0f, 1.0f);
}

// === 유틸리티 구현 ===

float UHSRewardsSystem::CalculateRewardValue(const FHSRewardItem& Reward) const
{
    // 값 캐싱
    if (const float* CachedValue = RewardValueCache.Find(Reward.ItemID))
    {
        return *CachedValue;
    }
    
    float Value = 0.0f;
    
    // 보상 타입별 가치 계산
    switch (Reward.RewardType)
    {
        case EHSRewardType::Experience:
            Value = Reward.Value * 0.1f; // 경험치는 낮은 가치
            break;
            
        case EHSRewardType::Currency:
            Value = Reward.Value;
            break;
            
        case EHSRewardType::Item:
            // 아이템 희귀도와 수량 고려
            Value = Reward.Value * (1.0f + Reward.Rarity * 0.2f) * Reward.Quantity;
            break;
            
        case EHSRewardType::Skill:
            Value = Reward.Value * 2.0f; // 스킬 포인트는 높은 가치
            break;
            
        case EHSRewardType::Achievement:
            Value = 100.0f; // 업적은 고정 가치
            break;
            
        case EHSRewardType::Title:
            Value = 50.0f; // 칭호도 고정 가치
            break;
            
        default:
            Value = Reward.Value;
            break;
    }
    
    // 캐시에 저장 (mutable 변수 직접 수정)
    RewardValueCache.Add(Reward.ItemID, Value);
    
    return Value;
}

float UHSRewardsSystem::AnalyzePlayerNeed(int32 PlayerID, const FHSRewardItem& Reward) const
{
    TTuple<int32, FName> CacheKey = MakeTuple(PlayerID, Reward.ItemID);
    if (const float* CachedNeed = NeedAnalysisCache.Find(CacheKey))
    {
        return *CachedNeed;
    }
    
    float NeedScore = 0.0f;

    const AHSPlayerCharacter* PlayerCharacter = nullptr;
    const UHSInventoryComponent* InventoryComponent = nullptr;
    const UHSStatsComponent* StatsComponent = nullptr;
    const UHSLevelSystem* LevelSystem = nullptr;

    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS->GetPlayerId() == PlayerID)
                {
                    if (APawn* Pawn = PS->GetPawn())
                    {
                        PlayerCharacter = Cast<AHSPlayerCharacter>(Pawn);
                        if (PlayerCharacter)
                        {
                            InventoryComponent = PlayerCharacter->GetInventoryComponent();
                            StatsComponent = PlayerCharacter->GetStatsComponent();
                            LevelSystem = StatsComponent ? StatsComponent->GetLevelSystem() : nullptr;
                        }
                    }
                    break;
                }
            }
        }
    }

    // 기본 필요도 (보상 타입별) 계산
    switch (Reward.RewardType)
    {
        case EHSRewardType::Item:
            NeedScore = 0.5f + (Reward.Rarity * 0.1f);
            break;
            
        case EHSRewardType::Currency:
            NeedScore = 0.7f;
            break;
            
        case EHSRewardType::Experience:
            NeedScore = 0.6f;
            if (LevelSystem)
            {
                const int32 PlayerLevel = LevelSystem->GetCurrentLevel();
                const float LevelProgress = LevelSystem->GetLevelProgress();
                const float LevelFactor = FMath::Clamp(1.0f - static_cast<float>(PlayerLevel) / 60.0f, 0.3f, 1.0f);
                const float ProgressFactor = 0.5f + (1.0f - LevelProgress) * 0.5f;
                NeedScore = FMath::Clamp(LevelFactor * ProgressFactor, 0.3f, 1.0f);
            }
            break;
            
        default:
            NeedScore = 0.5f;
            break;
    }
    
    // 플레이어 기여도에 따른 조정
    const FHSPlayerContribution* Contribution = PlayerContributions.Find(PlayerID);
    if (Contribution)
    {
        const float NormalizedScore = FMath::Clamp(Contribution->TotalScore, 0.0f, 1000.0f);
        const float ContribFactor = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 1000.0f), FVector2D(1.1f, 0.6f), NormalizedScore);
        NeedScore *= ContribFactor;
    }

    if (InventoryComponent)
    {
        const FString RewardNameReference = !Reward.ItemName.IsEmpty()
            ? Reward.ItemName
            : Reward.ItemID.ToString();
        const FString NormalizedRewardName = RewardNameReference.ToLower();

        int32 OwnedQuantity = 0;
        int32 CurrencyQuantity = 0;

        const TArray<FHSInventorySlot> Slots = InventoryComponent->GetFilteredItems(EHSInventoryFilter::None);
        for (const FHSInventorySlot& Slot : Slots)
        {
            if (!Slot.IsValid() || !Slot.Item)
            {
                continue;
            }

            const FString ItemNameLower = Slot.Item->GetItemName().ToLower();
            if (!NormalizedRewardName.IsEmpty() && ItemNameLower == NormalizedRewardName)
            {
                OwnedQuantity += Slot.Quantity;
            }

            if (Reward.RewardType == EHSRewardType::Currency && Slot.Item->GetItemType() == EHSItemType::Currency)
            {
                CurrencyQuantity += Slot.Quantity;
            }
        }

        if (Reward.RewardType == EHSRewardType::Item)
        {
            if (OwnedQuantity > 0)
            {
                const float DuplicatePenalty = FMath::Clamp(1.0f / (1.0f + OwnedQuantity), 0.25f, 1.0f);
                NeedScore *= DuplicatePenalty;
            }
            else
            {
                const int32 EmptySlots = InventoryComponent->GetEmptySlotCount();
                if (EmptySlots == 0)
                {
                    NeedScore *= 0.5f;
                }
            }
        }
        else if (Reward.RewardType == EHSRewardType::Currency)
        {
            const float CurrencyModifier = CurrencyQuantity > 0
                ? FMath::Clamp(1.0f / (1.0f + CurrencyQuantity / 500.0f), 0.35f, 1.0f)
                : 1.0f;
            NeedScore *= CurrencyModifier;
        }
    }

    if (StatsComponent && Reward.RewardType == EHSRewardType::Item)
    {
        const float HealthPercent = StatsComponent->GetHealthPercent();
        if (HealthPercent < 0.4f)
        {
            NeedScore = FMath::Clamp(NeedScore + 0.15f, 0.0f, 1.0f);
        }
    }

    NeedScore = FMath::Clamp(NeedScore, 0.0f, 1.0f);
    
    // 캐시에 저장 (mutable 변수 직접 수정)
    NeedAnalysisCache.Add(CacheKey, NeedScore);
    
    return NeedScore;
}

bool UHSRewardsSystem::ChangeDistributionMethod(EHSDistributionType NewMethod)
{
    if (!bSessionActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 활성 세션이 없어 분배 방식을 변경할 수 없습니다"));
        return false;
    }
    
    EHSDistributionType OldMethod = CurrentSession.DistributionType;
    CurrentSession.DistributionType = NewMethod;
    
    // 투표 방식으로 변경하는 경우 투표 초기화
    if (NewMethod == EHSDistributionType::Vote)
    {
        ClearVotes();
    }
    
    // 델리게이트 호출
    OnDistributionMethodChanged.Broadcast(OldMethod, NewMethod);
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 분배 방식 변경됨 - %d → %d"), 
           (int32)OldMethod, (int32)NewMethod);
    
    return true;
}

// === 내부 함수 구현 ===

void UHSRewardsSystem::InitializeContributionWeights()
{
    // 기여도 타입별 가중치 설정
    ContributionWeights.Add(EHSContributionType::Damage, 1.0f);
    ContributionWeights.Add(EHSContributionType::Healing, 0.9f);
    ContributionWeights.Add(EHSContributionType::Support, 0.8f);
    ContributionWeights.Add(EHSContributionType::Tank, 0.85f);
    ContributionWeights.Add(EHSContributionType::Discovery, 0.7f);
    ContributionWeights.Add(EHSContributionType::Crafting, 0.6f);
    ContributionWeights.Add(EHSContributionType::Resource, 0.5f);
    ContributionWeights.Add(EHSContributionType::Objective, 1.2f);
    ContributionWeights.Add(EHSContributionType::Leadership, 0.9f);
    ContributionWeights.Add(EHSContributionType::Teamwork, 0.8f);
    
    UE_LOG(LogTemp, Log, TEXT("HSRewardsSystem: 기여도 가중치 초기화 완료"));
}

bool UHSRewardsSystem::IsValidPlayer(int32 PlayerID) const
{
    if (PlayerID < 0)
    {
        return false;
    }
    
    // 현재 게임에 참여 중인 플레이어인지 확인
    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS->GetPlayerId() == PlayerID)
                {
                    return true;
                }
            }
        }
    }
    
    return false;
}

FString UHSRewardsSystem::GenerateSessionID() const
{
    // GUID 기반 고유 세션 ID 생성
    FGuid NewGuid = FGuid::NewGuid();
    return FString::Printf(TEXT("RS_%s"), *NewGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
}

bool UHSRewardsSystem::ValidateDistributionResults(const TArray<FHSDistributionResult>& Results) const
{
    // 기본 검증
    if (Results.Num() == 0)
    {
        return false;
    }
    
    // 각 결과의 유효성 확인
    for (const FHSDistributionResult& Result : Results)
    {
        if (!IsValidPlayer(Result.RecipientPlayerID))
        {
            UE_LOG(LogTemp, Error, TEXT("HSRewardsSystem: 잘못된 수령자 ID: %d"), Result.RecipientPlayerID);
            return false;
        }
        
        if (Result.RewardItems.Num() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("HSRewardsSystem: 보상 아이템이 없음"));
            return false;
        }
        
        if (Result.TotalValue < 0.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("HSRewardsSystem: 음수 보상 가치: %f"), Result.TotalValue);
            return false;
        }
    }
    
    return true;
}

void UHSRewardsSystem::PerformPeriodicUpdate()
{
    // 기여도 백분율 재계산
    CalculateContributionPercentages();
    
    // 만료된 캐시 정리
    CleanupExpiredData();
    
    // 세션 타임아웃 확인
    if (bSessionActive)
    {
        FDateTime CurrentTime = FDateTime::Now();
        float SessionDuration = (CurrentTime - CurrentSession.StartTime).GetTotalMinutes();
        
        // 세션이 60분 이상 지속되면 강제 종료
        if (SessionDuration > 60.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSRewardsSystem: 세션 타임아웃으로 강제 종료 - ID: %s"), 
                   *CurrentSession.SessionID);
            EndRewardSession(CurrentSession.SessionID);
        }
    }
}

void UHSRewardsSystem::CleanupExpiredData()
{
    FDateTime CurrentTime = FDateTime::Now();
    
    // 10분 이상 된 캐시 정리
    if ((CurrentTime - LastContributionUpdate).GetTotalMinutes() > 10.0f)
    {
        ContributionCache.Empty();
        RewardValueCache.Empty();
        NeedAnalysisCache.Empty();
    }
    
    // 30분 이상 된 투표 정리
    CurrentVotes.RemoveAll([CurrentTime](const FHSRewardVote& Vote)
    {
        return (CurrentTime - Vote.VoteTime).GetTotalMinutes() > 30.0f;
    });
    
    // 분배 기록 정리 (최근 100개만 유지)
    if (DistributionHistory.Num() > 100)
    {
        int32 RemoveCount = DistributionHistory.Num() - 100;
        DistributionHistory.RemoveAt(0, RemoveCount);
    }
}
