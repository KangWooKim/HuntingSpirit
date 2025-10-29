#include "HSRunManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Containers/Set.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemTypes.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Math/UnrealMathUtility.h"

namespace
{
    FString BuildPlayerIdentifier(const APlayerState* PlayerState)
    {
        if (!PlayerState)
        {
            return FString();
        }

        if (PlayerState->GetUniqueId().IsValid())
        {
            return PlayerState->GetUniqueId()->ToString();
        }

        return PlayerState->GetPlayerName();
    }

    FString BuildLocalPlayerIdentifier(const ULocalPlayer* LocalPlayer)
    {
        if (!LocalPlayer)
        {
            return FString();
        }

        const FUniqueNetIdRepl NetId = LocalPlayer->GetPreferredUniqueNetId();
        if (NetId.IsValid())
        {
            return NetId->ToString();
        }

        return LocalPlayer->GetNickname();
    }
}

UHSRunManager::UHSRunManager()
{
    // 성능 최적화를 위한 초기화
    StatisticsUpdateInterval = 1.0f;
    CachedDifficultyMultiplier = 1.0f;
    CachedDifficulty = EHSRunDifficulty::Normal;
    CachedCooperationBonus = 1.0f;
    CachedCooperativeActions = -1;
}

void UHSRunManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // 런 매니저 초기화
    CurrentRun = FHSRunData();
    CurrentRun.State = EHSRunState::None;
    
    UE_LOG(LogTemp, Log, TEXT("HSRunManager 초기화 완료"));
}

void UHSRunManager::Deinitialize()
{
    // 활성화된 타이머들 정리
    if (UWorld* World = GetWorld())
    {
        if (RunUpdateTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(RunUpdateTimerHandle);
        }
        if (StatisticsTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(StatisticsTimerHandle);
        }
    }
    
    // 진행 중인 런이 있다면 안전하게 종료
    if (IsRunActive())
    {
        EndCurrentRun(EHSRunResult::Abandoned);
    }
    
    Super::Deinitialize();
    
    UE_LOG(LogTemp, Log, TEXT("HSRunManager 정리 완료"));
}

bool UHSRunManager::StartNewRun(const FHSRunConfiguration& Configuration)
{
    // 이미 활성화된 런이 있는지 확인
    if (IsRunActive())
    {
        UE_LOG(LogTemp, Warning, TEXT("런 시작 실패: 이미 활성화된 런이 존재합니다"));
        return false;
    }
    
    // 새로운 런 데이터 초기화
    InitializeRunData(Configuration);
    
    // 런 상태를 준비 상태로 변경
    ChangeRunState(EHSRunState::Preparing);
    
    // 월드 시드가 0이면 랜덤 시드 생성
    if (CurrentRun.Configuration.WorldSeed == 0)
    {
        CurrentRun.Configuration.WorldSeed = FMath::RandRange(1, 999999);
    }
    
    // 타이머 시작
    if (UWorld* World = GetWorld())
    {
        // 런 업데이트 타이머 (매 프레임)
        World->GetTimerManager().SetTimer(RunUpdateTimerHandle, this, &UHSRunManager::UpdateRunProgress, 0.1f, true);
        
        // 통계 업데이트 타이머
        World->GetTimerManager().SetTimer(StatisticsTimerHandle, this, &UHSRunManager::UpdateStatistics, StatisticsUpdateInterval, true);
    }
    
    // 런 상태를 활성화로 변경
    ChangeRunState(EHSRunState::Active);
    
    UE_LOG(LogTemp, Log, TEXT("새로운 런 시작됨 - ID: %s, 난이도: %d, 시드: %d"), 
           *CurrentRun.RunID, (int32)Configuration.Difficulty, Configuration.WorldSeed);
    
    return true;
}

