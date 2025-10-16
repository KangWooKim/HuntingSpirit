// 사냥의 영혼(HuntingSpirit) 게임의 팀 관리자 클래스 구현

#include "HSTeamManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "../Core/PlayerController/HSPlayerController.h"
#include "../Characters/Player/HSPlayerCharacter.h"
#include "../Characters/Stats/HSStatsComponent.h"
#include "../Characters/Stats/HSLevelSystem.h"
#include "../Combat/HSCombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetConnection.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Set.h"

#include "Math/Vector.h"
#include "HAL/PlatformMath.h"

// 디버그 로깅 매크로 정의
DEFINE_LOG_CATEGORY_STATIC(LogHSTeamManager, Log, All);

// 성능 측정을 위한 매크로 (디버그 빌드에서만 활성화)
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    #define HS_TEAM_PERF_SCOPE(Name) SCOPE_CYCLE_COUNTER(STAT_##Name)
    DECLARE_CYCLE_STAT(TEXT("HSTeamManager_CreateTeam"), STAT_HSTeamManager_CreateTeam, STATGROUP_Game);
    DECLARE_CYCLE_STAT(TEXT("HSTeamManager_DisbandTeam"), STAT_HSTeamManager_DisbandTeam, STATGROUP_Game);
    DECLARE_CYCLE_STAT(TEXT("HSTeamManager_AddPlayer"), STAT_HSTeamManager_AddPlayer, STATGROUP_Game);
    DECLARE_CYCLE_STAT(TEXT("HSTeamManager_RemovePlayer"), STAT_HSTeamManager_RemovePlayer, STATGROUP_Game);
    DECLARE_CYCLE_STAT(TEXT("HSTeamManager_Cleanup"), STAT_HSTeamManager_Cleanup, STATGROUP_Game);
#else
    #define HS_TEAM_PERF_SCOPE(Name)
#endif

UHSTeamManager::UHSTeamManager()
{
    // 기본값 초기화
    NextTeamID = 1;
    CleanupInterval = 30.0f; // 30초마다 정리 작업 수행
    MaxTeamsAllowed = 100; // 최대 100개 팀까지 허용
    DefaultMaxTeamSize = 4; // 기본 4인 팀
    bIsInitialized = false;

    // 메모리 풀 미리 할당 (성능 최적화)
    InactiveTeamPool.Reserve(MaxTeamsAllowed / 4); // 25% 미리 할당
    TeamDatabase.Reserve(MaxTeamsAllowed);
    PlayerToTeamMap.Reserve(MaxTeamsAllowed * DefaultMaxTeamSize); // 예상 최대 플레이어 수

    UE_LOG(LogHSTeamManager, Log, TEXT("HSTeamManager 생성자 호출됨"));
}

void UHSTeamManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogHSTeamManager, Log, TEXT("HSTeamManager 초기화 시작"));

    // 초기화 플래그 설정
    bIsInitialized = true;

    // 정기적인 정리 작업 타이머 설정
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &UHSTeamManager::PerformScheduledCleanup,
            CleanupInterval,
            true // 반복 실행
        );

        UE_LOG(LogHSTeamManager, Log, TEXT("정리 작업 타이머 설정 완료 (%.1f초 주기)"), CleanupInterval);
    }

    UE_LOG(LogHSTeamManager, Log, TEXT("HSTeamManager 초기화 완료"));
}

void UHSTeamManager::Deinitialize()
{
    UE_LOG(LogHSTeamManager, Log, TEXT("HSTeamManager 종료 중..."));

    // 타이머 해제
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }

    // 모든 팀 해체
    DisbandAllTeams();

    // 메모리 정리
    {
        FScopeLock Lock(&TeamDatabaseMutex);
        TeamDatabase.Empty();
        PlayerToTeamMap.Empty();
        InactiveTeamPool.Empty();
        TeamCreationTimes.Empty();
    }

    bIsInitialized = false;
    
    UE_LOG(LogHSTeamManager, Log, TEXT("HSTeamManager 종료 완료"));

    Super::Deinitialize();
}

void UHSTeamManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 네트워크 복제 설정
    DOREPLIFETIME(UHSTeamManager, TeamDatabase);
    DOREPLIFETIME(UHSTeamManager, NextTeamID);
}

