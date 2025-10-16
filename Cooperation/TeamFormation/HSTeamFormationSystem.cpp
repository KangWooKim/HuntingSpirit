// HuntingSpirit 게임의 팀 구성 시스템 구현
// 역할 기반 팀 편성, 자동 매칭, 시너지 계산 로직
// 작성자: KangWooKim (https://github.com/KangWooKim)

#include "HSTeamFormationSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
// #include "Algorithm/ParallelFor.h" // UE5에서 제거됨, 대신 Async 사용
#include "../HSTeamManager.h"

UHSTeamFormationSystem::UHSTeamFormationSystem()
{
    // 기본 설정 초기화
    LevelTolerancePercent = 0.2f;      // 20% 레벨 허용 오차
    SkillTolerancePercent = 0.15f;     // 15% 스킬 허용 오차
    DefaultMatchingTimeout = 300.0f;   // 5분 매칭 타임아웃
    
    // 풀링 초기화
    TeamCompositionPool.Reserve(20);
    MatchingRequestPool.Reserve(50);
    
    // 캐시 초기화
    LastCacheUpdate = FDateTime::MinValue();
}

void UHSTeamFormationSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 팀 구성 시스템 초기화 시작"));
    
    // 역할 시너지 초기화
    InitializeRoleSynergies();
    
    // 역할 가중치 초기화
    InitializeRoleWeights();
    
    // 매칭 처리 타이머 설정 (2초마다)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            MatchingProcessTimer,
            this,
            &UHSTeamFormationSystem::ProcessMatchingQueue,
            2.0f,
            true
        );
        
        // 캐시 정리 타이머 설정 (30초마다)
        World->GetTimerManager().SetTimer(
            CacheCleanupTimer,
            this,
            &UHSTeamFormationSystem::CleanupCache,
            30.0f,
            true
        );
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 팀 구성 시스템 초기화 완료"));
}

void UHSTeamFormationSystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 팀 구성 시스템 정리 시작"));
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MatchingProcessTimer);
        World->GetTimerManager().ClearTimer(CacheCleanupTimer);
    }
    
    // 데이터 정리
    PlayerSkills.Empty();
    MatchingQueue.Empty();
    ActiveTeams.Empty();
    RoleSynergies.Empty();
    RoleWeights.Empty();
    
    // 캐시 정리
    PlayerSearchCache.Empty();
    SynergyCache.Empty();
    CompatibilityCache.Empty();
    
    // 풀링 정리
    TeamCompositionPool.Empty();
    MatchingRequestPool.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 팀 구성 시스템 정리 완료"));
    
    Super::Deinitialize();
}

// === 플레이어 스킬 관리 구현 ===

void UHSTeamFormationSystem::RegisterPlayerSkills(const FHSPlayerSkillInfo& SkillInfo)
{
    if (SkillInfo.PlayerID < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 잘못된 플레이어 ID: %d"), SkillInfo.PlayerID);
        return;
    }
    
    // 기존 정보가 있으면 업데이트, 없으면 새로 추가
    FHSPlayerSkillInfo& StoredInfo = PlayerSkills.FindOrAdd(SkillInfo.PlayerID);
    StoredInfo = SkillInfo;
    StoredInfo.LastPlayed = FDateTime::Now();
    
    // 캐시 무효화
    PlayerSearchCache.Empty();
    CompatibilityCache.Empty();
    LastCacheUpdate = FDateTime::Now();
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 플레이어 스킬 정보 등록됨 - ID: %d, 주역할: %d, 레벨: %d"), 
           SkillInfo.PlayerID, (int32)SkillInfo.PrimaryRole, SkillInfo.OverallLevel);
}

FHSPlayerSkillInfo UHSTeamFormationSystem::GetPlayerSkills(int32 PlayerID) const
{
    if (const FHSPlayerSkillInfo* SkillInfo = PlayerSkills.Find(PlayerID))
    {
        return *SkillInfo;
    }
    
    // 플레이어 정보가 없으면 기본값 생성 시도
    FHSPlayerSkillInfo DefaultInfo;
    DefaultInfo.PlayerID = PlayerID;
    
    // 게임에서 플레이어 이름 가져오기
    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS->GetPlayerId() == PlayerID)
                {
                    DefaultInfo.PlayerName = PS->GetPlayerName();
                    break;
                }
            }
        }
    }
    
    if (DefaultInfo.PlayerName.IsEmpty())
    {
        DefaultInfo.PlayerName = FString::Printf(TEXT("Player_%d"), PlayerID);
    }
    
    return DefaultInfo;
}

void UHSTeamFormationSystem::UpdateRoleProficiency(int32 PlayerID, EHSTeamRole Role, float NewProficiency)
{
    FHSPlayerSkillInfo* SkillInfo = PlayerSkills.Find(PlayerID);
    if (!SkillInfo)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 플레이어 스킬 정보를 찾을 수 없음 - ID: %d"), PlayerID);
        return;
    }
    
    // 숙련도 값 유효성 검사
    NewProficiency = FMath::Clamp(NewProficiency, 0.0f, 1.0f);
    
    float OldProficiency = SkillInfo->RoleProficiency.FindOrAdd(Role, 0.0f);
    SkillInfo->RoleProficiency.Add(Role, NewProficiency);
    SkillInfo->LastPlayed = FDateTime::Now();
    
    // 주 역할의 숙련도가 변경되면 전체 평점도 업데이트
    if (Role == SkillInfo->PrimaryRole)
    {
        // 전투/지원/리더십 평점 재계산
        switch (Role)
        {
            case EHSTeamRole::Tank:
            case EHSTeamRole::DPS:
                SkillInfo->CombatRating = FMath::Lerp(SkillInfo->CombatRating, NewProficiency * 100.0f, 0.3f);
                break;
                
            case EHSTeamRole::Support:
            case EHSTeamRole::Healer:
                SkillInfo->SupportRating = FMath::Lerp(SkillInfo->SupportRating, NewProficiency * 100.0f, 0.3f);
                break;
                
            case EHSTeamRole::Leader:
                SkillInfo->LeadershipRating = FMath::Lerp(SkillInfo->LeadershipRating, NewProficiency * 100.0f, 0.3f);
                break;
                
            default:
                break;
        }
    }
    
    // 캐시 무효화
    CompatibilityCache.Empty();
    SynergyCache.Empty();
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSTeamFormationSystem: 역할 숙련도 업데이트됨 - Player: %d, Role: %d, %f → %f"), 
           PlayerID, (int32)Role, OldProficiency, NewProficiency);
}

