// 사냥의 영혼(HuntingSpirit) 게임의 플레이어 상태 클래스 구현
// 개별 플레이어의 상태와 통계를 관리하고 네트워크 복제를 담당하는 클래스 구현

#include "HSPlayerState.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HuntingSpirit/Characters/Stats/HSStatsComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetConnection.h"
#include "Kismet/KismetMathLibrary.h"

// 생성자
AHSPlayerState::AHSPlayerState()
{
    // 기본값 설정
    PlayerStatus = EHSPlayerStatus::PS_Loading;
    PlayerClass = EHSPlayerClass::None;
    PlayerRole = EHSPlayerRole::PR_None;
    TeamID = -1; // 팀 없음
    
    // 구조체 기본값 초기화
    PlayerStatistics = FHSPlayerSessionStatistics();
    LevelInfo = FHSPlayerLevelInfo();
    InventoryState = FHSPlayerInventoryState();

    // 타이밍 변수 초기화
    PlayStartTime = 0.0f;
    CurrentLifeStartTime = 0.0f;
    LastActionTime = 0.0f;

    // 설정 가능한 매개변수 기본값
    ExperienceMultiplier = 1.0f;
    BaseExperiencePerLevel = 100.0f;
    ExperienceScalingFactor = 1.2f;
    MaxLevel = 50;
    StatisticsUpdateInterval = 10.0f; // 10초마다
    NetworkStatusCheckInterval = 5.0f; // 5초마다

    // 플래그 초기화
    bInitialized = false;

    // 네트워크 복제 설정
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.0f; // 1초마다 틱 (성능 최적화)

    // 레벨 정보 기본값 설정
    LevelInfo.ExperienceToNextLevel = CalculateExperienceForNextLevel(1);
}

// 게임 시작 시 호출
void AHSPlayerState::BeginPlay()
{
    Super::BeginPlay();

    // 플레이 시작 시간 기록
    PlayStartTime = GetWorld()->GetTimeSeconds();
    CurrentLifeStartTime = PlayStartTime;

    // 서버에서만 타이머 설정
    if (HasAuthority())
    {
        SetupTimers();
    }

    // 플레이어 상태를 생존으로 변경
    if (HasAuthority())
    {
        SetPlayerStatus(EHSPlayerStatus::PS_Alive);
    }

    bInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 상태 초기화 완료 - %s"), *GetPlayerName());
}

// 매 프레임 호출
void AHSPlayerState::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 서버에서만 실행
    if (!HasAuthority() || !bInitialized)
    {
        return;
    }

    // 생존 시간 업데이트 (생존 상태인 경우만)
    if (PlayerStatus == EHSPlayerStatus::PS_Alive)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        PlayerStatistics.SurvivalTime = CurrentTime - CurrentLifeStartTime;
    }
}

// 게임 종료 시 호출
void AHSPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(SurvivalTimeUpdateTimer);
        TimerManager.ClearTimer(StatisticsUpdateTimer);
        TimerManager.ClearTimer(NetworkStatusTimer);
    }

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 상태 정리 완료 - %s"), *GetPlayerName());

    Super::EndPlay(EndPlayReason);
}

// 네트워크 복제 설정
void AHSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 플레이어 상태 복제
    DOREPLIFETIME(AHSPlayerState, PlayerStatus);
    DOREPLIFETIME(AHSPlayerState, PlayerClass);
    DOREPLIFETIME(AHSPlayerState, PlayerRole);
    DOREPLIFETIME(AHSPlayerState, TeamID);
    DOREPLIFETIME(AHSPlayerState, PlayerStatistics);
    DOREPLIFETIME(AHSPlayerState, LevelInfo);
    DOREPLIFETIME(AHSPlayerState, InventoryState);
}

// === 플레이어 상태 관리 ===

