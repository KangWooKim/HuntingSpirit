// HuntingSpirit 게임의 보상 분배 시스템
// 전리품 분배, 기여도 추적, 공정한 보상 시스템 제공
// 작성자: KangWooKim (https://github.com/KangWooKim)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "HSRewardsSystem.generated.h"

// 전방 선언
class UHSInventoryComponent;
class UHSItemInstance;

// 기여도 타입
UENUM(BlueprintType)
enum class EHSContributionType : uint8
{
    None = 0,
    Damage,              // 데미지 기여도
    Healing,             // 힐링 기여도
    Support,             // 서포트 기여도
    Tank,                // 탱킹 기여도
    Discovery,           // 발견 기여도
    Crafting,            // 제작 기여도
    Resource,            // 자원 수집 기여도
    Objective,           // 목표 완수 기여도
    Leadership,          // 리더십 기여도
    Teamwork             // 팀워크 기여도
};

// 보상 타입
UENUM(BlueprintType)
enum class EHSRewardType : uint8
{
    None = 0,
    Experience,          // 경험치
    Currency,            // 화폐
    Item,                // 아이템
    Skill,               // 스킬 포인트
    Achievement,         // 업적
    Title                // 칭호
};

// 분배 방식
UENUM(BlueprintType)
enum class EHSDistributionType : uint8
{
    Equal = 0,           // 균등 분배
    Contribution,        // 기여도 비례
    Need,                // 필요도 기반
    Random,              // 랜덤 분배
    Vote,                // 투표 기반
    Leader               // 리더 결정
};

// 플레이어 기여도 정보
USTRUCT(BlueprintType)
struct FHSPlayerContribution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerID;

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    // 기여도별 점수 (0.0 ~ 1.0)
    UPROPERTY(BlueprintReadOnly)
    TMap<EHSContributionType, float> ContributionScores;

    UPROPERTY(BlueprintReadOnly)
    float TotalScore;

    UPROPERTY(BlueprintReadOnly)
    float ContributionPercentage;

    UPROPERTY(BlueprintReadOnly)
    FDateTime LastUpdateTime;

    FHSPlayerContribution()
    {
        PlayerID = -1;
        PlayerName = TEXT("");
        TotalScore = 0.0f;
        ContributionPercentage = 0.0f;
        LastUpdateTime = FDateTime::Now();
    }
};

// 보상 아이템 정보
USTRUCT(BlueprintType)
struct FHSRewardItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ItemID;

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity;

    UPROPERTY(BlueprintReadOnly)
    EHSRewardType RewardType;

    UPROPERTY(BlueprintReadOnly)
    float Value;

    UPROPERTY(BlueprintReadOnly)
    FString ItemName;

    UPROPERTY(BlueprintReadOnly)
    int32 Rarity;

    FHSRewardItem()
    {
        ItemID = NAME_None;
        Quantity = 1;
        RewardType = EHSRewardType::Item;
        Value = 0.0f;
        ItemName = TEXT("");
        Rarity = 0;
    }
};

// 분배 결과
USTRUCT(BlueprintType)
struct FHSDistributionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 RecipientPlayerID;

    UPROPERTY(BlueprintReadOnly)
    TArray<FHSRewardItem> RewardItems;

    UPROPERTY(BlueprintReadOnly)
    float TotalValue;

    UPROPERTY(BlueprintReadOnly)
    FString Reason;

    FHSDistributionResult()
    {
        RecipientPlayerID = -1;
        TotalValue = 0.0f;
        Reason = TEXT("");
    }
};

// 투표 정보
USTRUCT(BlueprintType)
struct FHSRewardVote
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 VoterPlayerID;

    UPROPERTY(BlueprintReadOnly)
    int32 CandidatePlayerID;

    UPROPERTY(BlueprintReadOnly)
    FName ItemID;

    UPROPERTY(BlueprintReadOnly)
    FDateTime VoteTime;

    FHSRewardVote()
    {
        VoterPlayerID = -1;
        CandidatePlayerID = -1;
        ItemID = NAME_None;
        VoteTime = FDateTime::Now();
    }
};