int32 UHSTeamManager::CreateTeam(APlayerState* TeamLeader, int32 MaxTeamSize)
{
    HS_TEAM_PERF_SCOPE(HSTeamManager_CreateTeam);

    // 입력 검증
    if (!TeamLeader)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("CreateTeam: TeamLeader가 null입니다"));
        return -1;
    }

    if (!bIsInitialized)
    {
        UE_LOG(LogHSTeamManager, Error, TEXT("CreateTeam: TeamManager가 초기화되지 않았습니다"));
        return -1;
    }

    // 이미 팀에 속한 플레이어인지 확인
    if (IsPlayerInTeam(TeamLeader))
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("CreateTeam: 플레이어 %s는 이미 팀에 속해있습니다"), *TeamLeader->GetPlayerName());
        return -1;
    }

    // 최대 팀 수 제한 확인
    if (TeamDatabase.Num() >= MaxTeamsAllowed)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("CreateTeam: 최대 팀 수에 도달했습니다 (%d/%d)"), TeamDatabase.Num(), MaxTeamsAllowed);
        return -1;
    }

    FScopeLock Lock(&TeamDatabaseMutex);

    // 새 팀 ID 생성
    const int32 NewTeamID = GenerateNewTeamID();

    // 팀 크기 설정 (기본값 사용 또는 사용자 지정)
    const int32 FinalMaxTeamSize = (MaxTeamSize > 0) ? MaxTeamSize : DefaultMaxTeamSize;

    // 메모리 풀에서 팀 정보 가져오기 (최적화)
    FHSTeamInfo NewTeamInfo;
    if (InactiveTeamPool.Num() > 0)
    {
        NewTeamInfo = InactiveTeamPool.Pop();
        UE_LOG(LogHSTeamManager, VeryVerbose, TEXT("메모리 풀에서 팀 정보 재사용"));
    }

    // 팀 정보 설정
    NewTeamInfo.TeamID = NewTeamID;
    NewTeamInfo.TeamLeader = TeamLeader;
    NewTeamInfo.TeamMembers.Empty();
    NewTeamInfo.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewTeamInfo.bIsActive = true;
    NewTeamInfo.MaxTeamSize = FinalMaxTeamSize;
    NewTeamInfo.SharedHealth = 100.0f;
    NewTeamInfo.TeamLevel = 1;

    // 팀 데이터베이스에 추가
    TeamDatabase.Add(NewTeamInfo);

    // 플레이어-팀 매핑 업데이트
    UpdatePlayerTeamMapping(TeamLeader, NewTeamID);

    // 생성 시간 기록 (성능 분석용)
    TeamCreationTimes.Add(NewTeamID, NewTeamInfo.CreationTime);

    UE_LOG(LogHSTeamManager, Log, TEXT("새 팀 생성됨 - ID: %d, 리더: %s, 최대 인원: %d"), 
           NewTeamID, *TeamLeader->GetPlayerName(), FinalMaxTeamSize);

    // 이벤트 브로드캐스트
    OnTeamCreated.Broadcast(NewTeamID, NewTeamInfo);

    return NewTeamID;
}

bool UHSTeamManager::DisbandTeam(int32 TeamID)
{
    HS_TEAM_PERF_SCOPE(HSTeamManager_DisbandTeam);

    if (!bIsInitialized)
    {
        UE_LOG(LogHSTeamManager, Error, TEXT("DisbandTeam: TeamManager가 초기화되지 않았습니다"));
        return false;
    }

    FScopeLock Lock(&TeamDatabaseMutex);

    // 팀 존재 여부 확인
    const int32 TeamIndex = FindTeamIndexByID(TeamID);
    if (TeamIndex == INDEX_NONE)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("DisbandTeam: 팀 ID %d를 찾을 수 없습니다"), TeamID);
        return false;
    }

    FHSTeamInfo* TeamInfo = &TeamDatabase[TeamIndex];

    // 해체 전에 팀 정보 백업 (이벤트용)
    const FHSTeamInfo TeamInfoCopy = *TeamInfo;

    // 모든 팀원의 매핑 제거
    if (TeamInfo->TeamLeader.IsValid())
    {
        UpdatePlayerTeamMapping(TeamInfo->TeamLeader.Get(), -1);
    }

    for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo->TeamMembers)
    {
        if (Member.IsValid())
        {
            UpdatePlayerTeamMapping(Member.Get(), -1);
        }
    }

    // 메모리 풀로 팀 정보 반환 (재사용을 위해)
    TeamInfo->bIsActive = false;
    TeamInfo->TeamMembers.Empty();
    TeamInfo->TeamLeader = nullptr;
    
    if (InactiveTeamPool.Num() < MaxTeamsAllowed / 4) // 풀 크기 제한
    {
        InactiveTeamPool.Add(*TeamInfo);
    }

    // 데이터베이스에서 제거
    TeamDatabase.RemoveAt(TeamIndex);
    TeamCreationTimes.Remove(TeamID);

    UE_LOG(LogHSTeamManager, Log, TEXT("팀 해체됨 - ID: %d"), TeamID);

    // 이벤트 브로드캐스트
    OnTeamDisbanded.Broadcast(TeamID, TeamInfoCopy);

    return true;
}