// 플레이어 상태 설정
void AHSPlayerState::SetPlayerStatus(EHSPlayerStatus NewStatus)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSPlayerState: 클라이언트에서 플레이어 상태 변경 시도"));
        return;
    }

    if (PlayerStatus == NewStatus)
    {
        return;
    }

    EHSPlayerStatus OldStatus = PlayerStatus;
    PlayerStatus = NewStatus;

    // 상태 변경에 따른 처리
    switch (NewStatus)
    {
        case EHSPlayerStatus::PS_Alive:
            // 생존 시작 시간 기록
            CurrentLifeStartTime = GetWorld()->GetTimeSeconds();
            break;

        case EHSPlayerStatus::PS_Dead:
            // 데스 카운트 증가
            IncrementDeaths();
            break;

        case EHSPlayerStatus::PS_Reviving:
            // 부활 중 상태 처리
            break;

        default:
            break;
    }

    // 이벤트 브로드캐스트
    OnPlayerStatusChanged.Broadcast(NewStatus);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 상태 변경 - %s: %d -> %d"), 
           *GetPlayerName(), (int32)OldStatus, (int32)NewStatus);
}

// === 플레이어 클래스 및 역할 관리 ===

// 플레이어 클래스 설정
void AHSPlayerState::SetPlayerClass(EHSPlayerClass NewPlayerClass)
{
    if (!HasAuthority())
    {
        return;
    }

    if (PlayerClass != NewPlayerClass)
    {
        PlayerClass = NewPlayerClass;
        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 클래스 변경 - %s: %d"), 
               *GetPlayerName(), (int32)NewPlayerClass);
    }
}

// 플레이어 역할 설정
void AHSPlayerState::SetPlayerRole(EHSPlayerRole NewRole)
{
    if (!HasAuthority())
    {
        return;
    }

    if (PlayerRole != NewRole)
    {
        EHSPlayerRole OldRole = PlayerRole;
        PlayerRole = NewRole;

        // 이벤트 브로드캐스트
        OnPlayerRoleChanged.Broadcast(OldRole, NewRole);

        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 역할 변경 - %s: %d -> %d"), 
               *GetPlayerName(), (int32)OldRole, (int32)NewRole);
    }
}

// === 팀 관리 ===

// 팀 ID 설정
void AHSPlayerState::SetTeamID(int32 NewTeamID)
{
    if (!HasAuthority())
    {
        return;
    }

    if (TeamID != NewTeamID)
    {
        int32 OldTeamID = TeamID;
        TeamID = NewTeamID;

        // 이벤트 브로드캐스트
        OnPlayerTeamChanged.Broadcast(OldTeamID, NewTeamID);

        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 팀 변경 - %s: %d -> %d"), 
               *GetPlayerName(), OldTeamID, NewTeamID);
    }
}

// === 레벨 및 경험치 시스템 ===

// 경험치 추가
void AHSPlayerState::AddExperience(float ExperienceAmount, bool bBroadcastEvent)
{
    if (!HasAuthority() || ExperienceAmount <= 0.0f)
    {
        return;
    }

    FScopeLock LevelLock(&LevelInfoMutex);

    // 경험치 배율 적용
    float ActualExperience = ExperienceAmount * ExperienceMultiplier;
    
    LevelInfo.CurrentExperience += ActualExperience;
    LevelInfo.TotalExperience += ActualExperience;

    // 레벨업 확인
    while (LevelInfo.CurrentExperience >= LevelInfo.ExperienceToNextLevel && 
           LevelInfo.CurrentLevel < MaxLevel)
    {
        // 남은 경험치 계산
        float RemainingExperience = LevelInfo.CurrentExperience - LevelInfo.ExperienceToNextLevel;
        
        // 레벨업 처리
        int32 OldLevel = LevelInfo.CurrentLevel;
        LevelInfo.CurrentLevel++;
        LevelInfo.CurrentExperience = RemainingExperience;
        LevelInfo.ExperienceToNextLevel = CalculateExperienceForNextLevel(LevelInfo.CurrentLevel);
        LevelInfo.LevelStartTime = GetWorld()->GetTimeSeconds();
        
        // 스킬 포인트 지급
        LevelInfo.SkillPoints++;

        // 레벨업 처리
        ProcessLevelUp(LevelInfo.CurrentLevel);

        // 이벤트 브로드캐스트
        OnPlayerLevelUp.Broadcast(OldLevel, LevelInfo.CurrentLevel);

        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 레벨업! - %s: %d -> %d"), 
               *GetPlayerName(), OldLevel, LevelInfo.CurrentLevel);
    }

    // 경험치 획득 이벤트 브로드캐스트
    if (bBroadcastEvent)
    {
        OnPlayerExperienceGained.Broadcast(ActualExperience, LevelInfo.TotalExperience);
    }
}