bool UHSTeamFormationSystem::ChangePlayerPrimaryRole(int32 PlayerID, EHSTeamRole NewRole)
{
    FHSPlayerSkillInfo* SkillInfo = PlayerSkills.Find(PlayerID);
    if (!SkillInfo)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 플레이어 스킬 정보를 찾을 수 없음 - ID: %d"), PlayerID);
        return false;
    }
    
    if (NewRole == EHSTeamRole::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 잘못된 역할 변경 시도 - Player: %d"), PlayerID);
        return false;
    }
    
    EHSTeamRole OldRole = SkillInfo->PrimaryRole;
    
    // 기존 주 역할을 보조 역할로 추가 (숙련도가 충분한 경우)
    if (OldRole != EHSTeamRole::None && OldRole != NewRole)
    {
        const float* OldRoleProficiency = SkillInfo->RoleProficiency.Find(OldRole);
        if (OldRoleProficiency && *OldRoleProficiency >= 0.6f) // 60% 이상 숙련도
        {
            SkillInfo->SecondaryRoles.AddUnique(OldRole);
        }
    }
    
    // 새 주 역할 설정
    SkillInfo->PrimaryRole = NewRole;
    
    // 보조 역할에서 제거 (주 역할이 되었으므로)
    SkillInfo->SecondaryRoles.Remove(NewRole);
    
    // 새 역할의 숙련도가 없으면 기본값 설정
    if (!SkillInfo->RoleProficiency.Contains(NewRole))
    {
        SkillInfo->RoleProficiency.Add(NewRole, 0.3f); // 기본 30% 숙련도
    }
    
    SkillInfo->LastPlayed = FDateTime::Now();
    
    // 캐시 무효화
    PlayerSearchCache.Empty();
    CompatibilityCache.Empty();
    SynergyCache.Empty();
    
    // 델리게이트 호출
    OnRoleAssigned.Broadcast(PlayerID, OldRole, NewRole);
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 플레이어 주 역할 변경됨 - Player: %d, %d → %d"), 
           PlayerID, (int32)OldRole, (int32)NewRole);
    
    return true;
}

bool UHSTeamFormationSystem::AddSecondaryRole(int32 PlayerID, EHSTeamRole Role)
{
    FHSPlayerSkillInfo* SkillInfo = PlayerSkills.Find(PlayerID);
    if (!SkillInfo)
    {
        return false;
    }
    
    if (Role == EHSTeamRole::None || Role == SkillInfo->PrimaryRole)
    {
        return false;
    }
    
    // 보조 역할 추가 (중복 방지)
    SkillInfo->SecondaryRoles.AddUnique(Role);
    
    // 기본 숙련도 설정 (아직 없는 경우)
    if (!SkillInfo->RoleProficiency.Contains(Role))
    {
        SkillInfo->RoleProficiency.Add(Role, 0.2f); // 기본 20% 숙련도
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 보조 역할 추가됨 - Player: %d, Role: %d"), 
           PlayerID, (int32)Role);
    
    return true;
}

bool UHSTeamFormationSystem::RemoveSecondaryRole(int32 PlayerID, EHSTeamRole Role)
{
    FHSPlayerSkillInfo* SkillInfo = PlayerSkills.Find(PlayerID);
    if (!SkillInfo)
    {
        return false;
    }
    
    int32 RemovedCount = SkillInfo->SecondaryRoles.Remove(Role);
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 보조 역할 제거됨 - Player: %d, Role: %d"), 
               PlayerID, (int32)Role);
        return true;
    }
    
    return false;
}

// === 팀 구성 구현 ===

FHSTeamComposition UHSTeamFormationSystem::CreateTeamManual(const TArray<int32>& PlayerIDs, EHSFormationStrategy Strategy)
{
    FHSTeamComposition TeamComp;
    
    if (PlayerIDs.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 팀 구성에 최소 2명의 플레이어가 필요합니다"));
        return TeamComp;
    }
    
    // 플레이어 스킬 정보 수집
    TArray<FHSPlayerSkillInfo> TeamMembers;
    for (int32 PlayerID : PlayerIDs)
    {
        FHSPlayerSkillInfo SkillInfo = GetPlayerSkills(PlayerID);
        if (SkillInfo.PlayerID != -1)
        {
            TeamMembers.Add(SkillInfo);
        }
    }
    
    if (TeamMembers.Num() != PlayerIDs.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 일부 플레이어의 스킬 정보를 찾을 수 없습니다"));
        return TeamComp;
    }
    
    // 팀 구성 정보 설정
    TeamComp.TeamID = GenerateTeamID();
    TeamComp.TeamMembers = TeamMembers;
    TeamComp.Strategy = Strategy;
    TeamComp.CreationTime = FDateTime::Now();
    
    // 역할 분배 계산
    for (const FHSPlayerSkillInfo& Member : TeamMembers)
    {
        int32& RoleCount = TeamComp.RoleDistribution.FindOrAdd(Member.PrimaryRole, 0);
        RoleCount++;
    }
    
    // 팀 통계 계산
    float TotalCombatRating = 0.0f;
    float TotalTeamworkRating = 0.0f;
    
    for (const FHSPlayerSkillInfo& Member : TeamMembers)
    {
        TotalCombatRating += Member.CombatRating;
        TotalTeamworkRating += Member.TeamworkRating;
    }
    
    TeamComp.AverageCombatRating = TotalCombatRating / TeamMembers.Num();
    TeamComp.AverageTeamworkRating = TotalTeamworkRating / TeamMembers.Num();
    
    // 시너지 점수 계산
    TeamComp.TeamSynergyScore = CalculateTeamSynergy(TeamMembers);
    
    // 밸런스 점수 계산
    TeamComp.BalanceScore = CalculateTeamBalance(TeamComp);
    
    // 리더 추천
    TeamComp.LeaderPlayerID = RecommendTeamLeader(TeamMembers);
    
    TeamComp.bIsValid = true;
    
    // 활성 팀 목록에 추가
    ActiveTeams.Add(TeamComp.TeamID, TeamComp);
    
    // 델리게이트 호출
    OnTeamFormed.Broadcast(TeamComp);
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 수동 팀 구성 완료 - ID: %s, 멤버 수: %d, 시너지: %.2f"), 
           *TeamComp.TeamID, TeamMembers.Num(), TeamComp.TeamSynergyScore);
    
    return TeamComp;
}