bool UHSTeamManager::AddPlayerToTeam(int32 TeamID, APlayerState* PlayerState)
{
    HS_TEAM_PERF_SCOPE(HSTeamManager_AddPlayer);

    // 입력 검증
    if (!PlayerState)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("AddPlayerToTeam: PlayerState가 null입니다"));
        return false;
    }

    if (!bIsInitialized)
    {
        UE_LOG(LogHSTeamManager, Error, TEXT("AddPlayerToTeam: TeamManager가 초기화되지 않았습니다"));
        return false;
    }

    // 이미 팀에 속한 플레이어인지 확인
    if (IsPlayerInTeam(PlayerState))
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("AddPlayerToTeam: 플레이어 %s는 이미 팀에 속해있습니다"), *PlayerState->GetPlayerName());
        return false;
    }

    FScopeLock Lock(&TeamDatabaseMutex);

    // 팀 존재 여부 확인
    FHSTeamInfo* TeamInfo = FindTeamByID(TeamID);
    if (!TeamInfo)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("AddPlayerToTeam: 팀 ID %d를 찾을 수 없습니다"), TeamID);
        return false;
    }

    // 팀이 가득 찼는지 확인
    if (TeamInfo->IsTeamFull())
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("AddPlayerToTeam: 팀 %d가 가득 참 (%d/%d)"), 
               TeamID, TeamInfo->GetTeamMemberCount(), TeamInfo->MaxTeamSize);
        return false;
    }

    // 팀원 추가
    TeamInfo->TeamMembers.Add(PlayerState);

    // 플레이어-팀 매핑 업데이트
    UpdatePlayerTeamMapping(PlayerState, TeamID);

    UE_LOG(LogHSTeamManager, Log, TEXT("플레이어 %s가 팀 %d에 가입함 (%d/%d)"), 
           *PlayerState->GetPlayerName(), TeamID, TeamInfo->GetTeamMemberCount(), TeamInfo->MaxTeamSize);

    // 이벤트 브로드캐스트
    OnPlayerJoinedTeam.Broadcast(TeamID, PlayerState, *TeamInfo);

    return true;
}

bool UHSTeamManager::RemovePlayerFromTeam(APlayerState* PlayerState)
{
    HS_TEAM_PERF_SCOPE(HSTeamManager_RemovePlayer);

    if (!PlayerState)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("RemovePlayerFromTeam: PlayerState가 null입니다"));
        return false;
    }

    if (!bIsInitialized)
    {
        UE_LOG(LogHSTeamManager, Error, TEXT("RemovePlayerFromTeam: TeamManager가 초기화되지 않았습니다"));
        return false;
    }

    // 플레이어가 속한 팀 찾기
    const int32 TeamID = GetPlayerTeamID(PlayerState);
    if (TeamID == -1)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("RemovePlayerFromTeam: 플레이어 %s는 어떤 팀에도 속하지 않습니다"), *PlayerState->GetPlayerName());
        return false;
    }

    FScopeLock Lock(&TeamDatabaseMutex);

    FHSTeamInfo* TeamInfo = FindTeamByID(TeamID);
    if (!TeamInfo)
    {
        UE_LOG(LogHSTeamManager, Error, TEXT("RemovePlayerFromTeam: 팀 ID %d를 찾을 수 없습니다"), TeamID);
        return false;
    }

    const bool bWasLeader = (TeamInfo->TeamLeader.IsValid() && TeamInfo->TeamLeader.Get() == PlayerState);

    // 팀원에서 제거
    TeamInfo->TeamMembers.RemoveAll([PlayerState](const TWeakObjectPtr<APlayerState>& Member)
    {
        return Member.IsValid() && Member.Get() == PlayerState;
    });

    // 리더였다면 리더십 이양 또는 팀 해체
    if (bWasLeader)
    {
        if (TeamInfo->TeamMembers.Num() > 0)
        {
            // 첫 번째 팀원을 새 리더로 임명
            APlayerState* NewLeader = TeamInfo->TeamMembers[0].Get();
            if (NewLeader)
            {
                TeamInfo->TeamLeader = NewLeader;
                // 새 리더를 팀원 목록에서 제거
                TeamInfo->TeamMembers.RemoveAt(0);
                
                UE_LOG(LogHSTeamManager, Log, TEXT("팀 %d의 리더가 %s에서 %s로 변경됨"), 
                       TeamID, *PlayerState->GetPlayerName(), *NewLeader->GetPlayerName());

                // 리더 변경 이벤트
                OnTeamLeaderChanged.Broadcast(TeamID, NewLeader, PlayerState);
            }
        }
        else
        {
            // 팀원이 없으면 팀 해체
            UE_LOG(LogHSTeamManager, Log, TEXT("리더 %s가 떠나고 팀원이 없어 팀 %d 해체"), 
                   *PlayerState->GetPlayerName(), TeamID);
            
            // 여기서는 Lock을 해제하고 DisbandTeam을 호출해야 함 (재귀 Lock 방지)
            Lock.~FScopeLock();
            DisbandTeam(TeamID);
            UpdatePlayerTeamMapping(PlayerState, -1);
            return true;
        }
    }

    // 플레이어-팀 매핑 업데이트
    UpdatePlayerTeamMapping(PlayerState, -1);

    UE_LOG(LogHSTeamManager, Log, TEXT("플레이어 %s가 팀 %d에서 떠남"), *PlayerState->GetPlayerName(), TeamID);

    // 이벤트 브로드캐스트
    OnPlayerLeftTeam.Broadcast(TeamID, PlayerState, *TeamInfo);

    return true;
}