void UHSRunManager::EndCurrentRun(EHSRunResult Result)
{
    if (!IsRunActive())
    {
        UE_LOG(LogTemp, Warning, TEXT("런 종료 실패: 활성화된 런이 없습니다"));
        return;
    }
    
    // 런 결과 설정
    CurrentRun.Result = Result;
    CurrentRun.EndTime = FDateTime::Now();
    CurrentRun.ElapsedTime = (CurrentRun.EndTime - CurrentRun.StartTime).GetTotalSeconds();
    CurrentRun.Statistics.RunDuration = CurrentRun.ElapsedTime;
    
    // 보상 계산
    CurrentRun.Rewards = CalculateRunRewards();
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        if (RunUpdateTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(RunUpdateTimerHandle);
        }
        if (StatisticsTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(StatisticsTimerHandle);
        }
    }
    
    // 런 상태 변경
    EHSRunState NewState = (Result == EHSRunResult::Victory) ? EHSRunState::Completed : EHSRunState::Failed;
    ChangeRunState(NewState);
    
    // 런 완료 이벤트 발생
    OnRunCompleted.Broadcast(Result, CurrentRun.Rewards);
    
    UE_LOG(LogTemp, Log, TEXT("런 종료됨 - 결과: %d, 지속시간: %.2f초, 메타소울: %d"), 
           (int32)Result, CurrentRun.ElapsedTime, CurrentRun.Rewards.MetaSouls);
}

void UHSRunManager::PauseCurrentRun()
{
    if (CurrentRun.State == EHSRunState::Active)
    {
        ChangeRunState(EHSRunState::Paused);
        
        // 타이머 일시정지
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().PauseTimer(RunUpdateTimerHandle);
            World->GetTimerManager().PauseTimer(StatisticsTimerHandle);
        }
        
        UE_LOG(LogTemp, Log, TEXT("런 일시정지됨"));
    }
}

void UHSRunManager::ResumeCurrentRun()
{
    if (CurrentRun.State == EHSRunState::Paused)
    {
        ChangeRunState(EHSRunState::Active);
        
        // 타이머 재개
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().UnPauseTimer(RunUpdateTimerHandle);
            World->GetTimerManager().UnPauseTimer(StatisticsTimerHandle);
        }
        
        UE_LOG(LogTemp, Log, TEXT("런 재개됨"));
    }
}

void UHSRunManager::AbandonCurrentRun()
{
    if (IsRunActive())
    {
        EndCurrentRun(EHSRunResult::Abandoned);
        UE_LOG(LogTemp, Log, TEXT("런 포기됨"));
    }
}

EHSRunState UHSRunManager::GetCurrentRunState() const
{
    return CurrentRun.State;
}

const FHSRunData& UHSRunManager::GetCurrentRunData() const
{
    return CurrentRun;
}

bool UHSRunManager::IsRunActive() const
{
    return CurrentRun.State == EHSRunState::Active || CurrentRun.State == EHSRunState::Paused;
}

float UHSRunManager::GetRunProgress() const
{
    if (!IsRunActive() || CurrentRun.Configuration.TimeLimit <= 0.0f)
    {
        return 0.0f;
    }
    
    float ElapsedTime = (FDateTime::Now() - CurrentRun.StartTime).GetTotalSeconds();
    return FMath::Clamp(ElapsedTime / CurrentRun.Configuration.TimeLimit, 0.0f, 1.0f);
}

void UHSRunManager::AddEnemyKill(int32 Count)
{
    if (IsRunActive() && Count > 0)
    {
        CurrentRun.Statistics.EnemiesKilled += Count;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
        
        // 캐시 무효화
        CachedCooperativeActions = -1;
    }
}

void UHSRunManager::AddBossKill(float KillTime)
{
    if (IsRunActive())
    {
        CurrentRun.Statistics.BossesDefeated++;
        
        // 최고 기록 업데이트
        if (CurrentRun.Statistics.BestBossKillTime == 0.0f || KillTime < CurrentRun.Statistics.BestBossKillTime)
        {
            CurrentRun.Statistics.BestBossKillTime = KillTime;
        }
        
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
        
        UE_LOG(LogTemp, Log, TEXT("보스 처치됨 - 시간: %.2f초"), KillTime);
    }
}

void UHSRunManager::AddPlayerDeath()
{
    if (IsRunActive())
    {
        CurrentRun.Statistics.DeathCount++;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
        
        UE_LOG(LogTemp, Log, TEXT("플레이어 사망 - 총 사망 수: %d"), CurrentRun.Statistics.DeathCount);
    }
}