FHSTeamComposition UHSTeamFormationSystem::CreateTeamAutomatic(const FHSTeamRequirements& Requirements)
{
    FHSTeamComposition TeamComp;
    
    // 호환 가능한 플레이어 찾기
    TArray<FHSPlayerSkillInfo> CompatiblePlayers = FindCompatiblePlayers(Requirements);
    
    if (CompatiblePlayers.Num() < Requirements.MinPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSTeamFormationSystem: 요구사항을 만족하는 플레이어가 부족합니다 - 필요: %d, 찾음: %d"), 
               Requirements.MinPlayers, CompatiblePlayers.Num());
        return TeamComp;
    }
    
    // 최적 팀 멤버 선택
    TArray<FHSPlayerSkillInfo> OptimalTeam;
    
    // 필수 역할부터 배치
    TArray<FHSPlayerSkillInfo> RemainingPlayers = CompatiblePlayers;
    
    for (const auto& RequiredRolePair : Requirements.RequiredRoles)
    {
        EHSTeamRole Role = RequiredRolePair.Key;
        int32 RequiredCount = RequiredRolePair.Value;
        
        // 해당 역할의 가장 숙련된 플레이어들 선택
        TArray<FHSPlayerSkillInfo> RoleCandidates;
        for (const FHSPlayerSkillInfo& Player : RemainingPlayers)
        {
            if (Player.PrimaryRole == Role || Player.SecondaryRoles.Contains(Role))
            {
                RoleCandidates.Add(Player);
            }
        }
        
        // 숙련도순으로 정렬
        RoleCandidates.Sort([Role](const FHSPlayerSkillInfo& A, const FHSPlayerSkillInfo& B)
        {
            const float* ProfA = A.RoleProficiency.Find(Role);
            const float* ProfB = B.RoleProficiency.Find(Role);
            float ScoreA = ProfA ? *ProfA : 0.0f;
            float ScoreB = ProfB ? *ProfB : 0.0f;
            return ScoreA > ScoreB;
        });
        
        // 필요한 수만큼 선택
        for (int32 i = 0; i < RequiredCount && i < RoleCandidates.Num(); i++)
        {
            OptimalTeam.Add(RoleCandidates[i]);
            RemainingPlayers.Remove(RoleCandidates[i]);
        }
    }
    
    // 선택 역할 배치 (남은 자리가 있는 경우)
    while (OptimalTeam.Num() < Requirements.MaxPlayers && RemainingPlayers.Num() > 0)
    {
        FHSPlayerSkillInfo* BestPlayer = nullptr;
        float BestScore = -1.0f;
        
        for (FHSPlayerSkillInfo& Player : RemainingPlayers)
        {
            // 팀 시너지를 고려한 점수 계산
            TArray<FHSPlayerSkillInfo> TestTeam = OptimalTeam;
            TestTeam.Add(Player);
            
            float SynergyScore = CalculateTeamSynergy(TestTeam);
            
            if (SynergyScore > BestScore)
            {
                BestScore = SynergyScore;
                BestPlayer = &Player;
            }
        }
        
        if (BestPlayer)
        {
            OptimalTeam.Add(*BestPlayer);
            RemainingPlayers.Remove(*BestPlayer);
        }
        else
        {
            break;
        }
    }
    
    // 팀 구성 완료
    if (OptimalTeam.Num() >= Requirements.MinPlayers)
    {
        TArray<int32> PlayerIDs;
        for (const FHSPlayerSkillInfo& Player : OptimalTeam)
        {
            PlayerIDs.Add(Player.PlayerID);
        }
        
        TeamComp = CreateTeamManual(PlayerIDs, Requirements.PreferredStrategy);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 자동 팀 구성 완료 - 멤버 수: %d"), OptimalTeam.Num());
    
    return TeamComp;
}

FHSTeamComposition UHSTeamFormationSystem::OptimizeTeam(const FHSTeamComposition& CurrentTeam)
{
    if (!CurrentTeam.bIsValid || CurrentTeam.TeamMembers.Num() < 2)
    {
        return CurrentTeam;
    }
    
    FHSTeamComposition OptimizedTeam = CurrentTeam;
    
    // 역할 재분배 최적화
    TArray<int32> PlayerIDs;
    for (const FHSPlayerSkillInfo& Member : CurrentTeam.TeamMembers)
    {
        PlayerIDs.Add(Member.PlayerID);
    }
    
    // 현재 팀 요구사항 생성
    FHSTeamRequirements Requirements;
    Requirements.MinPlayers = CurrentTeam.TeamMembers.Num();
    Requirements.MaxPlayers = CurrentTeam.TeamMembers.Num();
    Requirements.PreferredStrategy = CurrentTeam.Strategy;
    
    // 더 나은 역할 분배 찾기
    TMap<int32, EHSTeamRole> OptimalRoles = OptimizeRoleAssignment(CurrentTeam.TeamMembers, Requirements);
    
    // 역할 변경 적용
    bool bRolesChanged = false;
    for (const auto& RoleAssignment : OptimalRoles)
    {
        int32 PlayerID = RoleAssignment.Key;
        EHSTeamRole NewRole = RoleAssignment.Value;
        
        // 기존 역할과 다른 경우에만 변경
        for (int32 i = 0; i < OptimizedTeam.TeamMembers.Num(); i++)
        {
            if (OptimizedTeam.TeamMembers[i].PlayerID == PlayerID)
            {
                if (OptimizedTeam.TeamMembers[i].PrimaryRole != NewRole)
                {
                    OptimizedTeam.TeamMembers[i].PrimaryRole = NewRole;
                    bRolesChanged = true;
                }
                break;
            }
        }
    }
    
    if (bRolesChanged)
    {
        // 팀 통계 재계산
        OptimizedTeam.TeamSynergyScore = CalculateTeamSynergy(OptimizedTeam.TeamMembers);
        OptimizedTeam.BalanceScore = CalculateTeamBalance(OptimizedTeam);
        
        UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 팀 최적화 완료 - 시너지: %.2f → %.2f"), 
               CurrentTeam.TeamSynergyScore, OptimizedTeam.TeamSynergyScore);
    }
    
    return OptimizedTeam;
}

bool UHSTeamFormationSystem::ValidateTeamComposition(const FHSTeamComposition& Team, const FHSTeamRequirements& Requirements) const
{
    // 기본 유효성 검사
    if (!Team.bIsValid || Team.TeamMembers.Num() < Requirements.MinPlayers || Team.TeamMembers.Num() > Requirements.MaxPlayers)
    {
        return false;
    }
    
    // 역할 요구사항 검증
    TMap<EHSTeamRole, int32> TeamRoles;
    for (const FHSPlayerSkillInfo& Member : Team.TeamMembers)
    {
        int32& Count = TeamRoles.FindOrAdd(Member.PrimaryRole, 0);
        Count++;
    }
    
    // 필수 역할 확인
    for (const auto& RequiredRolePair : Requirements.RequiredRoles)
    {
        EHSTeamRole Role = RequiredRolePair.Key;
        int32 RequiredCount = RequiredRolePair.Value;
        
        const int32* TeamRoleCount = TeamRoles.Find(Role);
        if (!TeamRoleCount || *TeamRoleCount < RequiredCount)
        {
            return false;
        }
    }
    
    // 레벨 범위 확인
    for (const FHSPlayerSkillInfo& Member : Team.TeamMembers)
    {
        if (Member.OverallLevel < Requirements.MinLevel || Member.OverallLevel > Requirements.MaxLevel)
        {
            return false;
        }
    }
    
    // 최소 평점 확인
    if (Team.AverageCombatRating < Requirements.MinCombatRating || 
        Team.AverageTeamworkRating < Requirements.MinTeamworkRating)
    {
        return false;
    }
    
    // 리더 요구사항 확인
    if (Requirements.bRequireLeader)
    {
        bool bHasLeader = false;
        for (const FHSPlayerSkillInfo& Member : Team.TeamMembers)
        {
            if (Member.PrimaryRole == EHSTeamRole::Leader || Member.LeadershipRating >= 70.0f)
            {
                bHasLeader = true;
                break;
            }
        }
        
        if (!bHasLeader)
        {
            return false;
        }
    }
    
    return true;
}

// === 매칭 시스템 구현 ===

FString UHSTeamFormationSystem::RequestMatching(int32 PlayerID, const FHSTeamRequirements& Requirements, EHSMatchingPriority Priority)
{
    // 기존 매칭 요청 확인 및 취소
    for (int32 i = MatchingQueue.Num() - 1; i >= 0; i--)
    {
        if (MatchingQueue[i].RequesterPlayerID == PlayerID)
        {
            MatchingQueue.RemoveAt(i);
        }
    }
    
    // 새 매칭 요청 생성
    FHSMatchingRequest NewRequest;
    NewRequest.RequestID = GenerateMatchingRequestID();
    NewRequest.RequesterPlayerID = PlayerID;
    NewRequest.Requirements = Requirements;
    NewRequest.Priority = Priority;
    NewRequest.RequestTime = FDateTime::Now();
    NewRequest.TimeoutSeconds = DefaultMatchingTimeout;
    NewRequest.bIsActive = true;
    
    MatchingQueue.Add(NewRequest);
    
    // 델리게이트 호출
    OnMatchingRequested.Broadcast(PlayerID, NewRequest);
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 매칭 요청 제출됨 - Player: %d, ID: %s"), 
           PlayerID, *NewRequest.RequestID);
    
    return NewRequest.RequestID;
}