bool UHSTeamManager::ChangeTeamLeader(int32 TeamID, APlayerState* NewLeader)
{
    if (!NewLeader || !bIsInitialized)
    {
        return false;
    }

    FScopeLock Lock(&TeamDatabaseMutex);

    FHSTeamInfo* TeamInfo = FindTeamByID(TeamID);
    if (!TeamInfo)
    {
        return false;
    }

    // 새 리더가 팀에 속해있는지 확인
    if (!TeamInfo->IsPlayerInTeam(NewLeader))
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("ChangeTeamLeader: %s는 팀 %d에 속하지 않습니다"), 
               *NewLeader->GetPlayerName(), TeamID);
        return false;
    }

    APlayerState* OldLeader = TeamInfo->TeamLeader.Get();
    
    // 새 리더를 팀원 목록에서 제거하고 리더로 설정
    TeamInfo->TeamMembers.RemoveAll([NewLeader](const TWeakObjectPtr<APlayerState>& Member)
    {
        return Member.IsValid() && Member.Get() == NewLeader;
    });

    // 이전 리더를 팀원으로 추가
    if (OldLeader)
    {
        TeamInfo->TeamMembers.Add(OldLeader);
    }

    TeamInfo->TeamLeader = NewLeader;

    UE_LOG(LogHSTeamManager, Log, TEXT("팀 %d의 리더가 변경됨: %s -> %s"), 
           TeamID, 
           OldLeader ? *OldLeader->GetPlayerName() : TEXT("None"),
           *NewLeader->GetPlayerName());

    // 이벤트 브로드캐스트
    OnTeamLeaderChanged.Broadcast(TeamID, NewLeader, OldLeader);

    return true;
}

int32 UHSTeamManager::GetPlayerTeamID(APlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return -1;
    }

    // 캐시에서 빠른 검색
    if (const int32* TeamID = PlayerToTeamMap.Find(PlayerState))
    {
        return *TeamID;
    }

    return -1;
}

FHSTeamInfo UHSTeamManager::GetTeamInfo(int32 TeamID) const
{
    FScopeLock Lock(&TeamDatabaseMutex);

    if (const FHSTeamInfo* TeamInfo = FindTeamByID(TeamID))
    {
        return *TeamInfo;
    }

    return FHSTeamInfo(); // 빈 구조체 반환
}

FHSTeamInfo UHSTeamManager::GetPlayerTeamInfo(APlayerState* PlayerState) const
{
    const int32 TeamID = GetPlayerTeamID(PlayerState);
    if (TeamID != -1)
    {
        return GetTeamInfo(TeamID);
    }

    return FHSTeamInfo(); // 빈 구조체 반환
}

TArray<int32> UHSTeamManager::GetAllActiveTeamIDs() const
{
    TArray<int32> ActiveTeamIDs;
    
    FScopeLock Lock(&TeamDatabaseMutex);
    
    // 메모리 미리 할당 (성능 최적화)
    ActiveTeamIDs.Reserve(TeamDatabase.Num());

    for (const FHSTeamInfo& TeamInfo : TeamDatabase)
    {
        if (TeamInfo.bIsActive)
        {
            ActiveTeamIDs.Add(TeamInfo.TeamID);
        }
    }

    return ActiveTeamIDs;
}