// 레벨 설정 (관리자 전용)
void AHSPlayerState::SetLevel(int32 NewLevel)
{
    if (!HasAuthority())
    {
        return;
    }

    NewLevel = FMath::Clamp(NewLevel, 1, MaxLevel);
    
    if (LevelInfo.CurrentLevel != NewLevel)
    {
        int32 OldLevel = LevelInfo.CurrentLevel;
        LevelInfo.CurrentLevel = NewLevel;
        LevelInfo.CurrentExperience = 0.0f;
        LevelInfo.ExperienceToNextLevel = CalculateExperienceForNextLevel(NewLevel);
        LevelInfo.LevelStartTime = GetWorld()->GetTimeSeconds();

        ProcessLevelUp(NewLevel);
        OnPlayerLevelUp.Broadcast(OldLevel, NewLevel);

        UE_LOG(LogTemp, Warning, TEXT("HSPlayerState: 관리자에 의한 레벨 설정 - %s: %d -> %d"), 
               *GetPlayerName(), OldLevel, NewLevel);
    }
}

// 레벨 진행률 반환
float AHSPlayerState::GetLevelProgress() const
{
    if (LevelInfo.ExperienceToNextLevel <= 0.0f)
    {
        return 1.0f; // 최대 레벨
    }

    return FMath::Clamp(LevelInfo.CurrentExperience / LevelInfo.ExperienceToNextLevel, 0.0f, 1.0f);
}

// === 통계 관리 ===

// 킬 수 증가
void AHSPlayerState::IncrementKills(int32 KillCount)
{
    if (!HasAuthority() || KillCount <= 0)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    PlayerStatistics.Kills += KillCount;
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 킬 수 증가 - %s: +%d (총 %d)"), 
           *GetPlayerName(), KillCount, PlayerStatistics.Kills);
}

// 데스 수 증가
void AHSPlayerState::IncrementDeaths()
{
    if (!HasAuthority())
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    PlayerStatistics.Deaths++;
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 데스 수 증가 - %s: %d"), 
           *GetPlayerName(), PlayerStatistics.Deaths);
}

// 어시스트 수 증가
void AHSPlayerState::IncrementAssists(int32 AssistCount)
{
    if (!HasAuthority() || AssistCount <= 0)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    PlayerStatistics.Assists += AssistCount;
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 어시스트 수 증가 - %s: +%d (총 %d)"), 
           *GetPlayerName(), AssistCount, PlayerStatistics.Assists);
}

// 데미지 통계 업데이트
void AHSPlayerState::UpdateDamageStatistics(float DamageDealt, float DamageTaken)
{
    if (!HasAuthority())
    {
        return;
    }

    bool bUpdated = false;
    
    {
        FScopeLock StatisticsLock(&StatisticsMutex);
        
        if (DamageDealt > 0.0f)
        {
            PlayerStatistics.TotalDamageDealt += DamageDealt;
            bUpdated = true;
        }
        
        if (DamageTaken > 0.0f)
        {
            PlayerStatistics.TotalDamageTaken += DamageTaken;
            bUpdated = true;
        }
    }

    if (bUpdated)
    {
        OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);
    }
}