void UHSRunManager::AddPlayerRevive()
{
    if (IsRunActive())
    {
        CurrentRun.Statistics.ReviveCount++;
        CurrentRun.Statistics.PlayersRevived++;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
        
        UE_LOG(LogTemp, Log, TEXT("플레이어 부활 - 총 부활 수: %d"), CurrentRun.Statistics.ReviveCount);
    }
}

void UHSRunManager::AddDamage(float DamageDealt, float DamageTaken)
{
    if (IsRunActive())
    {
        if (DamageDealt > 0.0f)
        {
            CurrentRun.Statistics.TotalDamageDealt += DamageDealt;
        }
        if (DamageTaken > 0.0f)
        {
            CurrentRun.Statistics.TotalDamageTaken += DamageTaken;
        }
        
        // 매번 브로드캐스트하지 않고 배치로 처리 (성능 최적화)
        static int32 DamageUpdateCounter = 0;
        DamageUpdateCounter++;
        if (DamageUpdateCounter >= 10) // 10번마다 업데이트
        {
            OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
            DamageUpdateCounter = 0;
        }
    }
}

void UHSRunManager::AddItemCollection(int32 Count)
{
    if (IsRunActive() && Count > 0)
    {
        CurrentRun.Statistics.ItemsCollected += Count;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
    }
}

void UHSRunManager::AddResourceGathering(int32 Count)
{
    if (IsRunActive() && Count > 0)
    {
        CurrentRun.Statistics.ResourcesGathered += Count;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
    }
}

void UHSRunManager::AddCooperativeAction()
{
    if (IsRunActive())
    {
        CurrentRun.Statistics.CooperativeActions++;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
        
        // 캐시 무효화
        CachedCooperativeActions = -1;
    }
}

void UHSRunManager::AddComboAttack()
{
    if (IsRunActive())
    {
        CurrentRun.Statistics.ComboAttacks++;
        OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
    }
}

FHSRunRewards UHSRunManager::CalculateRunRewards() const
{
    FHSRunRewards Rewards;
    
    if (CurrentRun.State == EHSRunState::None)
    {
        return Rewards;
    }
    
    // 기본 보상 계산
    const FHSRunStatistics& Stats = CurrentRun.Statistics;
    
    // 메타소울 계산 (적 처치 + 보스 처치 + 생존 보너스)
    Rewards.MetaSouls = Stats.EnemiesKilled * 2 + Stats.BossesDefeated * 50;
    
    // 생존 보너스 (사망 횟수에 따른 페널티)
    float SurvivalBonus = FMath::Max(0.5f, 1.0f - (Stats.DeathCount * 0.1f));
    Rewards.MetaSouls = FMath::RoundToInt(Rewards.MetaSouls * SurvivalBonus);
    
    // 에센스 포인트 계산 (협동 행동과 콤보 공격)
    Rewards.EssencePoints = Stats.CooperativeActions * 5 + Stats.ComboAttacks * 3;
    
    // 경험치 계산
    Rewards.BaseExperience = Stats.EnemiesKilled * 10 + Stats.BossesDefeated * 100;
    
    // 난이도 배율 적용
    float DifficultyMultiplier = GetDifficultyMultiplier(CurrentRun.Configuration.Difficulty);
    float CooperationBonus = GetCooperationBonus();
    float TotalMultiplier = CalculateRewardMultiplier() * DifficultyMultiplier * CooperationBonus;
    
    // 최종 보상에 배율 적용
    Rewards.MetaSouls = FMath::RoundToInt(Rewards.MetaSouls * TotalMultiplier);
    Rewards.EssencePoints = FMath::RoundToInt(Rewards.EssencePoints * TotalMultiplier);
    Rewards.BaseExperience = FMath::RoundToInt(Rewards.BaseExperience * TotalMultiplier);
    
    // 보너스 경험치 (빠른 클리어 보너스)
    if (CurrentRun.Result == EHSRunResult::Victory && CurrentRun.Statistics.RunDuration > 0.0f)
    {
        float TimeBonus = FMath::Max(0.0f, 1.0f - (CurrentRun.Statistics.RunDuration / CurrentRun.Configuration.TimeLimit));
        Rewards.BonusExperience = FMath::RoundToInt(Rewards.BaseExperience * TimeBonus * 0.5f);
    }
    
    // 언락 포인트 (보스 처치 시에만)
    if (Stats.BossesDefeated > 0)
    {
        Rewards.UnlockPoints = Stats.BossesDefeated;
    }
    
    return Rewards;
}