bool UHSTeamFormationSystem::CancelMatching(int32 PlayerID, const FString& RequestID)
{
    for (int32 i = 0; i < MatchingQueue.Num(); i++)
    {
        FHSMatchingRequest& Request = MatchingQueue[i];
        if (Request.RequesterPlayerID == PlayerID && Request.RequestID == RequestID)
        {
            Request.bIsActive = false;
            MatchingQueue.RemoveAt(i);
            
            // 델리게이트 호출
            OnMatchingCancelled.Broadcast(PlayerID, RequestID);
            
            UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 매칭 요청 취소됨 - Player: %d, ID: %s"), 
                   PlayerID, *RequestID);
            
            return true;
        }
    }
    
    return false;
}

TArray<FHSMatchingRequest> UHSTeamFormationSystem::GetActiveMatchingRequests() const
{
    TArray<FHSMatchingRequest> ActiveRequests;
    
    for (const FHSMatchingRequest& Request : MatchingQueue)
    {
        if (Request.bIsActive)
        {
            ActiveRequests.Add(Request);
        }
    }
    
    return ActiveRequests;
}

TArray<FHSPlayerSkillInfo> UHSTeamFormationSystem::FindCompatiblePlayers(const FHSTeamRequirements& Requirements) const
{
    // 캐시 확인
    FString CacheKey = FString::Printf(TEXT("Compatible_%d_%d_%d"), 
                                      Requirements.MinLevel, Requirements.MaxLevel, (int32)Requirements.PreferredStrategy);
    
    if (const TArray<FHSPlayerSkillInfo>* CachedResult = PlayerSearchCache.Find(CacheKey))
    {
        FDateTime CurrentTime = FDateTime::Now();
        if ((CurrentTime - LastCacheUpdate).GetTotalSeconds() < 30.0f)
        {
            return *CachedResult;
        }
    }
    
    TArray<FHSPlayerSkillInfo> CompatiblePlayers;
    
    for (const auto& PlayerPair : PlayerSkills)
    {
        const FHSPlayerSkillInfo& Player = PlayerPair.Value;
        
        // 레벨 범위 확인
        if (Player.OverallLevel < Requirements.MinLevel || Player.OverallLevel > Requirements.MaxLevel)
        {
            continue;
        }
        
        // 최소 평점 확인
        if (Player.CombatRating < Requirements.MinCombatRating || 
            Player.TeamworkRating < Requirements.MinTeamworkRating)
        {
            continue;
        }
        
        // 필수 역할 확인 (하나라도 만족하면 OK)
        bool bMeetsRoleRequirement = Requirements.RequiredRoles.Num() == 0;
        for (const auto& RequiredRolePair : Requirements.RequiredRoles)
        {
            EHSTeamRole RequiredRole = RequiredRolePair.Key;
            if (Player.PrimaryRole == RequiredRole || Player.SecondaryRoles.Contains(RequiredRole))
            {
                bMeetsRoleRequirement = true;
                break;
            }
        }
        
        if (bMeetsRoleRequirement)
        {
            CompatiblePlayers.Add(Player);
        }
    }
    
    // 캐시에 저장
    PlayerSearchCache.Add(CacheKey, CompatiblePlayers);
    LastCacheUpdate = FDateTime::Now();
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSTeamFormationSystem: 호환 플레이어 검색 완료 - 총: %d, 호환: %d"), 
           PlayerSkills.Num(), CompatiblePlayers.Num());
    
    return CompatiblePlayers;
}

void UHSTeamFormationSystem::ProcessMatchingQueue()
{
    if (MatchingQueue.Num() == 0)
    {
        return;
    }
    
    // 만료된 요청 제거
    CheckExpiredMatchingRequests();
    
    // 매칭 처리 (우선순위별로 정렬)
    MatchingQueue.Sort([](const FHSMatchingRequest& A, const FHSMatchingRequest& B)
    {
        // 요청 시간이 오래된 것부터 처리
        return A.RequestTime < B.RequestTime;
    });
    
    // 각 매칭 요청 처리
    TArray<int32> ProcessedRequests;
    
    for (int32 i = 0; i < MatchingQueue.Num(); i++)
    {
        const FHSMatchingRequest& Request = MatchingQueue[i];
        
        if (!Request.bIsActive)
        {
            continue;
        }
        
        // 최적 매칭 찾기
        TArray<FHSPlayerSkillInfo> OptimalMatch = FindOptimalMatch(Request);
        
        if (OptimalMatch.Num() >= Request.Requirements.MinPlayers)
        {
            // 팀 구성 생성
            TArray<int32> PlayerIDs;
            for (const FHSPlayerSkillInfo& Player : OptimalMatch)
            {
                PlayerIDs.Add(Player.PlayerID);
            }
            
            FHSTeamComposition NewTeam = CreateTeamManual(PlayerIDs, Request.Requirements.PreferredStrategy);
            
            if (NewTeam.bIsValid)
            {
                // 매칭된 플레이어들의 다른 요청들도 제거
                for (int32 PlayerID : PlayerIDs)
                {
                    for (int32 j = MatchingQueue.Num() - 1; j >= 0; j--)
                    {
                        if (MatchingQueue[j].RequesterPlayerID == PlayerID)
                        {
                            ProcessedRequests.Add(j);
                        }
                    }
                }
                
                UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 매칭 성공 - 요청 ID: %s, 팀 ID: %s"), 
                       *Request.RequestID, *NewTeam.TeamID);
            }
        }
    }
    
    // 처리된 요청들 제거 (역순으로)
    ProcessedRequests.Sort([](const int32& A, const int32& B) { return A > B; });
    for (int32 Index : ProcessedRequests)
    {
        if (Index < MatchingQueue.Num())
        {
            MatchingQueue.RemoveAt(Index);
        }
    }
}

// === 분석 및 통계 구현 ===