// 힐링 통계 업데이트
void AHSPlayerState::UpdateHealingStatistics(float HealingDone, float HealingReceived)
{
    if (!HasAuthority())
    {
        return;
    }

    bool bUpdated = false;
    
    {
        FScopeLock StatisticsLock(&StatisticsMutex);
        
        if (HealingDone > 0.0f)
        {
            PlayerStatistics.TotalHealingDone += HealingDone;
            bUpdated = true;
        }
        
        if (HealingReceived > 0.0f)
        {
            PlayerStatistics.TotalHealingReceived += HealingReceived;
            bUpdated = true;
        }
    }

    if (bUpdated)
    {
        OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);
    }
}

// 자원 수집 통계 업데이트
void AHSPlayerState::UpdateResourceStatistics(int32 ResourceAmount)
{
    if (!HasAuthority() || ResourceAmount <= 0)
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    PlayerStatistics.ResourcesGathered += ResourceAmount;
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);
}

// 협동 액션 통계 업데이트
void AHSPlayerState::UpdateCoopActionStatistics(bool bSuccess)
{
    if (!HasAuthority())
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    PlayerStatistics.CoopActionsParticipated++;
    
    if (bSuccess)
    {
        PlayerStatistics.SuccessfulCoopActions++;
    }
    
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 협동 액션 참여 - %s: %s"), 
           *GetPlayerName(), bSuccess ? TEXT("성공") : TEXT("실패"));
}

// 부활 관련 통계 업데이트
void AHSPlayerState::UpdateRevivalStatistics(bool bRevived)
{
    if (!HasAuthority())
    {
        return;
    }

    FScopeLock StatisticsLock(&StatisticsMutex);
    
    if (bRevived)
    {
        PlayerStatistics.TimesRevived++;
    }
    else
    {
        PlayerStatistics.PlayersRevived++;
    }
    
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 부활 통계 업데이트 - %s: %s"), 
           *GetPlayerName(), bRevived ? TEXT("부활당함") : TEXT("부활시킴"));
}

// KDA 비율 계산
float AHSPlayerState::GetKDARate() const
{
    int32 Deaths = FMath::Max(1, PlayerStatistics.Deaths); // 0으로 나누기 방지
    return (float)(PlayerStatistics.Kills + PlayerStatistics.Assists) / Deaths;
}

// 분당 평균 데미지 계산
float AHSPlayerState::GetDamagePerMinute() const
{
    float PlayTimeMinutes = GetTotalPlayTime() / 60.0f;
    if (PlayTimeMinutes <= 0.0f)
    {
        return 0.0f;
    }
    
    return PlayerStatistics.TotalDamageDealt / PlayTimeMinutes;
}

// === 인벤토리 상태 관리 ===

// 인벤토리 상태 업데이트
void AHSPlayerState::UpdateInventoryState(const FHSPlayerInventoryState& NewInventoryState)
{
    if (HasAuthority())
    {
        InventoryState = NewInventoryState;
    }
}

// 현재 무기 타입 설정
void AHSPlayerState::SetCurrentWeapon(const FName& WeaponType)
{
    if (HasAuthority())
    {
        InventoryState.CurrentWeaponType = WeaponType;
        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 무기 변경 - %s: %s"), 
               *GetPlayerName(), *WeaponType.ToString());
    }
}