float UHSRunManager::GetDifficultyMultiplier(EHSRunDifficulty Difficulty) const
{
    // 캐시된 값 확인 (성능 최적화)
    if (CachedDifficulty == Difficulty)
    {
        return CachedDifficultyMultiplier;
    }
    
    float Multiplier = 1.0f;
    
    switch (Difficulty)
    {
        case EHSRunDifficulty::Easy:
            Multiplier = 0.8f;
            break;
        case EHSRunDifficulty::Normal:
            Multiplier = 1.0f;
            break;
        case EHSRunDifficulty::Hard:
            Multiplier = 1.5f;
            break;
        case EHSRunDifficulty::Nightmare:
            Multiplier = 2.0f;
            break;
        case EHSRunDifficulty::Hell:
            Multiplier = 3.0f;
            break;
        default:
            Multiplier = 1.0f;
            break;
    }
    
    // 캐시 업데이트
    CachedDifficulty = Difficulty;
    CachedDifficultyMultiplier = Multiplier;
    
    return Multiplier;
}

float UHSRunManager::GetCooperationBonus() const
{
    // 캐시된 값 확인 (성능 최적화)
    if (CachedCooperativeActions == CurrentRun.Statistics.CooperativeActions)
    {
        return CachedCooperationBonus;
    }
    
    // 협동 행동 수에 따른 보너스 계산
    int32 CoopActions = CurrentRun.Statistics.CooperativeActions;
    float Bonus = 1.0f + (CoopActions * 0.05f); // 협동 행동 당 5% 보너스
    Bonus = FMath::Min(Bonus, 2.0f); // 최대 200%까지만
    
    // 캐시 업데이트
    CachedCooperativeActions = CoopActions;
    CachedCooperationBonus = Bonus;
    
    return Bonus;
}

void UHSRunManager::ChangeRunState(EHSRunState NewState)
{
    if (CurrentRun.State != NewState)
    {
        EHSRunState OldState = CurrentRun.State;
        CurrentRun.State = NewState;
        
        // 상태 변화에 따른 특별 처리
        if (NewState == EHSRunState::Active && OldState == EHSRunState::Preparing)
        {
            CurrentRun.StartTime = FDateTime::Now();
        }
        
        OnRunStateChanged.Broadcast(NewState);
        
        UE_LOG(LogTemp, Log, TEXT("런 상태 변경: %d -> %d"), (int32)OldState, (int32)NewState);
    }
}

void UHSRunManager::UpdateRunProgress()
{
    if (!IsRunActive())
    {
        return;
    }
    
    // 시간 제한 확인
    if (CurrentRun.Configuration.TimeLimit > 0.0f)
    {
        float ElapsedTime = (FDateTime::Now() - CurrentRun.StartTime).GetTotalSeconds();
        if (ElapsedTime >= CurrentRun.Configuration.TimeLimit)
        {
            EndCurrentRun(EHSRunResult::Timeout);
            return;
        }
    }

    if (CurrentRun.ParticipantIDs.Num() > 0)
    {
        TSet<FString> ConnectedPlayerIds;

        if (UWorld* World = GetWorld())
        {
            if (AGameStateBase* GameState = World->GetGameState())
            {
                for (APlayerState* PlayerState : GameState->PlayerArray)
                {
                    const FString Identifier = BuildPlayerIdentifier(PlayerState);
                    if (!Identifier.IsEmpty())
                    {
                        ConnectedPlayerIds.Add(Identifier);
                    }
                }
            }
        }

        if (ConnectedPlayerIds.Num() == 0)
        {
            if (UGameInstance* GameInstance = GetGameInstance())
            {
                for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
                {
                    const FString Identifier = BuildLocalPlayerIdentifier(LocalPlayer);
                    if (!Identifier.IsEmpty())
                    {
                        ConnectedPlayerIds.Add(Identifier);
                    }
                }
            }
        }

        if (ConnectedPlayerIds.Num() > 0)
        {
            for (const FString& Identifier : ConnectedPlayerIds)
            {
                if (!CurrentRun.ParticipantIDs.Contains(Identifier))
                {
                    CurrentRun.ParticipantIDs.Add(Identifier);
                }
            }

            for (const FString& ParticipantId : CurrentRun.ParticipantIDs)
            {
                if (ParticipantId.Equals(TEXT("LocalPlayer")))
                {
                    continue;
                }

                if (!ConnectedPlayerIds.Contains(ParticipantId))
                {
                    UE_LOG(LogTemp, Warning, TEXT("런 참가자 연결 끊김 감지: %s"), *ParticipantId);
                    EndCurrentRun(EHSRunResult::Disconnection);
                    return;
                }
            }
        }
    }
}