float UHSTeamFormationSystem::CalculateTeamSynergy(const TArray<FHSPlayerSkillInfo>& TeamMembers) const
{
    if (TeamMembers.Num() < 2)
    {
        return 0.0f;
    }
    
    // 캐시 확인
    // 테팀 시너지 계산을 위한 해시 생성 (PlayerID 배열로 변환)
    TArray<int32> PlayerIDs;
    for (const FHSPlayerSkillInfo& Member : TeamMembers)
    {
        PlayerIDs.Add(Member.PlayerID);
    }
    FString TeamHash = GenerateTeamHash(PlayerIDs);
    if (const float* CachedSynergy = SynergyCache.Find(TeamHash))
    {
        return *CachedSynergy;
    }
    
    float TotalSynergy = 0.0f;
    int32 PairCount = 0;
    
    // 모든 플레이어 쌍의 시너지 계산
    for (int32 i = 0; i < TeamMembers.Num(); i++)
    {
        for (int32 j = i + 1; j < TeamMembers.Num(); j++)
        {
            const FHSPlayerSkillInfo& Player1 = TeamMembers[i];
            const FHSPlayerSkillInfo& Player2 = TeamMembers[j];
            
            // 역할 시너지 확인
            float RoleSynergy = 1.0f;
            for (const FHSRoleSynergy& Synergy : RoleSynergies)
            {
                if ((Synergy.Role1 == Player1.PrimaryRole && Synergy.Role2 == Player2.PrimaryRole) ||
                    (Synergy.Role1 == Player2.PrimaryRole && Synergy.Role2 == Player1.PrimaryRole))
                {
                    RoleSynergy = Synergy.SynergyMultiplier;
                    break;
                }
            }
            
            // 스킬 레벨 호환성
            float LevelDiff = FMath::Abs(Player1.OverallLevel - Player2.OverallLevel);
            float LevelCompatibility = FMath::Max(0.0f, 1.0f - (LevelDiff / 20.0f)); // 레벨 차이 20 이상에서 0
            
            // 팀워크 점수 평균
            float TeamworkAverage = (Player1.TeamworkRating + Player2.TeamworkRating) / 200.0f; // 0-1 범위로 정규화
            
            // 최종 시너지 계산
            float PairSynergy = RoleSynergy * LevelCompatibility * TeamworkAverage;
            TotalSynergy += PairSynergy;
            PairCount++;
        }
    }
    
    float AverageSynergy = PairCount > 0 ? TotalSynergy / PairCount : 0.0f;
    
    // 캐시에 저장
    SynergyCache.Add(TeamHash, AverageSynergy);
    
    return AverageSynergy;
}

float UHSTeamFormationSystem::CalculateTeamBalance(const FHSTeamComposition& Team) const
{
    if (Team.TeamMembers.Num() < 2)
    {
        return 0.0f;
    }
    
    float BalanceScore = 0.0f;
    
    // 역할 분배 균형 (40%)
    float RoleBalance = 0.0f;
    int32 UniqueRoles = Team.RoleDistribution.Num();
    int32 TotalMembers = Team.TeamMembers.Num();
    
    if (UniqueRoles > 0)
    {
        // 역할 다양성 점수
        float RoleDiversity = FMath::Min(1.0f, float(UniqueRoles) / 4.0f); // 최대 4개 역할까지
        
        // 역할 분배 균등성 점수
        float RoleEquality = 1.0f;
        for (const auto& RolePair : Team.RoleDistribution)
        {
            float RoleRatio = float(RolePair.Value) / float(TotalMembers);
            float IdealRatio = 1.0f / float(UniqueRoles);
            float Deviation = FMath::Abs(RoleRatio - IdealRatio);
            RoleEquality -= Deviation;
        }
        RoleEquality = FMath::Max(0.0f, RoleEquality);
        
        RoleBalance = (RoleDiversity + RoleEquality) / 2.0f;
    }
    
    // 스킬 레벨 균형 (30%)
    float LevelBalance = 0.0f;
    if (TotalMembers > 1)
    {
        TArray<int32> Levels;
        for (const FHSPlayerSkillInfo& Member : Team.TeamMembers)
        {
            Levels.Add(Member.OverallLevel);
        }
        
        // 표준편차 계산
        int32 MinLevel = FMath::Min<int32>(Levels);
        int32 MaxLevel = FMath::Max<int32>(Levels);
        
        if (MaxLevel > MinLevel)
        {
            float LevelSpread = float(MaxLevel - MinLevel);
            LevelBalance = FMath::Max(0.0f, 1.0f - (LevelSpread / 50.0f)); // 레벨 차이 50 이상에서 0
        }
        else
        {
            LevelBalance = 1.0f; // 모든 플레이어가 같은 레벨
        }
    }
    
    // 평점 균형 (30%)
    float RatingBalance = 0.0f;
    if (TotalMembers > 1)
    {
        TArray<float> CombatRatings;
        TArray<float> TeamworkRatings;
        
        for (const FHSPlayerSkillInfo& Member : Team.TeamMembers)
        {
            CombatRatings.Add(Member.CombatRating);
            TeamworkRatings.Add(Member.TeamworkRating);
        }
        
        // 전투 평점 균형
        float CombatMin = FMath::Min<float>(CombatRatings);
        float CombatMax = FMath::Max<float>(CombatRatings);
        float CombatBalance = CombatMax > CombatMin ? 
            FMath::Max(0.0f, 1.0f - ((CombatMax - CombatMin) / 100.0f)) : 1.0f;
        
        // 팀워크 평점 균형
        float TeamworkMin = FMath::Min<float>(TeamworkRatings);
        float TeamworkMax = FMath::Max<float>(TeamworkRatings);
        float TeamworkBalance = TeamworkMax > TeamworkMin ? 
            FMath::Max(0.0f, 1.0f - ((TeamworkMax - TeamworkMin) / 100.0f)) : 1.0f;
        
        RatingBalance = (CombatBalance + TeamworkBalance) / 2.0f;
    }
    
    // 최종 밸런스 점수 계산
    BalanceScore = (RoleBalance * 0.4f) + (LevelBalance * 0.3f) + (RatingBalance * 0.3f);
    
    return FMath::Clamp(BalanceScore, 0.0f, 1.0f);
}

TMap<EHSTeamRole, int32> UHSTeamFormationSystem::AnalyzeRoleShortage() const
{
    TMap<EHSTeamRole, int32> RoleCount;
    TMap<EHSTeamRole, int32> RoleShortage;
    
    // 현재 플레이어들의 역할 분포 계산
    for (const auto& PlayerPair : PlayerSkills)
    {
        const FHSPlayerSkillInfo& Player = PlayerPair.Value;
        int32& Count = RoleCount.FindOrAdd(Player.PrimaryRole, 0);
        Count++;
    }
    
    // 이상적인 역할 분포 (균등 분배 기준)
    int32 TotalPlayers = PlayerSkills.Num();
    int32 IdealCountPerRole = TotalPlayers / 8; // 8개 주요 역할
    
    // 부족한 역할 계산
    TArray<EHSTeamRole> AllRoles = {
        EHSTeamRole::Tank, EHSTeamRole::DPS, EHSTeamRole::Support, EHSTeamRole::Healer,
        EHSTeamRole::Scout, EHSTeamRole::Leader, EHSTeamRole::Specialist, EHSTeamRole::Flexible
    };
    
    for (EHSTeamRole Role : AllRoles)
    {
        int32 CurrentCount = RoleCount.FindOrAdd(Role, 0);
        int32 Shortage = FMath::Max(0, IdealCountPerRole - CurrentCount);
        
        if (Shortage > 0)
        {
            RoleShortage.Add(Role, Shortage);
        }
    }
    
    return RoleShortage;
}