bool UHSTeamManager::IsPlayerInTeam(APlayerState* PlayerState) const
{
    return GetPlayerTeamID(PlayerState) != -1;
}

bool UHSTeamManager::ArePlayersInSameTeam(APlayerState* PlayerA, APlayerState* PlayerB) const
{
    if (!PlayerA || !PlayerB)
    {
        return false;
    }

    const int32 TeamA = GetPlayerTeamID(PlayerA);
    const int32 TeamB = GetPlayerTeamID(PlayerB);

    return (TeamA != -1) && (TeamA == TeamB);
}

bool UHSTeamManager::IsPlayerTeamLeader(APlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return false;
    }

    const int32 TeamID = GetPlayerTeamID(PlayerState);
    if (TeamID == -1)
    {
        return false;
    }

    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    return TeamInfo.TeamLeader.IsValid() && TeamInfo.TeamLeader.Get() == PlayerState;
}

void UHSTeamManager::BroadcastMessageToTeam(int32 TeamID, const FString& Message, bool bIncludeLeader)
{
    if (Message.IsEmpty())
    {
        return;
    }

    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    if (!TeamInfo.bIsActive)
    {
        return;
    }

    TSet<const APlayerState*> ProcessedPlayers;
    const auto SendToPlayer = [&](const TWeakObjectPtr<APlayerState>& PlayerPtr)
    {
        if (!PlayerPtr.IsValid())
        {
            return;
        }

        APlayerState* PlayerState = PlayerPtr.Get();
        if (ProcessedPlayers.Contains(PlayerState))
        {
            return;
        }

        if (AController* OwnerController = Cast<AController>(PlayerState->GetOwner()))
        {
            if (AHSPlayerController* HSController = Cast<AHSPlayerController>(OwnerController))
            {
                HSController->ClientMessage(Message);
            }
            else if (APlayerController* PlayerController = Cast<APlayerController>(OwnerController))
            {
                PlayerController->ClientMessage(Message);
            }
        }

        ProcessedPlayers.Add(PlayerState);
    };

    if (bIncludeLeader)
    {
        SendToPlayer(TeamInfo.TeamLeader);
    }

    for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo.TeamMembers)
    {
        SendToPlayer(Member);
    }

    UE_LOG(LogHSTeamManager, Log, TEXT("팀 %d에 메시지 브로드캐스트: %s"), TeamID, *Message);
}

float UHSTeamManager::GetTeamAverageLevel(int32 TeamID) const
{
    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    if (!TeamInfo.bIsActive)
    {
        return 0.0f;
    }

    float TotalLevel = 0.0f;
    int32 ValidPlayerCount = 0;
    TSet<const APlayerState*> ProcessedPlayers;

    const auto AccumulateLevel = [&](const TWeakObjectPtr<APlayerState>& PlayerPtr)
    {
        if (!PlayerPtr.IsValid())
        {
            return;
        }

        APlayerState* PlayerState = PlayerPtr.Get();
        if (ProcessedPlayers.Contains(PlayerState))
        {
            return;
        }

        ProcessedPlayers.Add(PlayerState);

        if (APawn* PlayerPawn = PlayerState->GetPawn())
        {
            if (AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(PlayerPawn))
            {
                if (UHSStatsComponent* StatsComponent = PlayerCharacter->GetStatsComponent())
                {
                    if (UHSLevelSystem* LevelSystem = StatsComponent->GetLevelSystem())
                    {
                        TotalLevel += static_cast<float>(LevelSystem->GetCurrentLevel());
                        ValidPlayerCount++;
                    }
                }
            }
        }
    };

    AccumulateLevel(TeamInfo.TeamLeader);

    for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo.TeamMembers)
    {
        AccumulateLevel(Member);
    }

    if (ValidPlayerCount == 0)
    {
        return static_cast<float>(TeamInfo.TeamLevel);
    }

    return TotalLevel / ValidPlayerCount;
}