// 소모품 추가
void AHSPlayerState::AddConsumable(const FName& ItemType, int32 Amount)
{
    if (!HasAuthority() || Amount <= 0)
    {
        return;
    }

    // 기존 아이템 찾기
    FConsumableItemInfo* ExistingItem = nullptr;
    for (FConsumableItemInfo& ConsumableItem : InventoryState.Consumables)
    {
        if (ConsumableItem.ItemType == ItemType)
        {
            ExistingItem = &ConsumableItem;
            break;
        }
    }

    if (ExistingItem)
    {
        // 기존 아이템 수량 증가
        ExistingItem->Quantity += Amount;
        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 소모품 추가 - %s: %s +%d (총 %d)"), 
               *GetPlayerName(), *ItemType.ToString(), Amount, ExistingItem->Quantity);
    }
    else
    {
        // 새 아이템 추가
        InventoryState.Consumables.Add(FConsumableItemInfo(ItemType, Amount));
        UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 새 소모품 추가 - %s: %s %d개"), 
               *GetPlayerName(), *ItemType.ToString(), Amount);
    }
}

// === 성능 및 네트워크 정보 ===

// 플레이어 핑 반환
float AHSPlayerState::GetPlayerPing() const
{
    if (APlayerController* PC = GetPlayerController())
    {
        if (UNetConnection* NetConnection = PC->GetNetConnection())
        {
            return NetConnection->AvgLag * 1000.0f; // 밀리초 단위로 변환
        }
    }
    return 0.0f;
}

// 패킷 손실률 반환
float AHSPlayerState::GetPacketLossRate() const
{
    if (APlayerController* PC = GetPlayerController())
    {
        if (UNetConnection* NetConnection = PC->GetNetConnection())
        {
            // UE5.4에서는 패킷 손실률을 네트워크 드라이버에서 가져옴
            // 기본값으로 0.0f를 반환 (실제 구현은 네트워크 드라이버에서 처리)
            return 0.0f;
        }
    }
    return 0.0f;
}

// 연결 품질 반환
int32 AHSPlayerState::GetConnectionQuality() const
{
    float Ping = GetPlayerPing();
    float PacketLoss = GetPacketLossRate();

    // 연결 품질 계산 (0-4 스케일)
    if (Ping < 50.0f && PacketLoss < 0.01f)
        return 4; // 매우 좋음
    else if (Ping < 100.0f && PacketLoss < 0.02f)
        return 3; // 좋음
    else if (Ping < 200.0f && PacketLoss < 0.05f)
        return 2; // 보통
    else if (Ping < 300.0f && PacketLoss < 0.10f)
        return 1; // 나쁨
    else
        return 0; // 매우 나쁨
}

// === 유틸리티 함수들 ===

// 총 플레이 시간 반환
float AHSPlayerState::GetTotalPlayTime() const
{
    if (PlayStartTime <= 0.0f)
    {
        return 0.0f;
    }
    
    return GetWorld()->GetTimeSeconds() - PlayStartTime;
}

// 현재 생존 시간 반환
float AHSPlayerState::GetCurrentSurvivalTime() const
{
    if (PlayerStatus != EHSPlayerStatus::PS_Alive || CurrentLifeStartTime <= 0.0f)
    {
        return 0.0f;
    }
    
    return GetWorld()->GetTimeSeconds() - CurrentLifeStartTime;
}

// 플레이어 정보 문자열 반환
FString AHSPlayerState::GetPlayerInfoString() const
{
    return FString::Printf(TEXT("Player: %s | Level: %d | Class: %d | Status: %d | Team: %d | K/D/A: %d/%d/%d"),
        *GetPlayerName(),
        LevelInfo.CurrentLevel,
        (int32)PlayerClass,
        (int32)PlayerStatus,
        TeamID,
        PlayerStatistics.Kills,
        PlayerStatistics.Deaths,
        PlayerStatistics.Assists
    );
}

// === 프라이빗 함수들 ===

// 다음 레벨까지 필요한 경험치 계산
float AHSPlayerState::CalculateExperienceForNextLevel(int32 Level) const
{
    if (Level >= MaxLevel)
    {
        return 0.0f; // 최대 레벨에 도달
    }
    
    // 지수적 증가 공식: BaseExp * (ScalingFactor ^ (Level - 1))
    return BaseExperiencePerLevel * FMath::Pow(ExperienceScalingFactor, Level - 1);
}