float UHSTeamFormationSystem::CalculatePlayerCompatibility(int32 PlayerID1, int32 PlayerID2) const
{
    // 캐시 확인
    TTuple<int32, int32> CacheKey = MakeTuple(FMath::Min(PlayerID1, PlayerID2), FMath::Max(PlayerID1, PlayerID2));
    if (const float* CachedCompatibility = CompatibilityCache.Find(CacheKey))
    {
        return *CachedCompatibility;
    }
    
    const FHSPlayerSkillInfo* Player1 = PlayerSkills.Find(PlayerID1);
    const FHSPlayerSkillInfo* Player2 = PlayerSkills.Find(PlayerID2);
    
    if (!Player1 || !Player2)
    {
        return 0.0f;
    }
    
    float Compatibility = 0.0f;
    
    // 레벨 호환성 (30%)
    float LevelDiff = FMath::Abs(Player1->OverallLevel - Player2->OverallLevel);
    float LevelCompatibility = FMath::Max(0.0f, 1.0f - (LevelDiff / 20.0f));
    
    // 스킬 호환성 (40%)
    float SkillCompatibility = 0.0f;
    float CombatDiff = FMath::Abs(Player1->CombatRating - Player2->CombatRating);
    float TeamworkDiff = FMath::Abs(Player1->TeamworkRating - Player2->TeamworkRating);
    
    SkillCompatibility = (FMath::Max(0.0f, 1.0f - (CombatDiff / 100.0f)) + 
                         FMath::Max(0.0f, 1.0f - (TeamworkDiff / 100.0f))) / 2.0f;
    
    // 역할 상성 (30%)
    float RoleCompatibility = 1.0f;
    for (const FHSRoleSynergy& Synergy : RoleSynergies)
    {
        if ((Synergy.Role1 == Player1->PrimaryRole && Synergy.Role2 == Player2->PrimaryRole) ||
            (Synergy.Role1 == Player2->PrimaryRole && Synergy.Role2 == Player1->PrimaryRole))
        {
            RoleCompatibility = Synergy.SynergyMultiplier;
            break;
        }
    }
    
    Compatibility = (LevelCompatibility * 0.3f) + (SkillCompatibility * 0.4f) + (RoleCompatibility * 0.3f);
    
    // 캐시에 저장
    CompatibilityCache.Add(CacheKey, Compatibility);
    
    return Compatibility;
}

// === 유틸리티 구현 ===

TArray<FHSTeamComposition> UHSTeamFormationSystem::GenerateRecommendedTeams(const FHSTeamRequirements& Requirements, int32 MaxSuggestions)
{
    TArray<FHSTeamComposition> RecommendedTeams;
    
    // 호환 가능한 플레이어 찾기
    TArray<FHSPlayerSkillInfo> CompatiblePlayers = FindCompatiblePlayers(Requirements);
    
    if (CompatiblePlayers.Num() < Requirements.MinPlayers)
    {
        return RecommendedTeams;
    }
    
    // 여러 가지 전략으로 팀 구성 시도
    TArray<EHSFormationStrategy> Strategies = {
        EHSFormationStrategy::Balanced,
        EHSFormationStrategy::Aggressive,
        EHSFormationStrategy::Defensive,
        EHSFormationStrategy::Support
    };
    
    for (EHSFormationStrategy Strategy : Strategies)
    {
        if (RecommendedTeams.Num() >= MaxSuggestions)
        {
            break;
        }
        
        FHSTeamRequirements ModifiedRequirements = Requirements;
        ModifiedRequirements.PreferredStrategy = Strategy;
        
        FHSTeamComposition Team = CreateTeamAutomatic(ModifiedRequirements);
        
        if (Team.bIsValid)
        {
            // 중복 팀 확인 (같은 멤버 구성)
            bool bIsDuplicate = false;
            for (const FHSTeamComposition& ExistingTeam : RecommendedTeams)
            {
                if (ExistingTeam.TeamMembers.Num() == Team.TeamMembers.Num())
                {
                    bool bSameMembers = true;
                    for (const FHSPlayerSkillInfo& Member : Team.TeamMembers)
                    {
                        bool bFound = false;
                        for (const FHSPlayerSkillInfo& ExistingMember : ExistingTeam.TeamMembers)
                        {
                            if (Member.PlayerID == ExistingMember.PlayerID)
                            {
                                bFound = true;
                                break;
                            }
                        }
                        if (!bFound)
                        {
                            bSameMembers = false;
                            break;
                        }
                    }
                    
                    if (bSameMembers)
                    {
                        bIsDuplicate = true;
                        break;
                    }
                }
            }
            
            if (!bIsDuplicate)
            {
                RecommendedTeams.Add(Team);
            }
        }
    }
    
    // 시너지 점수순으로 정렬
    RecommendedTeams.Sort([](const FHSTeamComposition& A, const FHSTeamComposition& B)
    {
        return A.TeamSynergyScore > B.TeamSynergyScore;
    });
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 추천 팀 생성 완료 - %d개"), RecommendedTeams.Num());
    
    return RecommendedTeams;
}

TArray<FHSPlayerSkillInfo> UHSTeamFormationSystem::GetRecommendedPlayersForRole(EHSTeamRole Role, int32 MaxSuggestions) const
{
    TArray<FHSPlayerSkillInfo> RolePlayers;
    
    // 해당 역할의 플레이어들 수집
    for (const auto& PlayerPair : PlayerSkills)
    {
        const FHSPlayerSkillInfo& Player = PlayerPair.Value;
        
        if (Player.PrimaryRole == Role || Player.SecondaryRoles.Contains(Role))
        {
            RolePlayers.Add(Player);
        }
    }
    
    // 역할 숙련도와 전체 평점으로 정렬
    RolePlayers.Sort([Role](const FHSPlayerSkillInfo& A, const FHSPlayerSkillInfo& B)
    {
        const float* ProfA = A.RoleProficiency.Find(Role);
        const float* ProfB = B.RoleProficiency.Find(Role);
        
        float ScoreA = (ProfA ? *ProfA : 0.0f) * 0.7f + (A.CombatRating + A.TeamworkRating) / 200.0f * 0.3f;
        float ScoreB = (ProfB ? *ProfB : 0.0f) * 0.7f + (B.CombatRating + B.TeamworkRating) / 200.0f * 0.3f;
        
        return ScoreA > ScoreB;
    });
    
    // 최대 추천 수만큼 반환
    if (RolePlayers.Num() > MaxSuggestions)
    {
        RolePlayers.SetNum(MaxSuggestions);
    }
    
    return RolePlayers;
}

int32 UHSTeamFormationSystem::RecommendTeamLeader(const TArray<FHSPlayerSkillInfo>& TeamMembers) const
{
    if (TeamMembers.Num() == 0)
    {
        return -1;
    }
    
    int32 BestLeaderID = -1;
    float BestLeadershipScore = -1.0f;
    
    for (const FHSPlayerSkillInfo& Member : TeamMembers)
    {
        float LeadershipScore = 0.0f;
        
        // 리더십 평점 (40%)
        LeadershipScore += Member.LeadershipRating * 0.4f;
        
        // 팀워크 평점 (30%)
        LeadershipScore += Member.TeamworkRating * 0.3f;
        
        // 경험 (20%)
        float ExperienceScore = FMath::Min(100.0f, Member.GamesPlayed * 2.0f); // 최대 50게임까지 고려
        LeadershipScore += ExperienceScore * 0.2f;
        
        // 승률 (10%)
        LeadershipScore += Member.WinRate * 100.0f * 0.1f;
        
        // 리더 역할 보너스
        if (Member.PrimaryRole == EHSTeamRole::Leader)
        {
            LeadershipScore *= 1.2f;
        }
        
        if (LeadershipScore > BestLeadershipScore)
        {
            BestLeadershipScore = LeadershipScore;
            BestLeaderID = Member.PlayerID;
        }
    }
    
    return BestLeaderID;
}