float UHSTeamManager::GetTeamTotalHealth(int32 TeamID) const
{
    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    if (!TeamInfo.bIsActive)
    {
        return 0.0f;
    }

    float TotalHealth = 0.0f;
    TSet<const APlayerState*> ProcessedPlayers;

    const auto AccumulateHealth = [&](const TWeakObjectPtr<APlayerState>& PlayerPtr)
    {
        if (!PlayerPtr.IsValid())
        {
            return;
        }

        APlayerState* PlayerState = PlayerPtr.Get();
        if (ProcessedPlayers.Contains(PlayerState))
        {
            return;
        }

        ProcessedPlayers.Add(PlayerState);

        if (APawn* PlayerPawn = PlayerState->GetPawn())
        {
            if (AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(PlayerPawn))
            {
                if (UHSStatsComponent* StatsComponent = PlayerCharacter->GetStatsComponent())
                {
                    TotalHealth += StatsComponent->GetCurrentHealth();
                    return;
                }

                if (UHSCombatComponent* CombatComponent = PlayerCharacter->FindComponentByClass<UHSCombatComponent>())
                {
                    TotalHealth += CombatComponent->GetCurrentHealth();
                }
            }
        }
    };

    AccumulateHealth(TeamInfo.TeamLeader);

    for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo.TeamMembers)
    {
        AccumulateHealth(Member);
    }

    return TotalHealth;
}

FVector UHSTeamManager::GetTeamCenterLocation(int32 TeamID) const
{
    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    if (!TeamInfo.bIsActive)
    {
        return FVector::ZeroVector;
    }

    // SIMD 최적화를 위한 벡터 배열 준비
    TArray<FVector> PlayerLocations;
    PlayerLocations.Reserve(TeamInfo.MaxTeamSize);

    // 리더 위치 추가
    if (TeamInfo.TeamLeader.IsValid())
    {
        if (APawn* PlayerPawn = TeamInfo.TeamLeader->GetPawn())
        {
            PlayerLocations.Add(PlayerPawn->GetActorLocation());
        }
    }

    // 팀원 위치 추가
    for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo.TeamMembers)
    {
        if (Member.IsValid())
        {
            if (APawn* PlayerPawn = Member->GetPawn())
            {
                PlayerLocations.Add(PlayerPawn->GetActorLocation());
            }
        }
    }

    if (PlayerLocations.Num() == 0)
    {
        return FVector::ZeroVector;
    }

    // SIMD 최적화된 벡터 평균 계산
    FVector CenterLocation = FVector::ZeroVector;
    for (const FVector& Location : PlayerLocations)
    {
        CenterLocation += Location;
    }

    return CenterLocation / PlayerLocations.Num();
}

void UHSTeamManager::CleanupInvalidData()
{
    HS_TEAM_PERF_SCOPE(HSTeamManager_Cleanup);

    if (!bIsInitialized)
    {
        return;
    }

    FScopeLock Lock(&TeamDatabaseMutex);

    TArray<int32> TeamsToRemove;
    
    // 유효하지 않은 팀들 찾기
    for (FHSTeamInfo& TeamInfo : TeamDatabase)
    {
        // 유효하지 않은 팀원들 정리
        TeamInfo.CleanupInvalidMembers();
        
        // 리더와 팀원이 모두 없으면 팀 제거 대상으로 표시
        if (!TeamInfo.TeamLeader.IsValid() && TeamInfo.TeamMembers.Num() == 0)
        {
            TeamsToRemove.Add(TeamInfo.TeamID);
        }
    }

    // 유효하지 않은 팀들 제거 (역순으로 제거하여 인덱스 문제 방지)
    for (int32 TeamID : TeamsToRemove)
    {
        const int32 TeamIndex = FindTeamIndexByID(TeamID);
        if (TeamIndex != INDEX_NONE)
        {
            UE_LOG(LogHSTeamManager, Log, TEXT("유효하지 않은 팀 제거: %d"), TeamID);
            TeamDatabase.RemoveAt(TeamIndex);
            TeamCreationTimes.Remove(TeamID);
        }
    }

    // 플레이어-팀 매핑 캐시 정리
    TArray<TWeakObjectPtr<APlayerState>> PlayersToRemove;
    for (const auto& PlayerTeamPair : PlayerToTeamMap)
    {
        if (!PlayerTeamPair.Key.IsValid() || FindTeamIndexByID(PlayerTeamPair.Value) == INDEX_NONE)
        {
            PlayersToRemove.Add(PlayerTeamPair.Key);
        }
    }

    for (const TWeakObjectPtr<APlayerState>& Player : PlayersToRemove)
    {
        PlayerToTeamMap.Remove(Player);
    }

    UE_LOG(LogHSTeamManager, VeryVerbose, TEXT("정리 작업 완료 - 제거된 팀: %d개, 정리된 매핑: %d개"), 
           TeamsToRemove.Num(), PlayersToRemove.Num());
}