// 보상 세션 정보
USTRUCT(BlueprintType)
struct FHSRewardSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SessionID;

    UPROPERTY(BlueprintReadOnly)
    TArray<FHSRewardItem> AvailableRewards;

    UPROPERTY(BlueprintReadOnly)
    TArray<FHSPlayerContribution> PlayerContributions;

    UPROPERTY(BlueprintReadOnly)
    EHSDistributionType DistributionType;

    UPROPERTY(BlueprintReadOnly)
    bool bDistributionComplete;

    UPROPERTY(BlueprintReadOnly)
    FDateTime StartTime;

    UPROPERTY(BlueprintReadOnly)
    FDateTime EndTime;

    FHSRewardSession()
    {
        SessionID = TEXT("");
        DistributionType = EHSDistributionType::Contribution;
        bDistributionComplete = false;
        StartTime = FDateTime::Now();
        EndTime = FDateTime::MinValue();
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContributionUpdated, int32, PlayerID, const FHSPlayerContribution&, Contribution);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRewardSessionStarted, const FHSRewardSession&, Session);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRewardDistributed, const TArray<FHSDistributionResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVoteSubmitted, int32, VoterID, int32, CandidateID, FName, ItemID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDistributionMethodChanged, EHSDistributionType, OldMethod, EHSDistributionType, NewMethod);