FHSTeamRequirements UHSTeamFormationSystem::GetOptimalRequirementsForStrategy(EHSFormationStrategy Strategy) const
{
    FHSTeamRequirements Requirements;
    
    Requirements.MinPlayers = 3;
    Requirements.MaxPlayers = 4;
    Requirements.PreferredStrategy = Strategy;
    
    switch (Strategy)
    {
        case EHSFormationStrategy::Balanced:
            Requirements.RequiredRoles.Add(EHSTeamRole::Tank, 1);
            Requirements.RequiredRoles.Add(EHSTeamRole::DPS, 2);
            Requirements.RequiredRoles.Add(EHSTeamRole::Support, 1);
            Requirements.MinCombatRating = 50.0f;
            Requirements.MinTeamworkRating = 60.0f;
            break;
            
        case EHSFormationStrategy::Aggressive:
            Requirements.RequiredRoles.Add(EHSTeamRole::DPS, 3);
            Requirements.OptionalRoles.Add(EHSTeamRole::Tank, 1);
            Requirements.MinCombatRating = 70.0f;
            Requirements.MinTeamworkRating = 40.0f;
            break;
            
        case EHSFormationStrategy::Defensive:
            Requirements.RequiredRoles.Add(EHSTeamRole::Tank, 2);
            Requirements.RequiredRoles.Add(EHSTeamRole::Support, 1);
            Requirements.RequiredRoles.Add(EHSTeamRole::Healer, 1);
            Requirements.MinCombatRating = 40.0f;
            Requirements.MinTeamworkRating = 70.0f;
            break;
            
        case EHSFormationStrategy::Support:
            Requirements.RequiredRoles.Add(EHSTeamRole::Support, 2);
            Requirements.RequiredRoles.Add(EHSTeamRole::Healer, 1);
            Requirements.OptionalRoles.Add(EHSTeamRole::DPS, 1);
            Requirements.MinCombatRating = 30.0f;
            Requirements.MinTeamworkRating = 80.0f;
            break;
            
        case EHSFormationStrategy::Specialized:
            Requirements.RequiredRoles.Add(EHSTeamRole::Specialist, 2);
            Requirements.OptionalRoles.Add(EHSTeamRole::Flexible, 2);
            Requirements.MinCombatRating = 60.0f;
            Requirements.MinTeamworkRating = 50.0f;
            break;
            
        default:
            // Adaptive/Custom은 기본 균형 설정 사용
            Requirements = GetOptimalRequirementsForStrategy(EHSFormationStrategy::Balanced);
            break;
    }
    
    return Requirements;
}

// === 내부 함수 구현 ===

void UHSTeamFormationSystem::InitializeRoleSynergies()
{
    RoleSynergies.Empty();
    
    // 탱커-DPS 시너지
    FHSRoleSynergy TankDPS;
    TankDPS.Role1 = EHSTeamRole::Tank;
    TankDPS.Role2 = EHSTeamRole::DPS;
    TankDPS.SynergyMultiplier = 1.3f;
    TankDPS.Description = TEXT("탱커가 어그로를 끌어 DPS가 안전하게 공격");
    RoleSynergies.Add(TankDPS);
    
    // 힐러-탱커 시너지
    FHSRoleSynergy HealerTank;
    HealerTank.Role1 = EHSTeamRole::Healer;
    HealerTank.Role2 = EHSTeamRole::Tank;
    HealerTank.SynergyMultiplier = 1.4f;
    HealerTank.Description = TEXT("힐러가 탱커를 지원하여 생존력 증가");
    RoleSynergies.Add(HealerTank);
    
    // 서포터-DPS 시너지
    FHSRoleSynergy SupportDPS;
    SupportDPS.Role1 = EHSTeamRole::Support;
    SupportDPS.Role2 = EHSTeamRole::DPS;
    SupportDPS.SynergyMultiplier = 1.2f;
    SupportDPS.Description = TEXT("서포터 버프로 DPS 성능 향상");
    RoleSynergies.Add(SupportDPS);
    
    // 정찰병-팀 시너지
    FHSRoleSynergy ScoutDPS;
    ScoutDPS.Role1 = EHSTeamRole::Scout;
    ScoutDPS.Role2 = EHSTeamRole::DPS;
    ScoutDPS.SynergyMultiplier = 1.15f;
    ScoutDPS.Description = TEXT("정찰병 정보로 전술적 우위");
    RoleSynergies.Add(ScoutDPS);
    
    FHSRoleSynergy ScoutTank;
    ScoutTank.Role1 = EHSTeamRole::Scout;
    ScoutTank.Role2 = EHSTeamRole::Tank;
    ScoutTank.SynergyMultiplier = 1.1f;
    ScoutTank.Description = TEXT("정찰 정보로 방어 전략 최적화");
    RoleSynergies.Add(ScoutTank);
    
    // 리더-전체 시너지
    FHSRoleSynergy LeaderTank;
    LeaderTank.Role1 = EHSTeamRole::Leader;
    LeaderTank.Role2 = EHSTeamRole::Tank;
    LeaderTank.SynergyMultiplier = 1.2f;
    LeaderTank.Description = TEXT("리더십으로 팀 조율 향상");
    RoleSynergies.Add(LeaderTank);
    
    FHSRoleSynergy LeaderDPS;
    LeaderDPS.Role1 = EHSTeamRole::Leader;
    LeaderDPS.Role2 = EHSTeamRole::DPS;
    LeaderDPS.SynergyMultiplier = 1.2f;
    LeaderDPS.Description = TEXT("전술 지시로 공격 효율성 증가");
    RoleSynergies.Add(LeaderDPS);
    
    FHSRoleSynergy LeaderSupport;
    LeaderSupport.Role1 = EHSTeamRole::Leader;
    LeaderSupport.Role2 = EHSTeamRole::Support;
    LeaderSupport.SynergyMultiplier = 1.25f;
    LeaderSupport.Description = TEXT("팀 전략 최적화");
    RoleSynergies.Add(LeaderSupport);
    
    // 힐러-서포터 시너지
    FHSRoleSynergy HealerSupport;
    HealerSupport.Role1 = EHSTeamRole::Healer;
    HealerSupport.Role2 = EHSTeamRole::Support;
    HealerSupport.SynergyMultiplier = 1.3f;
    HealerSupport.Description = TEXT("치유와 지원의 완벽한 조합");
    RoleSynergies.Add(HealerSupport);
    
    // 전문가-다목적 시너지
    FHSRoleSynergy SpecialistFlexible;
    SpecialistFlexible.Role1 = EHSTeamRole::Specialist;
    SpecialistFlexible.Role2 = EHSTeamRole::Flexible;
    SpecialistFlexible.SynergyMultiplier = 1.1f;
    SpecialistFlexible.Description = TEXT("전문성과 유연성의 조화");
    RoleSynergies.Add(SpecialistFlexible);
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 역할 시너지 초기화 완료 - %d개"), RoleSynergies.Num());
}

void UHSTeamFormationSystem::InitializeRoleWeights()
{
    RoleWeights.Empty();
    
    // 역할별 중요도 가중치 설정
    RoleWeights.Add(EHSTeamRole::Tank, 1.2f);        // 탱커는 중요
    RoleWeights.Add(EHSTeamRole::DPS, 1.0f);         // DPS는 기본
    RoleWeights.Add(EHSTeamRole::Support, 1.1f);     // 서포터는 중요
    RoleWeights.Add(EHSTeamRole::Healer, 1.3f);      // 힐러는 매우 중요
    RoleWeights.Add(EHSTeamRole::Scout, 0.9f);       // 정찰병은 선택적
    RoleWeights.Add(EHSTeamRole::Leader, 1.1f);      // 리더는 중요
    RoleWeights.Add(EHSTeamRole::Specialist, 0.8f);  // 전문가는 상황별
    RoleWeights.Add(EHSTeamRole::Flexible, 1.0f);    // 다목적은 기본
    
    UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 역할 가중치 초기화 완료"));
}