void UHSTeamManager::DisbandAllTeams()
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogHSTeamManager, Warning, TEXT("모든 팀 강제 해체 시작..."));

    TArray<int32> AllTeamIDs = GetAllActiveTeamIDs();
    
    for (int32 TeamID : AllTeamIDs)
    {
        DisbandTeam(TeamID);
    }

    UE_LOG(LogHSTeamManager, Warning, TEXT("모든 팀 해체 완료 (%d개 팀)"), AllTeamIDs.Num());
}

void UHSTeamManager::LogTeamManagerStatus() const
{
    UE_LOG(LogHSTeamManager, Log, TEXT("=== HSTeamManager 상태 ==="));
    UE_LOG(LogHSTeamManager, Log, TEXT("초기화 상태: %s"), bIsInitialized ? TEXT("완료") : TEXT("미완료"));
    UE_LOG(LogHSTeamManager, Log, TEXT("활성 팀 수: %d/%d"), TeamDatabase.Num(), MaxTeamsAllowed);
    UE_LOG(LogHSTeamManager, Log, TEXT("다음 팀 ID: %d"), NextTeamID);
    UE_LOG(LogHSTeamManager, Log, TEXT("플레이어-팀 매핑 수: %d"), PlayerToTeamMap.Num());
    UE_LOG(LogHSTeamManager, Log, TEXT("메모리 풀 크기: %d"), InactiveTeamPool.Num());
    UE_LOG(LogHSTeamManager, Log, TEXT("정리 작업 주기: %.1f초"), CleanupInterval);
    UE_LOG(LogHSTeamManager, Log, TEXT("==========================="));
}

void UHSTeamManager::LogTeamDetails(int32 TeamID) const
{
    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    
    if (!TeamInfo.bIsActive)
    {
        UE_LOG(LogHSTeamManager, Warning, TEXT("팀 ID %d는 존재하지 않거나 비활성 상태입니다"), TeamID);
        return;
    }

    UE_LOG(LogHSTeamManager, Log, TEXT("=== 팀 %d 상세 정보 ==="), TeamID);
    UE_LOG(LogHSTeamManager, Log, TEXT("리더: %s"), 
           TeamInfo.TeamLeader.IsValid() ? *TeamInfo.TeamLeader->GetPlayerName() : TEXT("None"));
    UE_LOG(LogHSTeamManager, Log, TEXT("팀원 수: %d/%d"), TeamInfo.GetTeamMemberCount(), TeamInfo.MaxTeamSize);
    
    for (int32 i = 0; i < TeamInfo.TeamMembers.Num(); i++)
    {
        if (TeamInfo.TeamMembers[i].IsValid())
        {
            UE_LOG(LogHSTeamManager, Log, TEXT("  팀원 %d: %s"), i + 1, *TeamInfo.TeamMembers[i]->GetPlayerName());
        }
    }
    
    UE_LOG(LogHSTeamManager, Log, TEXT("생성 시간: %.2f"), TeamInfo.CreationTime);
    UE_LOG(LogHSTeamManager, Log, TEXT("팀 레벨: %d"), TeamInfo.TeamLevel);
    UE_LOG(LogHSTeamManager, Log, TEXT("======================="));
}

int32 UHSTeamManager::GenerateNewTeamID()
{
    // 원자적 증가 연산으로 스레드 안전성 보장
    return FPlatformAtomics::InterlockedIncrement(&NextTeamID);
}

bool UHSTeamManager::ValidateTeamInfo(const FHSTeamInfo& TeamInfo) const
{
    // 기본 유효성 검사
    if (TeamInfo.TeamID <= 0)
    {
        return false;
    }

    if (TeamInfo.MaxTeamSize <= 0 || TeamInfo.MaxTeamSize > 100) // 현실적인 최대값 설정
    {
        return false;
    }

    if (TeamInfo.GetTeamMemberCount() > TeamInfo.MaxTeamSize)
    {
        return false;
    }

    return true;
}

void UHSTeamManager::UpdatePlayerTeamMapping(APlayerState* PlayerState, int32 NewTeamID)
{
    if (!PlayerState)
    {
        return;
    }

    if (NewTeamID == -1)
    {
        // 팀에서 제거
        PlayerToTeamMap.Remove(PlayerState);
    }
    else
    {
        // 새 팀에 추가
        PlayerToTeamMap.Add(PlayerState, NewTeamID);
    }
}

void UHSTeamManager::PerformScheduledCleanup()
{
    UE_LOG(LogHSTeamManager, VeryVerbose, TEXT("정기 정리 작업 실행"));
    CleanupInvalidData();
}