// 레벨업 처리
void AHSPlayerState::ProcessLevelUp(int32 NewLevel)
{
    // 레벨업 시 경험치 보너스 (예시)
    float BonusExperience = NewLevel * 50.0f;
    
    // 캐릭터에게 레벨업 알림 (스탯 증가 등)
    if (APawn* ControlledPawn = GetPawn())
    {
        if (AHSCharacterBase* Character = Cast<AHSCharacterBase>(ControlledPawn))
        {
            if (UHSStatsComponent* StatsComp = Character->FindComponentByClass<UHSStatsComponent>())
            {
                // 레벨업 시 스탯 보너스 적용 (예시)
                FBuffData LevelUpBuff;
                LevelUpBuff.BuffID = FString::Printf(TEXT("LevelUp_%d"), NewLevel);
                LevelUpBuff.BuffType = EBuffType::Health;
                LevelUpBuff.Value = 10.0f;
                LevelUpBuff.Duration = -1.0f; // 영구 버프
                LevelUpBuff.bIsPercentage = false;
                StatsComp->ApplyBuff(LevelUpBuff);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 레벨업 처리 완료 - %s: Level %d"), 
           *GetPlayerName(), NewLevel);
}

// 타이머 설정
void AHSPlayerState::SetupTimers()
{
    if (!HasAuthority())
    {
        return;
    }

    FTimerManager& TimerManager = GetWorld()->GetTimerManager();

    // 생존 시간 업데이트 타이머 (더 자주 업데이트)
    TimerManager.SetTimer(SurvivalTimeUpdateTimer, this, 
                        &AHSPlayerState::UpdateSurvivalTime, 1.0f, true);

    // 통계 자동 업데이트 타이머
    TimerManager.SetTimer(StatisticsUpdateTimer, this, 
                        &AHSPlayerState::AutoUpdateStatistics, 
                        StatisticsUpdateInterval, true);

    // 네트워크 상태 체크 타이머
    TimerManager.SetTimer(NetworkStatusTimer, this, 
                        &AHSPlayerState::CheckNetworkStatus, 
                        NetworkStatusCheckInterval, true);

    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 타이머 설정 완료 - %s"), *GetPlayerName());
}

// === 타이머 콜백 함수들 ===

// 생존 시간 업데이트
void AHSPlayerState::UpdateSurvivalTime()
{
    if (PlayerStatus == EHSPlayerStatus::PS_Alive && CurrentLifeStartTime > 0.0f)
    {
        FScopeLock StatisticsLock(&StatisticsMutex);
        PlayerStatistics.SurvivalTime = GetWorld()->GetTimeSeconds() - CurrentLifeStartTime;
    }
}

// 통계 자동 업데이트
void AHSPlayerState::AutoUpdateStatistics()
{
    // 자동으로 업데이트할 통계들 처리
    // 예: 보스 전투 시간 체크, 비활성 시간 체크 등
    
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);
}

// 네트워크 상태 체크
void AHSPlayerState::CheckNetworkStatus()
{
    // 연결 상태 확인 및 품질 체크
    float Ping = GetPlayerPing();
    float PacketLoss = GetPacketLossRate();
    int32 Quality = GetConnectionQuality();

    // 연결 품질이 매우 나쁜 경우 경고
    if (Quality <= 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSPlayerState: 네트워크 연결 품질 나쁨 - %s: Ping %.1fms, Loss %.2f%%"), 
               *GetPlayerName(), Ping, PacketLoss * 100.0f);
    }
}

// === 네트워크 복제 콜백 함수들 ===

void AHSPlayerState::OnRep_PlayerStatus()
{
    OnPlayerStatusChanged.Broadcast(PlayerStatus);
    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 상태 복제됨 - %s: %d"), 
           *GetPlayerName(), (int32)PlayerStatus);
}

void AHSPlayerState::OnRep_PlayerClass()
{
    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 클래스 복제됨 - %s: %d"), 
           *GetPlayerName(), (int32)PlayerClass);
}

