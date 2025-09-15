// HuntingSpirit 게임의 팀 구성 시스템
// 역할 기반 팀 편성, 스킬 밸런싱, 자동 매칭 시스템 제공
// 작성자: KangWooKim (https://github.com/KangWooKim)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "HSTeamFormationSystem.generated.h"

// 전방 선언
class UHSTeamManager;
class UHSCommunicationSystem;

// 팀 내 역할 (팀 구성 전용)
UENUM(BlueprintType)
enum class EHSTeamRole : uint8
{
    None = 0,
    Tank,            // 탱커 (방어 담당)
    DPS,             // 딜러 (공격 담당)
    Support,         // 서포터 (지원 담당)
    Healer,          // 힐러 (치유 담당)
    Scout,           // 정찰병 (탐색 담당)
    Leader,          // 리더 (지휘 담당)
    Specialist,      // 전문가 (특수 역할)
    Flexible         // 다목적 (여러 역할 가능)
};

// 팀 구성 전략
UENUM(BlueprintType)
enum class EHSFormationStrategy : uint8
{
    Balanced = 0,    // 균형잡힌 구성
    Aggressive,      // 공격적 구성
    Defensive,       // 방어적 구성
    Support,         // 지원 중심 구성
    Specialized,     // 전문화 구성
    Adaptive,        // 적응형 구성
    Custom           // 사용자 정의
};

// 매칭 우선순위
UENUM(BlueprintType)
enum class EHSMatchingPriority : uint8
{
    RoleBalance = 0, // 역할 균형 우선
    SkillLevel,      // 스킬 레벨 우선
    Experience,      // 경험치 우선
    Synergy,         // 시너지 우선
    Geography,       // 지역 우선
    Language,        // 언어 우선
    Friend           // 친구 우선
};

// 플레이어 스킬 정보
USTRUCT(BlueprintType)
struct FHSPlayerSkillInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerID;

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly)
    EHSTeamRole PrimaryRole;

    UPROPERTY(BlueprintReadOnly)
    TArray<EHSTeamRole> SecondaryRoles;

    // 역할별 숙련도 (0.0 ~ 1.0)
    UPROPERTY(BlueprintReadOnly)
    TMap<EHSTeamRole, float> RoleProficiency;

    UPROPERTY(BlueprintReadOnly)
    int32 OverallLevel;

    UPROPERTY(BlueprintReadOnly)
    float CombatRating;

    UPROPERTY(BlueprintReadOnly)
    float SupportRating;

    UPROPERTY(BlueprintReadOnly)
    float LeadershipRating;

    UPROPERTY(BlueprintReadOnly)
    float TeamworkRating;

    UPROPERTY(BlueprintReadOnly)
    int32 GamesPlayed;

    UPROPERTY(BlueprintReadOnly)
    float WinRate;

    UPROPERTY(BlueprintReadOnly)
    FDateTime LastPlayed;

    FHSPlayerSkillInfo()
    {
        PlayerID = -1;
        PlayerName = TEXT("");
        PrimaryRole = EHSTeamRole::None;
        OverallLevel = 1;
        CombatRating = 0.0f;
        SupportRating = 0.0f;
        LeadershipRating = 0.0f;
        TeamworkRating = 0.0f;
        GamesPlayed = 0;
        WinRate = 0.0f;
        LastPlayed = FDateTime::Now();
    }

    // 비교 연산자 추가 (TArray::Remove에 필요)
    bool operator==(const FHSPlayerSkillInfo& Other) const
    {
        return PlayerID == Other.PlayerID;
    }

    bool operator!=(const FHSPlayerSkillInfo& Other) const
    {
        return !(*this == Other);
    }
};