void UHSTeamFormationSystem::CheckExpiredMatchingRequests()
{
    FDateTime CurrentTime = FDateTime::Now();
    
    for (int32 i = MatchingQueue.Num() - 1; i >= 0; i--)
    {
        const FHSMatchingRequest& Request = MatchingQueue[i];
        
        float ElapsedTime = (CurrentTime - Request.RequestTime).GetTotalSeconds();
        
        if (ElapsedTime >= Request.TimeoutSeconds)
        {
            UE_LOG(LogTemp, Log, TEXT("HSTeamFormationSystem: 매칭 요청 타임아웃 - ID: %s"), *Request.RequestID);
            
            // 델리게이트 호출
            OnMatchingCancelled.Broadcast(Request.RequesterPlayerID, Request.RequestID);
            
            MatchingQueue.RemoveAt(i);
        }
    }
}

void UHSTeamFormationSystem::CleanupCache()
{
    FDateTime CurrentTime = FDateTime::Now();
    
    // 5분 이상 된 캐시 정리
    if ((CurrentTime - LastCacheUpdate).GetTotalMinutes() > 5.0f)
    {
        PlayerSearchCache.Empty();
        SynergyCache.Empty();
        CompatibilityCache.Empty();
        LastCacheUpdate = CurrentTime;
    }
    
    // 비활성 팀 정리 (30분 이상 된 팀)
    TArray<FString> TeamsToRemove;
    for (const auto& TeamPair : ActiveTeams)
    {
        const FHSTeamComposition& Team = TeamPair.Value;
        float TeamAge = (CurrentTime - Team.CreationTime).GetTotalMinutes();
        
        if (TeamAge > 30.0f)
        {
            TeamsToRemove.Add(TeamPair.Key);
        }
    }
    
    for (const FString& TeamID : TeamsToRemove)
    {
        ActiveTeams.Remove(TeamID);
    }
}

FString UHSTeamFormationSystem::GenerateTeamID() const
{
    FGuid NewGuid = FGuid::NewGuid();
    return FString::Printf(TEXT("TEAM_%s"), *NewGuid.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
}

FString UHSTeamFormationSystem::GenerateMatchingRequestID() const
{
    FGuid NewGuid = FGuid::NewGuid();
    return FString::Printf(TEXT("MATCH_%s"), *NewGuid.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
}

FString UHSTeamFormationSystem::GenerateTeamHash(const TArray<int32>& PlayerIDs) const
{
    TArray<int32> SortedIDs = PlayerIDs;
    SortedIDs.Sort();
    
    FString Hash;
    for (int32 ID : SortedIDs)
    {
        Hash += FString::Printf(TEXT("%d_"), ID);
    }
    
    return Hash;
}

TArray<FHSPlayerSkillInfo> UHSTeamFormationSystem::FindOptimalMatch(const FHSMatchingRequest& Request) const
{
    TArray<FHSPlayerSkillInfo> OptimalMatch;
    
    // 호환 가능한 플레이어 찾기
    TArray<FHSPlayerSkillInfo> Candidates = FindCompatiblePlayers(Request.Requirements);
    
    // 요청자도 포함
    FHSPlayerSkillInfo RequesterInfo = GetPlayerSkills(Request.RequesterPlayerID);
    if (RequesterInfo.PlayerID != -1)
    {
        Candidates.Add(RequesterInfo);
    }
    
    // 차단된 플레이어 제거
    for (int32 BlockedID : Request.BlockedPlayers)
    {
        Candidates.RemoveAll([BlockedID](const FHSPlayerSkillInfo& Player)
        {
            return Player.PlayerID == BlockedID;
        });
    }
    
    // 선호하는 팀원 우선 추가
    for (int32 PreferredID : Request.PreferredTeammates)
    {
        const FHSPlayerSkillInfo* PreferredPlayer = Candidates.FindByPredicate([PreferredID](const FHSPlayerSkillInfo& Player)
        {
            return Player.PlayerID == PreferredID;
        });
        
        if (PreferredPlayer && OptimalMatch.Num() < Request.Requirements.MaxPlayers)
        {
            OptimalMatch.Add(*PreferredPlayer);
            Candidates.Remove(*PreferredPlayer);
        }
    }
    
    // 나머지 자리를 최적 플레이어로 채움
    while (OptimalMatch.Num() < Request.Requirements.MaxPlayers && Candidates.Num() > 0)
    {
        FHSPlayerSkillInfo* BestCandidate = nullptr;
        float BestScore = -1.0f;
        
        for (FHSPlayerSkillInfo& Candidate : Candidates)
        {
            // 현재 팀과의 호환성 점수 계산
            float CompatibilityScore = 0.0f;
            
            for (const FHSPlayerSkillInfo& TeamMember : OptimalMatch)
            {
                CompatibilityScore += CalculatePlayerCompatibility(Candidate.PlayerID, TeamMember.PlayerID);
            }
            
            if (OptimalMatch.Num() > 0)
            {
                CompatibilityScore /= OptimalMatch.Num();
            }
            
            // 우선순위에 따른 추가 점수
            switch (Request.Priority)
            {
                case EHSMatchingPriority::SkillLevel:
                    CompatibilityScore += (Candidate.CombatRating + Candidate.TeamworkRating) / 200.0f;
                    break;
                    
                case EHSMatchingPriority::Experience:
                    CompatibilityScore += FMath::Min(1.0f, Candidate.GamesPlayed / 100.0f);
                    break;
                    
                case EHSMatchingPriority::Synergy:
                    // 이미 호환성 점수에 반영됨
                    break;
                    
                default:
                    break;
            }
            
            if (CompatibilityScore > BestScore)
            {
                BestScore = CompatibilityScore;
                BestCandidate = &Candidate;
            }
        }
        
        if (BestCandidate)
        {
            OptimalMatch.Add(*BestCandidate);
            Candidates.Remove(*BestCandidate);
        }
        else
        {
            break;
        }
    }
    
    return OptimalMatch;
}

TMap<int32, EHSTeamRole> UHSTeamFormationSystem::OptimizeRoleAssignment(const TArray<FHSPlayerSkillInfo>& Players, const FHSTeamRequirements& Requirements) const
{
    TMap<int32, EHSTeamRole> OptimalAssignment;
    
    // 현재는 간단한 구현 - 각 플레이어의 최고 숙련도 역할 할당
    for (const FHSPlayerSkillInfo& Player : Players)
    {
        EHSTeamRole BestRole = Player.PrimaryRole;
        float BestProficiency = 0.0f;
        
        // 모든 역할 중 가장 숙련된 역할 찾기
        for (const auto& ProficiencyPair : Player.RoleProficiency)
        {
            if (ProficiencyPair.Value > BestProficiency)
            {
                BestProficiency = ProficiencyPair.Value;
                BestRole = ProficiencyPair.Key;
            }
        }
        
        OptimalAssignment.Add(Player.PlayerID, BestRole);
    }
    
    return OptimalAssignment;
}