void UHSRunManager::UpdateStatistics()
{
    if (!IsRunActive())
    {
        return;
    }
    
    // 현재 런 지속시간 업데이트
    CurrentRun.Statistics.RunDuration = (FDateTime::Now() - CurrentRun.StartTime).GetTotalSeconds();
    
    // 주기적인 통계 업데이트 (성능 최적화를 위해 배치 처리)
    OnRunStatisticUpdated.Broadcast(CurrentRun.Statistics);
}

void UHSRunManager::InitializeRunData(const FHSRunConfiguration& Configuration)
{
    CurrentRun = FHSRunData();
    CurrentRun.Configuration = Configuration;
    CurrentRun.State = EHSRunState::None;
    CurrentRun.Result = EHSRunResult::None;
    CurrentRun.StartTime = FDateTime::Now();
    CurrentRun.EndTime = FDateTime::MinValue();
    CurrentRun.ElapsedTime = 0.0f;
    
    CurrentRun.ParticipantIDs.Empty();

    TSet<FString> UniqueParticipants;

    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            for (APlayerState* PlayerState : GameState->PlayerArray)
            {
                const FString Identifier = BuildPlayerIdentifier(PlayerState);
                if (!Identifier.IsEmpty())
                {
                    UniqueParticipants.Add(Identifier);
                }
            }
        }
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
        {
            const FString Identifier = BuildLocalPlayerIdentifier(LocalPlayer);
            if (!Identifier.IsEmpty())
            {
                UniqueParticipants.Add(Identifier);
            }
        }
    }

    for (const FString& ParticipantId : UniqueParticipants)
    {
        CurrentRun.ParticipantIDs.Add(ParticipantId);
    }

    if (CurrentRun.ParticipantIDs.Num() == 0)
    {
        CurrentRun.ParticipantIDs.Add(TEXT("LocalPlayer"));
    }
    
    // 캐시 초기화
    CachedDifficultyMultiplier = 1.0f;
    CachedDifficulty = Configuration.Difficulty;
    CachedCooperationBonus = 1.0f;
    CachedCooperativeActions = -1;
}

float UHSRunManager::CalculateRewardMultiplier() const
{
    float Multiplier = 1.0f;
    
    const FHSRunStatistics& Stats = CurrentRun.Statistics;
    
    // 퍼펙트 런 보너스 (사망 없이 클리어)
    if (Stats.DeathCount == 0 && CurrentRun.Result == EHSRunResult::Victory)
    {
        Multiplier += 0.5f; // 50% 보너스
    }
    
    // 스피드런 보너스 (제한시간의 50% 이내에 클리어)
    if (CurrentRun.Configuration.TimeLimit > 0.0f && Stats.RunDuration > 0.0f)
    {
        float TimeRatio = Stats.RunDuration / CurrentRun.Configuration.TimeLimit;
        if (TimeRatio <= 0.5f)
        {
            Multiplier += 0.3f; // 30% 보너스
        }
    }
    
    // 올킬 보너스 (매우 많은 적 처치)
    if (Stats.EnemiesKilled >= 100)
    {
        Multiplier += 0.2f; // 20% 보너스
    }
    
    return Multiplier;
}