// 팀 구성 요구사항
USTRUCT(BlueprintType)
struct FHSTeamRequirements
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 MinPlayers;

    UPROPERTY(BlueprintReadWrite)
    int32 MaxPlayers;

    UPROPERTY(BlueprintReadWrite)
    TMap<EHSTeamRole, int32> RequiredRoles;

    UPROPERTY(BlueprintReadWrite)
    TMap<EHSTeamRole, int32> OptionalRoles;

    UPROPERTY(BlueprintReadWrite)
    int32 MinLevel;

    UPROPERTY(BlueprintReadWrite)
    int32 MaxLevel;

    UPROPERTY(BlueprintReadWrite)
    float MinCombatRating;

    UPROPERTY(BlueprintReadWrite)
    float MinTeamworkRating;

    UPROPERTY(BlueprintReadWrite)
    EHSFormationStrategy PreferredStrategy;

    UPROPERTY(BlueprintReadWrite)
    bool bRequireLeader;

    UPROPERTY(BlueprintReadWrite)
    bool bAllowDuplicateRoles;

    FHSTeamRequirements()
    {
        MinPlayers = 2;
        MaxPlayers = 4;
        MinLevel = 1;
        MaxLevel = 100;
        MinCombatRating = 0.0f;
        MinTeamworkRating = 0.0f;
        PreferredStrategy = EHSFormationStrategy::Balanced;
        bRequireLeader = false;
        bAllowDuplicateRoles = true;
    }
};

// 팀 구성 결과
USTRUCT(BlueprintType)
struct FHSTeamComposition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString TeamID;

    UPROPERTY(BlueprintReadOnly)
    TArray<FHSPlayerSkillInfo> TeamMembers;

    UPROPERTY(BlueprintReadOnly)
    TMap<EHSTeamRole, int32> RoleDistribution;

    UPROPERTY(BlueprintReadOnly)
    float TeamSynergyScore;

    UPROPERTY(BlueprintReadOnly)
    float AverageCombatRating;

    UPROPERTY(BlueprintReadOnly)
    float AverageTeamworkRating;

    UPROPERTY(BlueprintReadOnly)
    EHSFormationStrategy Strategy;

    UPROPERTY(BlueprintReadOnly)
    int32 LeaderPlayerID;

    UPROPERTY(BlueprintReadOnly)
    float BalanceScore;

    UPROPERTY(BlueprintReadOnly)
    FDateTime CreationTime;

    UPROPERTY(BlueprintReadOnly)
    bool bIsValid;

    FHSTeamComposition()
    {
        TeamID = TEXT("");
        TeamSynergyScore = 0.0f;
        AverageCombatRating = 0.0f;
        AverageTeamworkRating = 0.0f;
        Strategy = EHSFormationStrategy::Balanced;
        LeaderPlayerID = -1;
        BalanceScore = 0.0f;
        CreationTime = FDateTime::Now();
        bIsValid = false;
    }
};

// 역할 시너지 정보
USTRUCT(BlueprintType)
struct FHSRoleSynergy
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    EHSTeamRole Role1;

    UPROPERTY(BlueprintReadWrite)
    EHSTeamRole Role2;

    UPROPERTY(BlueprintReadWrite)
    float SynergyMultiplier;

    UPROPERTY(BlueprintReadWrite)
    FString Description;

    FHSRoleSynergy()
    {
        Role1 = EHSTeamRole::None;
        Role2 = EHSTeamRole::None;
        SynergyMultiplier = 1.0f;
        Description = TEXT("");
    }
};

// 매칭 요청
USTRUCT(BlueprintType)
struct FHSMatchingRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RequestID;

    UPROPERTY(BlueprintReadOnly)
    int32 RequesterPlayerID;

    UPROPERTY(BlueprintReadOnly)
    FHSTeamRequirements Requirements;

    UPROPERTY(BlueprintReadOnly)
    EHSMatchingPriority Priority;

    UPROPERTY(BlueprintReadOnly)
    TArray<int32> PreferredTeammates;

    UPROPERTY(BlueprintReadOnly)
    TArray<int32> BlockedPlayers;

    UPROPERTY(BlueprintReadOnly)
    FDateTime RequestTime;

    UPROPERTY(BlueprintReadOnly)
    float TimeoutSeconds;

    UPROPERTY(BlueprintReadOnly)
    bool bIsActive;

    FHSMatchingRequest()
    {
        RequestID = TEXT("");
        RequesterPlayerID = -1;
        Priority = EHSMatchingPriority::RoleBalance;
        RequestTime = FDateTime::Now();
        TimeoutSeconds = 300.0f; // 5분
        bIsActive = true;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamFormed, const FHSTeamComposition&, TeamComposition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchingRequested, int32, PlayerID, const FHSMatchingRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchingCancelled, int32, PlayerID, const FString&, RequestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoleAssigned, int32, PlayerID, EHSTeamRole, OldRole, EHSTeamRole, NewRole);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamBalanceChanged, const FString&, TeamID, float, NewBalanceScore);