/**
 * 보상 분배 시스템
 * 플레이어 기여도 추적 및 공정한 보상 분배 관리
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSRewardsSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSRewardsSystem();

    // UGameInstanceSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 기여도 추적 시스템 ===

    // 기여도 추가/업데이트
    UFUNCTION(BlueprintCallable, Category = "Rewards|Contribution")
    void AddContribution(int32 PlayerID, EHSContributionType ContributionType, float Amount);

    // 플레이어 기여도 정보 가져오기
    UFUNCTION(BlueprintCallable, Category = "Rewards|Contribution")
    FHSPlayerContribution GetPlayerContribution(int32 PlayerID) const;

    // 모든 플레이어 기여도 가져오기
    UFUNCTION(BlueprintCallable, Category = "Rewards|Contribution")
    TArray<FHSPlayerContribution> GetAllContributions() const;

    // 기여도 초기화
    UFUNCTION(BlueprintCallable, Category = "Rewards|Contribution")
    void ResetContributions();

    // 기여도 계산 (백분율)
    UFUNCTION(BlueprintCallable, Category = "Rewards|Contribution")
    void CalculateContributionPercentages();

    // === 보상 세션 관리 ===

    // 보상 세션 시작
    UFUNCTION(BlueprintCallable, Category = "Rewards|Session")
    FString StartRewardSession(const TArray<FHSRewardItem>& Rewards, EHSDistributionType DistributionType);

    // 보상 세션 종료
    UFUNCTION(BlueprintCallable, Category = "Rewards|Session")
    bool EndRewardSession(const FString& SessionID);

    // 현재 활성 세션 가져오기
    UFUNCTION(BlueprintCallable, Category = "Rewards|Session")
    FHSRewardSession GetActiveSession() const;

    // 세션 유효성 확인
    UFUNCTION(BlueprintCallable, Category = "Rewards|Session")
    bool IsSessionActive() const;

    // === 보상 분배 ===

    // 보상 분배 실행
    UFUNCTION(BlueprintCallable, Category = "Rewards|Distribution")
    TArray<FHSDistributionResult> DistributeRewards(const FString& SessionID);

    // 균등 분배
    UFUNCTION(BlueprintCallable, Category = "Rewards|Distribution")
    TArray<FHSDistributionResult> DistributeEqually(const TArray<FHSRewardItem>& Rewards);

    // 기여도 비례 분배
    UFUNCTION(BlueprintCallable, Category = "Rewards|Distribution")
    TArray<FHSDistributionResult> DistributeByContribution(const TArray<FHSRewardItem>& Rewards);

    // 필요도 기반 분배
    UFUNCTION(BlueprintCallable, Category = "Rewards|Distribution")
    TArray<FHSDistributionResult> DistributeByNeed(const TArray<FHSRewardItem>& Rewards);

    // 랜덤 분배
    UFUNCTION(BlueprintCallable, Category = "Rewards|Distribution")
    TArray<FHSDistributionResult> DistributeRandomly(const TArray<FHSRewardItem>& Rewards);

    // === 투표 시스템 ===

    // 투표 제출
    UFUNCTION(BlueprintCallable, Category = "Rewards|Vote")
    bool SubmitVote(int32 VoterPlayerID, int32 CandidatePlayerID, FName ItemID);

    // 투표 결과 가져오기
    UFUNCTION(BlueprintCallable, Category = "Rewards|Vote")
    TArray<FHSRewardVote> GetVotesForItem(FName ItemID) const;

    // 투표 기반 분배
    UFUNCTION(BlueprintCallable, Category = "Rewards|Vote")
    FHSDistributionResult DistributeByVote(FName ItemID);

    // 투표 초기화
    UFUNCTION(BlueprintCallable, Category = "Rewards|Vote")
    void ClearVotes();

    // === 보상 분석 ===

    // 플레이어 보상 통계
    UFUNCTION(BlueprintCallable, Category = "Rewards|Analytics")
    float GetPlayerRewardValue(int32 PlayerID) const;

    // 세션 통계
    UFUNCTION(BlueprintCallable, Category = "Rewards|Analytics")
    float GetSessionTotalValue() const;

    // 공정성 지수 계산
    UFUNCTION(BlueprintCallable, Category = "Rewards|Analytics")
    float CalculateFairnessIndex() const;

    // === 유틸리티 ===

    // 보상 가치 계산
    UFUNCTION(BlueprintCallable, Category = "Rewards|Utility")
    float CalculateRewardValue(const FHSRewardItem& Reward) const;

    // 플레이어 필요도 분석
    UFUNCTION(BlueprintCallable, Category = "Rewards|Utility")
    float AnalyzePlayerNeed(int32 PlayerID, const FHSRewardItem& Reward) const;

    // 분배 방식 변경
    UFUNCTION(BlueprintCallable, Category = "Rewards|Utility")
    bool ChangeDistributionMethod(EHSDistributionType NewMethod);

    // === 델리게이트 ===

    UPROPERTY(BlueprintAssignable, Category = "Rewards|Events")
    FOnContributionUpdated OnContributionUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Rewards|Events")
    FOnRewardSessionStarted OnRewardSessionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Rewards|Events")
    FOnRewardDistributed OnRewardDistributed;

    UPROPERTY(BlueprintAssignable, Category = "Rewards|Events")
    FOnVoteSubmitted OnVoteSubmitted;

    UPROPERTY(BlueprintAssignable, Category = "Rewards|Events")
    FOnDistributionMethodChanged OnDistributionMethodChanged;

private:
    // === 현재 세션 데이터 ===

    UPROPERTY()
    FHSRewardSession CurrentSession;

    UPROPERTY()
    bool bSessionActive;

    // === 기여도 데이터 ===

    UPROPERTY()
    TMap<int32, FHSPlayerContribution> PlayerContributions;

    // === 투표 데이터 ===

    UPROPERTY()
    TArray<FHSRewardVote> CurrentVotes;

    // === 분배 기록 ===

    UPROPERTY()
    TArray<FHSDistributionResult> DistributionHistory;

    // === 설정 ===

    // 기여도 가중치
    UPROPERTY()
    TMap<EHSContributionType, float> ContributionWeights;

    // 분배 방식별 가중치
    UPROPERTY()
    float ContributionWeight;

    UPROPERTY()
    float NeedWeight;

    UPROPERTY()
    float RandomWeight;

    // 투표 제한 시간 (초)
    UPROPERTY()
    float VoteTimeLimit;

    // === 성능 최적화 ===

    // 기여도 계산 캐시
    mutable TMap<int32, FHSPlayerContribution> ContributionCache;
    mutable FDateTime LastContributionUpdate;

    // 보상 가치 캐시
    mutable TMap<FName, float> RewardValueCache;

    // 필요도 분석 캐시
    mutable TMap<TTuple<int32, FName>, float> NeedAnalysisCache;

    // 업데이트 타이머
    UPROPERTY()
    FTimerHandle UpdateTimer;

    // 오브젝트 풀링
    UPROPERTY()
    TArray<FHSDistributionResult> ResultPool;

    UPROPERTY()
    TArray<FHSPlayerContribution> ContributionPool;

    // === 내부 함수 ===

    // 기여도 가중치 초기화
    void InitializeContributionWeights();

    // 플레이어 유효성 확인
    bool IsValidPlayer(int32 PlayerID) const;

    // 세션 ID 생성
    FString GenerateSessionID() const;

    // 분배 결과 검증
    bool ValidateDistributionResults(const TArray<FHSDistributionResult>& Results) const;

    // 정기 업데이트 (기여도 재계산 등)
    void PerformPeriodicUpdate();

    // 메모리 정리
    void CleanupExpiredData();

    friend class UHSTeamManager;
    friend class UHSCommunicationSystem;
};