void UHSTeamManager::OnRep_TeamDatabase()
{
    // 네트워크 복제 시 호출되는 함수
    UE_LOG(LogHSTeamManager, VeryVerbose, TEXT("팀 데이터베이스 네트워크 복제됨"));
    
    // 클라이언트에서 플레이어-팀 매핑 캐시 재구성
    PlayerToTeamMap.Empty();
    
    for (const FHSTeamInfo& TeamInfo : TeamDatabase)
    {
        if (TeamInfo.TeamLeader.IsValid())
        {
            PlayerToTeamMap.Add(TeamInfo.TeamLeader, TeamInfo.TeamID);
        }
        
        for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo.TeamMembers)
        {
            if (Member.IsValid())
            {
                PlayerToTeamMap.Add(Member, TeamInfo.TeamID);
            }
        }
    }
}

// === 헬퍼 함수 구현 ===

FHSTeamInfo* UHSTeamManager::FindTeamByID(int32 TeamID)
{
    // 선형 검색으로 팀 찾기 (팀 개수가 적으므로 성능상 문제없음)
    for (FHSTeamInfo& TeamInfo : TeamDatabase)
    {
        if (TeamInfo.TeamID == TeamID)
        {
            return &TeamInfo;
        }
    }
    return nullptr;
}

const FHSTeamInfo* UHSTeamManager::FindTeamByID(int32 TeamID) const
{
    // 선형 검색으로 팀 찾기 (팀 개수가 적으므로 성능상 문제없음)
    for (const FHSTeamInfo& TeamInfo : TeamDatabase)
    {
        if (TeamInfo.TeamID == TeamID)
        {
            return &TeamInfo;
        }
    }
    return nullptr;
}

int32 UHSTeamManager::FindTeamIndexByID(int32 TeamID) const
{
    // 선형 검색으로 팀 인덱스 찾기
    for (int32 i = 0; i < TeamDatabase.Num(); ++i)
    {
        if (TeamDatabase[i].TeamID == TeamID)
        {
            return i;
        }
    }
    return INDEX_NONE;
}

// === Blueprint 안전 접근 헬퍼 함수들 ===

APlayerState* UHSTeamManager::GetTeamLeader(int32 TeamID) const
{
    // 팀 정보를 안전하게 가져와서 리더 반환
    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    if (!TeamInfo.bIsActive)
    {
        return nullptr;
    }

    return TeamInfo.TeamLeader.IsValid() ? TeamInfo.TeamLeader.Get() : nullptr;
}

TArray<APlayerState*> UHSTeamManager::GetTeamMembers(int32 TeamID) const
{
    TArray<APlayerState*> Members;
    
    // 팀 정보를 안전하게 가져오기
    const FHSTeamInfo TeamInfo = GetTeamInfo(TeamID);
    if (!TeamInfo.bIsActive)
    {
        return Members;
    }

    // 메모리 미리 할당 (성능 최적화)
    Members.Reserve(TeamInfo.TeamMembers.Num() + 1); // +1은 리더 포함 가능성

    // 팀 리더도 포함 (전체 팀원 개념)
    if (TeamInfo.TeamLeader.IsValid())
    {
        Members.Add(TeamInfo.TeamLeader.Get());
    }

    // 일반 팀원들 추가
    for (const TWeakObjectPtr<APlayerState>& Member : TeamInfo.TeamMembers)
    {
        if (Member.IsValid())
        {
            Members.Add(Member.Get());
        }
    }

    return Members;
}

APlayerState* UHSTeamManager::GetPlayerTeamLeader(APlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return nullptr;
    }

    // 플레이어가 속한 팀 ID 찾기
    const int32 TeamID = GetPlayerTeamID(PlayerState);
    if (TeamID == -1)
    {
        return nullptr;
    }

    // 해당 팀의 리더 반환
    return GetTeamLeader(TeamID);
}

TArray<APlayerState*> UHSTeamManager::GetPlayerTeamMembers(APlayerState* PlayerState) const
{
    TArray<APlayerState*> EmptyArray;
    
    if (!PlayerState)
    {
        return EmptyArray;
    }

    // 플레이어가 속한 팀 ID 찾기
    const int32 TeamID = GetPlayerTeamID(PlayerState);
    if (TeamID == -1)
    {
        return EmptyArray;
    }

    // 해당 팀의 모든 멤버 반환 (리더 + 팀원)
    return GetTeamMembers(TeamID);
}