/**
 * 팀 구성 시스템
 * 역할 기반 팀 편성 및 스킬 밸런싱 관리
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSTeamFormationSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSTeamFormationSystem();

    // UGameInstanceSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 플레이어 스킬 관리 ===

    // 플레이어 스킬 정보 등록/업데이트
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Skills")
    void RegisterPlayerSkills(const FHSPlayerSkillInfo& SkillInfo);

    // 플레이어 스킬 정보 가져오기
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Skills")
    FHSPlayerSkillInfo GetPlayerSkills(int32 PlayerID) const;

    // 역할 숙련도 업데이트
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Skills")
    void UpdateRoleProficiency(int32 PlayerID, EHSTeamRole Role, float NewProficiency);

    // 플레이어 주 역할 변경
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Skills")
    bool ChangePlayerPrimaryRole(int32 PlayerID, EHSTeamRole NewRole);

    // 보조 역할 추가/제거
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Skills")
    bool AddSecondaryRole(int32 PlayerID, EHSTeamRole Role);

    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Skills")
    bool RemoveSecondaryRole(int32 PlayerID, EHSTeamRole Role);

    // === 팀 구성 ===

    // 수동 팀 구성
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Composition")
    FHSTeamComposition CreateTeamManual(const TArray<int32>& PlayerIDs, EHSFormationStrategy Strategy);

    // 자동 팀 구성 (요구사항 기반)
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Composition")
    FHSTeamComposition CreateTeamAutomatic(const FHSTeamRequirements& Requirements);

    // 팀 최적화
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Composition")
    FHSTeamComposition OptimizeTeam(const FHSTeamComposition& CurrentTeam);

    // 팀 유효성 검증
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Composition")
    bool ValidateTeamComposition(const FHSTeamComposition& Team, const FHSTeamRequirements& Requirements) const;

    // === 매칭 시스템 ===

    // 매칭 요청 제출
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Matching")
    FString RequestMatching(int32 PlayerID, const FHSTeamRequirements& Requirements, EHSMatchingPriority Priority);

    // 매칭 요청 취소
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Matching")
    bool CancelMatching(int32 PlayerID, const FString& RequestID);

    // 활성 매칭 요청 가져오기
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Matching")
    TArray<FHSMatchingRequest> GetActiveMatchingRequests() const;

    // 호환 가능한 플레이어 찾기
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Matching")
    TArray<FHSPlayerSkillInfo> FindCompatiblePlayers(const FHSTeamRequirements& Requirements) const;

    // 매칭 처리 실행
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Matching")
    void ProcessMatchingQueue();

    // === 분석 및 통계 ===

    // 팀 시너지 계산
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Analysis")
    float CalculateTeamSynergy(const TArray<FHSPlayerSkillInfo>& TeamMembers) const;

    // 팀 밸런스 스코어 계산
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Analysis")
    float CalculateTeamBalance(const FHSTeamComposition& Team) const;

    // 역할 부족도 분석
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Analysis")
    TMap<EHSTeamRole, int32> AnalyzeRoleShortage() const;

    // 플레이어 호환성 점수
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Analysis")
    float CalculatePlayerCompatibility(int32 PlayerID1, int32 PlayerID2) const;

    // === 유틸리티 ===

    // 추천 팀 구성 생성
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Utility")
    TArray<FHSTeamComposition> GenerateRecommendedTeams(const FHSTeamRequirements& Requirements, int32 MaxSuggestions = 3);

    // 역할별 추천 플레이어
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Utility")
    TArray<FHSPlayerSkillInfo> GetRecommendedPlayersForRole(EHSTeamRole Role, int32 MaxSuggestions = 5) const;

    // 팀 리더 추천
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Utility")
    int32 RecommendTeamLeader(const TArray<FHSPlayerSkillInfo>& TeamMembers) const;

    // 전략별 최적 구성 가져오기
    UFUNCTION(BlueprintCallable, Category = "TeamFormation|Utility")
    FHSTeamRequirements GetOptimalRequirementsForStrategy(EHSFormationStrategy Strategy) const;

    // === 델리게이트 ===

    UPROPERTY(BlueprintAssignable, Category = "TeamFormation|Events")
    FOnTeamFormed OnTeamFormed;

    UPROPERTY(BlueprintAssignable, Category = "TeamFormation|Events")
    FOnMatchingRequested OnMatchingRequested;

    UPROPERTY(BlueprintAssignable, Category = "TeamFormation|Events")
    FOnMatchingCancelled OnMatchingCancelled;

    UPROPERTY(BlueprintAssignable, Category = "TeamFormation|Events")
    FOnRoleAssigned OnRoleAssigned;

    UPROPERTY(BlueprintAssignable, Category = "TeamFormation|Events")
    FOnTeamBalanceChanged OnTeamBalanceChanged;

private:
    // === 플레이어 데이터 ===

    UPROPERTY()
    TMap<int32, FHSPlayerSkillInfo> PlayerSkills;

    // === 매칭 데이터 ===

    UPROPERTY()
    TArray<FHSMatchingRequest> MatchingQueue;

    UPROPERTY()
    TMap<FString, FHSTeamComposition> ActiveTeams;

    // === 시너지 데이터 ===

    UPROPERTY()
    TArray<FHSRoleSynergy> RoleSynergies;

    // === 설정 ===

    // 역할별 가중치
    UPROPERTY()
    TMap<EHSTeamRole, float> RoleWeights;

    // 매칭 허용 오차
    UPROPERTY()
    float LevelTolerancePercent;

    UPROPERTY()
    float SkillTolerancePercent;

    // 매칭 타임아웃
    UPROPERTY()
    float DefaultMatchingTimeout;

    // === 성능 최적화 ===

    // 플레이어 검색 캐시
    mutable TMap<FString, TArray<FHSPlayerSkillInfo>> PlayerSearchCache;
    mutable FDateTime LastCacheUpdate;

    // 시너지 계산 캐시
    mutable TMap<FString, float> SynergyCache;

    // 호환성 계산 캐시
    mutable TMap<TTuple<int32, int32>, float> CompatibilityCache;

    // 업데이트 타이머
    UPROPERTY()
    FTimerHandle MatchingProcessTimer;

    UPROPERTY()
    FTimerHandle CacheCleanupTimer;

    // 오브젝트 풀링
    UPROPERTY()
    TArray<FHSTeamComposition> TeamCompositionPool;

    UPROPERTY()
    TArray<FHSMatchingRequest> MatchingRequestPool;

    // === 내부 함수 ===

    // 시너지 데이터 초기화
    void InitializeRoleSynergies();

    // 역할 가중치 초기화
    void InitializeRoleWeights();

    // 매칭 요청 만료 확인
    void CheckExpiredMatchingRequests();

    // 캐시 정리
    void CleanupCache();

    // 팀 ID 생성
    FString GenerateTeamID() const;

    // 매칭 요청 ID 생성
    FString GenerateMatchingRequestID() const;

    // 팀 구성 해시 생성 (캐싱용)
    FString GenerateTeamHash(const TArray<int32>& PlayerIDs) const;

    // 최적 매칭 찾기
    TArray<FHSPlayerSkillInfo> FindOptimalMatch(const FHSMatchingRequest& Request) const;

    // 역할 분배 최적화
    TMap<int32, EHSTeamRole> OptimizeRoleAssignment(const TArray<FHSPlayerSkillInfo>& Players, const FHSTeamRequirements& Requirements) const;

    friend class UHSTeamManager;
    friend class UHSCommunicationSystem;
    friend class UHSRewardsSystem;
};