void AHSPlayerState::OnRep_PlayerRole()
{
    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 플레이어 역할 복제됨 - %s: %d"), 
           *GetPlayerName(), (int32)PlayerRole);
}

void AHSPlayerState::OnRep_TeamID()
{
    OnPlayerTeamChanged.Broadcast(-1, TeamID); // OldTeamID는 클라이언트에서 알기 어려우므로 -1로 설정
    UE_LOG(LogTemp, Log, TEXT("HSPlayerState: 팀 ID 복제됨 - %s: %d"), 
           *GetPlayerName(), TeamID);
}

void AHSPlayerState::OnRep_PlayerStatistics()
{
    OnPlayerStatisticsUpdated.Broadcast(PlayerStatistics);
}

void AHSPlayerState::OnRep_LevelInfo()
{
    // 레벨 정보가 복제되었을 때 처리 (UI 업데이트 등)
}

void AHSPlayerState::OnRep_InventoryState()
{
    // 인벤토리 상태가 복제되었을 때 처리
}

// === 디버그 및 로깅 함수들 ===

// 플레이어 상태 로그 출력
void AHSPlayerState::LogPlayerState() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 플레이어 상태 정보: %s ==="), *GetPlayerName());
    UE_LOG(LogTemp, Warning, TEXT("상태: %d, 클래스: %d, 역할: %d"), 
           (int32)PlayerStatus, (int32)PlayerClass, (int32)PlayerRole);
    UE_LOG(LogTemp, Warning, TEXT("팀 ID: %d"), TeamID);
    UE_LOG(LogTemp, Warning, TEXT("레벨: %d, 경험치: %.1f/%.1f"), 
           LevelInfo.CurrentLevel, LevelInfo.CurrentExperience, LevelInfo.ExperienceToNextLevel);
    UE_LOG(LogTemp, Warning, TEXT("총 플레이 시간: %.1f초"), GetTotalPlayTime());
    UE_LOG(LogTemp, Warning, TEXT("현재 생존 시간: %.1f초"), GetCurrentSurvivalTime());
}

// 플레이어 통계 로그 출력
void AHSPlayerState::LogPlayerStatistics() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 플레이어 통계: %s ==="), *GetPlayerName());
    UE_LOG(LogTemp, Warning, TEXT("K/D/A: %d/%d/%d (KDA: %.2f)"), 
           PlayerStatistics.Kills, PlayerStatistics.Deaths, PlayerStatistics.Assists, GetKDARate());
    UE_LOG(LogTemp, Warning, TEXT("데미지: %.1f (분당 %.1f)"), 
           PlayerStatistics.TotalDamageDealt, GetDamagePerMinute());
    UE_LOG(LogTemp, Warning, TEXT("힐링: %.1f"), PlayerStatistics.TotalHealingDone);
    UE_LOG(LogTemp, Warning, TEXT("자원 수집: %d"), PlayerStatistics.ResourcesGathered);
    UE_LOG(LogTemp, Warning, TEXT("협동 액션: %d/%d"), 
           PlayerStatistics.SuccessfulCoopActions, PlayerStatistics.CoopActionsParticipated);
    UE_LOG(LogTemp, Warning, TEXT("부활: %d회 받음, %d명 살림"), 
           PlayerStatistics.TimesRevived, PlayerStatistics.PlayersRevived);
}

// === 메모리 최적화 관련 ===

// 사용하지 않는 데이터 정리
void AHSPlayerState::CleanupUnusedData()
{
    // 빈 소모품 엔트리 제거 (역순으로 순회하여 안전하게 제거)
    for (int32 i = InventoryState.Consumables.Num() - 1; i >= 0; --i)
    {
        if (InventoryState.Consumables[i].Quantity <= 0)
        {
            InventoryState.Consumables.RemoveAt(i);
        }
    }
}
