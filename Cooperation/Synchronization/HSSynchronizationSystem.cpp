// HuntingSpirit 게임의 고급 동기화 시스템 구현
// 지연 보상, 예측, 롤백, 네트워크 상태 동기화 실제 로직
// 작성자: KangWooKim (https://github.com/KangWooKim)

#include "HSSynchronizationSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFilemanager.h"
#include "Math/UnrealMathUtility.h"
#include "../HSTeamManager.h"

UHSSynchronizationSystem::UHSSynchronizationSystem()
{
    // 기본 설정 초기화
    NextSequenceNumber = 1;
    CurrentFrameNumber = 0;
    MaxRollbackFrames = 300;    // 5초간 롤백 가능 (60FPS 기준)
    
    TickRate = 60.0f;           // 60Hz 동기화
    PredictionTimeWindow = 0.5f; // 500ms 예측 윈도우
    RollbackTimeWindow = 5.0f;   // 5초 롤백 윈도우
    MaxPacketQueueSize = 1000;   // 최대 1000개 패킷 큐
    BandwidthLimit = 1000000.0f; // 1MB/s 대역폭 제한
    
    // 풀링 초기화
    PacketPool.Reserve(100);
    StatePool.Reserve(50);
    RollbackPool.Reserve(300);
    
    // 캐시 초기화
    LastCacheUpdate = FDateTime::MinValue();
}

void UHSSynchronizationSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 고급 동기화 시스템 초기화 시작"));
    
    // 동기화 상태 초기화
    TArray<EHSSyncType> SyncTypes = {
        EHSSyncType::PlayerState, EHSSyncType::WorldState, EHSSyncType::CombatState,
        EHSSyncType::InventoryState, EHSSyncType::QuestState, EHSSyncType::TeamState,
        EHSSyncType::RewardState
    };
    
    for (EHSSyncType SyncType : SyncTypes)
    {
        SyncStatusMap.Add(SyncType, EHSSyncStatus::Synced);
        SyncPriorityMap.Add(SyncType, EHSSyncPriority::Normal);
    }
    
    // 타이머 설정
    if (UWorld* World = GetWorld())
    {
        // 동기화 틱 타이머 (60Hz)
        World->GetTimerManager().SetTimer(
            SyncTickTimer,
            [this]()
            {
                this->TickSynchronization(1.0f / TickRate);
                this->ProcessSyncTick();
            },
            1.0f / TickRate,
            true
        );
        
        // 지연 보상 처리 타이머 (1초마다)
        World->GetTimerManager().SetTimer(
            DelayedRewardTimer,
            this,
            &UHSSynchronizationSystem::ProcessDelayedRewards,
            1.0f,
            true
        );
        
        // 통계 업데이트 타이머 (5초마다)
        World->GetTimerManager().SetTimer(
            StatisticsTimer,
            this,
            &UHSSynchronizationSystem::UpdateStatistics,
            5.0f,
            true
        );
        
        // 정리 타이머 (30초마다)
        World->GetTimerManager().SetTimer(
            CleanupTimer,
            this,
            &UHSSynchronizationSystem::PerformCleanup,
            30.0f,
            true
        );
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 고급 동기화 시스템 초기화 완료"));
}

void UHSSynchronizationSystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 고급 동기화 시스템 정리 시작"));
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SyncTickTimer);
        World->GetTimerManager().ClearTimer(DelayedRewardTimer);
        World->GetTimerManager().ClearTimer(StatisticsTimer);
        World->GetTimerManager().ClearTimer(CleanupTimer);
    }
    
    // 데이터 정리
    SyncStatusMap.Empty();
    SyncPriorityMap.Empty();
    OutgoingPackets.Empty();
    IncomingPackets.Empty();
    PredictionStates.Empty();
    PredictionHistory.Empty();
    RollbackHistory.Empty();
    DelayedRewards.Empty();
    PlayerLatencies.Empty();
    LatencyHistory.Empty();
    
    // 캐시 정리
    PredictionCache.Empty();
    StatusCache.Empty();
    
    // 풀링 정리
    PacketPool.Empty();
    StatePool.Empty();
    RollbackPool.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 고급 동기화 시스템 정리 완료"));
    
    Super::Deinitialize();
}

void UHSSynchronizationSystem::TickSynchronization(float DeltaTime)
{
    // 현업 최적화: 프레임 번호 증가
    CurrentFrameNumber++;
    
    // 예측 상태 업데이트
    UpdatePredictions(DeltaTime);
    
    // 패킷 큐 처리
    ProcessPacketQueue();
}

// === 동기화 패킷 관리 구현 ===

bool UHSSynchronizationSystem::SendSyncPacket(EHSSyncType SyncType, const TArray<uint8>& Data, EHSSyncPriority Priority, bool bReliable)
{
    if (Data.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 빈 데이터로 패킷을 전송할 수 없습니다"));
        return false;
    }
    
    // 대역폭 제한 확인
    if (GetBandwidthUsage() > BandwidthLimit)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 대역폭 제한 초과로 패킷 전송 지연"));
        // 우선순위가 높은 패킷만 전송
        if (Priority < EHSSyncPriority::High)
        {
            return false;
        }
    }
    
    // 현업 최적화: 오브젝트 풀링 사용
    FHSSyncPacket* Packet = nullptr;
    if (PacketPool.Num() > 0)
    {
        FHSSyncPacket TempPacket = PacketPool.Pop();
        Packet = &TempPacket;
        *Packet = FHSSyncPacket();
        OutgoingPackets.Add(*Packet);
        Packet = &OutgoingPackets.Last();
    }
    else
    {
        OutgoingPackets.Add(FHSSyncPacket());
        Packet = &OutgoingPackets.Last();
    }
    
    // 패킷 정보 설정
    Packet->PacketID = GeneratePacketID();
    Packet->SyncType = SyncType;
    Packet->Priority = Priority;
    Packet->Data = Data;
    Packet->Timestamp = FDateTime::Now();
    Packet->SequenceNumber = NextSequenceNumber++;
    Packet->bReliable = bReliable;
    
    // 현재 플레이어 ID 설정
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                Packet->SourcePlayerID = PS->GetPlayerId();
            }
        }
    }
    
    // 동기화 상태 업데이트
    SyncStatusMap.Add(SyncType, EHSSyncStatus::Syncing);
    
    // 통계 업데이트
    SyncStats.PacketsSent++;
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSSynchronizationSystem: 동기화 패킷 전송됨 - ID: %s, Type: %d, Size: %d"), 
           *Packet->PacketID, (int32)SyncType, Data.Num());
    
    return true;
}

void UHSSynchronizationSystem::ReceiveSyncPacket(const FHSSyncPacket& Packet)
{
    if (Packet.Data.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 빈 패킷 수신됨"));
        return;
    }
    
    // 패킷 유효성 검증
    if (Packet.SourcePlayerID < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 잘못된 소스 플레이어 ID: %d"), Packet.SourcePlayerID);
        return;
    }
    
    // 중복 패킷 확인
    for (const FHSSyncPacket& ExistingPacket : IncomingPackets)
    {
        if (ExistingPacket.PacketID == Packet.PacketID)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("HSSynchronizationSystem: 중복 패킷 무시됨 - ID: %s"), *Packet.PacketID);
            return;
        }
    }
    
    // 충돌 감지
    bool bConflict = DetectSyncConflict(Packet);
    EHSSyncStatus Status = bConflict ? EHSSyncStatus::Conflicted : EHSSyncStatus::Synced;
    
    if (bConflict)
    {
        // 충돌 해결 시도
        ResolveConflict(Packet.SyncType, Packet.SourcePlayerID, Packet.Data);
        SyncStats.SyncConflicts++;
    }
    
    // 수신 큐에 추가
    IncomingPackets.Add(Packet);
    
    // 큐 크기 제한
    if (IncomingPackets.Num() > MaxPacketQueueSize)
    {
        IncomingPackets.RemoveAt(0);
    }
    
    // 레이턴시 계산
    FDateTime CurrentTime = FDateTime::Now();
    float Latency = (CurrentTime - Packet.Timestamp).GetTotalMilliseconds();
    PlayerLatencies.Add(Packet.SourcePlayerID, Latency);
    
    // 동기화 상태 업데이트
    SyncStatusMap.Add(Packet.SyncType, Status);
    
    // 통계 업데이트
    SyncStats.PacketsReceived++;
    
    // 델리게이트 호출
    OnSyncPacketReceived.Broadcast(Packet, Status);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSSynchronizationSystem: 동기화 패킷 수신됨 - ID: %s, Type: %d, Latency: %.2fms"), 
           *Packet.PacketID, (int32)Packet.SyncType, Latency);
}

void UHSSynchronizationSystem::SetSyncPriority(EHSSyncType SyncType, EHSSyncPriority Priority)
{
    SyncPriorityMap.Add(SyncType, Priority);
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 동기화 우선순위 설정됨 - Type: %d, Priority: %d"), 
           (int32)SyncType, (int32)Priority);
}

EHSSyncStatus UHSSynchronizationSystem::GetSyncStatus(EHSSyncType SyncType) const
{
    // 현업 최적화: 캐시 확인
    if (const EHSSyncStatus* CachedStatus = StatusCache.Find(SyncType))
    {
        FDateTime CurrentTime = FDateTime::Now();
        if ((CurrentTime - LastCacheUpdate).GetTotalSeconds() < 1.0f)
        {
            return *CachedStatus;
        }
    }
    
    const EHSSyncStatus* Status = SyncStatusMap.Find(SyncType);
    EHSSyncStatus Result = Status ? *Status : EHSSyncStatus::None;
    
    // 캐시 업데이트 (mutable 변수 직접 수정)
    StatusCache.Add(SyncType, Result);
    LastCacheUpdate = FDateTime::Now();
    
    return Result;
}

// === 예측 시스템 구현 ===

FString UHSSynchronizationSystem::StartPrediction(const FHSPredictionState& InitialState, EHSPredictionType PredictionType)
{
    FString StateID = GenerateStateID();
    
    FHSPredictionState State = InitialState;
    State.StateID = StateID;
    State.PredictionType = PredictionType;
    State.PredictionTime = FDateTime::Now();
    
    PredictionStates.Add(StateID, State);
    
    // 예측 히스토리 초기화
    FHSPredictionHistoryArray HistoryArray;
    HistoryArray.AddPrediction(State);
    PredictionHistory.Add(StateID, HistoryArray);
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 예측 시작됨 - StateID: %s, Type: %d"), 
           *StateID, (int32)PredictionType);
    
    return StateID;
}

bool UHSSynchronizationSystem::UpdatePrediction(const FString& StateID, const FHSPredictionState& NewState)
{
    FHSPredictionState* ExistingState = PredictionStates.Find(StateID);
    if (!ExistingState)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 예측 상태를 찾을 수 없음 - StateID: %s"), *StateID);
        return false;
    }
    
    // 상태 업데이트
    *ExistingState = NewState;
    ExistingState->StateID = StateID; // ID 유지
    ExistingState->PredictionTime = FDateTime::Now();
    
    // 히스토리에 추가
    FHSPredictionHistoryArray* History = PredictionHistory.Find(StateID);
    if (History)
    {
        History->AddPrediction(NewState);
        
        // 히스토리 크기 제한 (최근 100개)
        if (History->GetCount() > 100)
        {
            // 가장 오래된 예측 제거
            if (History->PredictionStates.Num() > 0)
            {
                History->PredictionStates.RemoveAt(0);
            }
        }
    }
    
    // 캐시 무효화
    PredictionCache.Remove(StateID);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSSynchronizationSystem: 예측 상태 업데이트됨 - StateID: %s"), *StateID);
    
    return true;
}

FHSPredictionState UHSSynchronizationSystem::GetPredictedState(const FString& StateID, float FutureTime) const
{
    // 현업 최적화: 캐시 확인
    FString CacheKey = FString::Printf(TEXT("%s_%.3f"), *StateID, FutureTime);
    if (const FHSPredictionState* CachedState = PredictionCache.Find(CacheKey))
    {
        FDateTime CurrentTime = FDateTime::Now();
        if ((CurrentTime - LastCacheUpdate).GetTotalSeconds() < 0.1f) // 100ms 캐시
        {
            return *CachedState;
        }
    }
    
    const FHSPredictionState* CurrentState = PredictionStates.Find(StateID);
    if (!CurrentState)
    {
        return FHSPredictionState();
    }
    
    // 미래 시간이 0이면 현재 상태 반환
    if (FutureTime <= 0.0f)
    {
        return *CurrentState;
    }
    
    // 예측 타입에 따른 미래 상태 계산
    FHSPredictionState PredictedState = *CurrentState;
    
    switch (CurrentState->PredictionType)
    {
        case EHSPredictionType::Linear:
            PredictedState = PredictLinear(*CurrentState, FutureTime);
            break;
            
        case EHSPredictionType::Quadratic:
            PredictedState = PredictQuadratic(*CurrentState, FutureTime);
            break;
            
        case EHSPredictionType::Physics:
            PredictedState = PredictPhysics(*CurrentState, FutureTime);
            break;
            
        default:
            // 기본적으로 선형 예측 사용
            PredictedState = PredictLinear(*CurrentState, FutureTime);
            break;
    }
    
    PredictedState.StateID = StateID;
    PredictedState.PredictionTime = FDateTime::Now();
    
    // 캐시에 저장 (mutable 변수 직접 수정)
    PredictionCache.Add(CacheKey, PredictedState);
    
    return PredictedState;
}

void UHSSynchronizationSystem::CorrectPrediction(const FString& StateID, const FHSPredictionState& AuthoritativeState)
{
    FHSPredictionState* CurrentState = PredictionStates.Find(StateID);
    if (!CurrentState)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 보정할 예측 상태를 찾을 수 없음 - StateID: %s"), *StateID);
        return;
    }
    
    // 오차 계산
    float PositionError = FVector::Dist(CurrentState->Position, AuthoritativeState.Position);
    float VelocityError = FVector::Dist(CurrentState->Velocity, AuthoritativeState.Velocity);
    float TotalError = PositionError + VelocityError;
    
    // 상태 보정
    *CurrentState = AuthoritativeState;
    CurrentState->StateID = StateID;
    CurrentState->PredictionTime = FDateTime::Now();
    
    // 신뢰도 조정 (오차가 클수록 신뢰도 감소)
    float ErrorFactor = FMath::Clamp(TotalError / 1000.0f, 0.0f, 1.0f); // 1000 유닛 기준
    CurrentState->Confidence = FMath::Max(0.1f, CurrentState->Confidence - ErrorFactor * 0.2f);
    
    // 캐시 무효화
    PredictionCache.Empty();
    
    // 델리게이트 호출
    OnPredictionCorrected.Broadcast(StateID, TotalError);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSSynchronizationSystem: 예측 보정됨 - StateID: %s, Error: %.2f"), 
           *StateID, TotalError);
}

bool UHSSynchronizationSystem::StopPrediction(const FString& StateID)
{
    bool bRemoved = PredictionStates.Remove(StateID) > 0;
    PredictionHistory.Remove(StateID);
    
    // 캐시에서도 제거
    TArray<FString> KeysToRemove;
    for (const auto& CachePair : PredictionCache)
    {
        if (CachePair.Key.StartsWith(StateID))
        {
            KeysToRemove.Add(CachePair.Key);
        }
    }
    
    for (const FString& Key : KeysToRemove)
    {
        PredictionCache.Remove(Key);
    }
    
    if (bRemoved)
    {
        UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 예측 중지됨 - StateID: %s"), *StateID);
    }
    
    return bRemoved;
}

// === 롤백 시스템 구현 ===

FString UHSSynchronizationSystem::SaveStateSnapshot(const TArray<uint8>& StateData, int32 FrameNumber)
{
    FString StateID = GenerateStateID();
    
    // 현업 최적화: 오브젝트 풀링 사용
    FHSRollbackState* State = nullptr;
    if (RollbackPool.Num() > 0)
    {
        FHSRollbackState TempState = RollbackPool.Pop();
        State = &TempState;
        *State = FHSRollbackState();
        RollbackHistory.Add(*State);
        State = &RollbackHistory.Last();
    }
    else
    {
        RollbackHistory.Add(FHSRollbackState());
        State = &RollbackHistory.Last();
    }
    
    State->StateID = StateID;
    State->StateData = StateData;
    State->FrameNumber = FrameNumber;
    State->StateTime = FDateTime::Now();
    
    // 현재 프레임 번호로 기본값 설정
    if (FrameNumber == 0)
    {
        State->FrameNumber = CurrentFrameNumber;
    }
    
    // 롤백 히스토리 크기 제한
    if (RollbackHistory.Num() > MaxRollbackFrames)
    {
        // 가장 오래된 상태를 풀로 반환
        RollbackPool.Add(RollbackHistory[0]);
        RollbackHistory.RemoveAt(0);
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSSynchronizationSystem: 상태 스냅샷 저장됨 - StateID: %s, Frame: %d"), 
           *StateID, State->FrameNumber);
    
    return StateID;
}

bool UHSSynchronizationSystem::RollbackToState(const FString& StateID)
{
    const FHSRollbackState* TargetState = nullptr;
    
    for (const FHSRollbackState& State : RollbackHistory)
    {
        if (State.StateID == StateID)
        {
            TargetState = &State;
            break;
        }
    }
    
    if (!TargetState)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 롤백할 상태를 찾을 수 없음 - StateID: %s"), *StateID);
        return false;
    }
    
    // 롤백 실행 (실제 구현에서는 게임 상태 복원)
    int32 FramesRolledBack = CurrentFrameNumber - TargetState->FrameNumber;
    CurrentFrameNumber = TargetState->FrameNumber;
    
    // 롤백 이후의 상태들 제거
    RollbackHistory.RemoveAll([TargetState](const FHSRollbackState& State)
    {
        return State.FrameNumber > TargetState->FrameNumber;
    });
    
    // 통계 업데이트
    SyncStats.RollbacksPerformed++;
    
    // 델리게이트 호출
    OnRollbackPerformed.Broadcast(StateID, FramesRolledBack);
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 상태 롤백 실행됨 - StateID: %s, Frames: %d"), 
           *StateID, FramesRolledBack);
    
    return true;
}

bool UHSSynchronizationSystem::RollbackToFrame(int32 TargetFrame)
{
    if (TargetFrame > CurrentFrameNumber)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 미래 프레임으로 롤백할 수 없음 - Target: %d, Current: %d"), 
               TargetFrame, CurrentFrameNumber);
        return false;
    }
    
    // 가장 가까운 프레임 찾기
    const FHSRollbackState* BestState = nullptr;
    int32 BestFrameDiff = INT32_MAX;
    
    for (const FHSRollbackState& State : RollbackHistory)
    {
        if (State.FrameNumber <= TargetFrame)
        {
            int32 FrameDiff = TargetFrame - State.FrameNumber;
            if (FrameDiff < BestFrameDiff)
            {
                BestFrameDiff = FrameDiff;
                BestState = &State;
            }
        }
    }
    
    if (!BestState)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 롤백할 프레임을 찾을 수 없음 - Frame: %d"), TargetFrame);
        return false;
    }
    
    return RollbackToState(BestState->StateID);
}

TArray<FHSRollbackState> UHSSynchronizationSystem::GetRollbackHistory(int32 MaxFrames) const
{
    TArray<FHSRollbackState> Result;
    
    int32 StartIndex = FMath::Max(0, RollbackHistory.Num() - MaxFrames);
    for (int32 i = StartIndex; i < RollbackHistory.Num(); i++)
    {
        Result.Add(RollbackHistory[i]);
    }
    
    return Result;
}

void UHSSynchronizationSystem::CleanupRollbackHistory(int32 KeepFrames)
{
    if (RollbackHistory.Num() <= KeepFrames)
    {
        return;
    }
    
    int32 RemoveCount = RollbackHistory.Num() - KeepFrames;
    
    // 오래된 상태들을 풀로 반환
    for (int32 i = 0; i < RemoveCount; i++)
    {
        RollbackPool.Add(RollbackHistory[i]);
    }
    
    RollbackHistory.RemoveAt(0, RemoveCount);
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 롤백 히스토리 정리됨 - 제거: %d, 유지: %d"), 
           RemoveCount, KeepFrames);
}

// === 지연 보상 시스템 구현 ===

FString UHSSynchronizationSystem::ScheduleDelayedReward(int32 PlayerID, const TArray<uint8>& RewardData, float DelaySeconds)
{
    if (PlayerID < 0 || RewardData.Num() == 0 || DelaySeconds < 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 잘못된 지연 보상 매개변수"));
        return TEXT("");
    }
    
    FHSDelayedReward DelayedReward;
    DelayedReward.RewardID = GenerateRewardID();
    DelayedReward.RecipientPlayerID = PlayerID;
    DelayedReward.RewardData = RewardData;
    DelayedReward.DelaySeconds = DelaySeconds;
    DelayedReward.ScheduledTime = FDateTime::Now() + FTimespan::FromSeconds(DelaySeconds);
    DelayedReward.bAutoApply = true;
    DelayedReward.RetryCount = 0;
    
    DelayedRewards.Add(DelayedReward);
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 지연 보상 스케줄됨 - ID: %s, Player: %d, Delay: %.2fs"), 
           *DelayedReward.RewardID, PlayerID, DelaySeconds);
    
    return DelayedReward.RewardID;
}

bool UHSSynchronizationSystem::ApplyDelayedReward(const FString& RewardID)
{
    FHSDelayedReward* Reward = nullptr;
    
    for (FHSDelayedReward& DelayedReward : DelayedRewards)
    {
        if (DelayedReward.RewardID == RewardID)
        {
            Reward = &DelayedReward;
            break;
        }
    }
    
    if (!Reward)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSSynchronizationSystem: 지연 보상을 찾을 수 없음 - ID: %s"), *RewardID);
        return false;
    }
    
    // 실제 보상 적용 로직 (실제 구현에서는 인벤토리 시스템 연동)
    // 여기서는 델리게이트 호출로 대체
    OnDelayedRewardApplied.Broadcast(RewardID, Reward->RecipientPlayerID);
    
    // 보상 목록에서 제거
    DelayedRewards.RemoveAll([RewardID](const FHSDelayedReward& DelayedReward)
    {
        return DelayedReward.RewardID == RewardID;
    });
    
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 지연 보상 적용됨 - ID: %s"), *RewardID);
    
    return true;
}

bool UHSSynchronizationSystem::CancelDelayedReward(const FString& RewardID)
{
    int32 RemovedCount = DelayedRewards.RemoveAll([RewardID](const FHSDelayedReward& DelayedReward)
    {
        return DelayedReward.RewardID == RewardID;
    });
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 지연 보상 취소됨 - ID: %s"), *RewardID);
        return true;
    }
    
    return false;
}

TArray<FHSDelayedReward> UHSSynchronizationSystem::GetPlayerDelayedRewards(int32 PlayerID) const
{
    TArray<FHSDelayedReward> PlayerRewards;
    
    for (const FHSDelayedReward& DelayedReward : DelayedRewards)
    {
        if (DelayedReward.RecipientPlayerID == PlayerID)
        {
            PlayerRewards.Add(DelayedReward);
        }
    }
    
    return PlayerRewards;
}

void UHSSynchronizationSystem::ProcessDelayedRewards()
{
    FDateTime CurrentTime = FDateTime::Now();
    TArray<FString> RewardsToApply;
    
    // 적용할 보상 찾기
    for (const FHSDelayedReward& DelayedReward : DelayedRewards)
    {
        if (DelayedReward.bAutoApply && CurrentTime >= DelayedReward.ScheduledTime)
        {
            RewardsToApply.Add(DelayedReward.RewardID);
        }
    }
    
    // 보상 적용
    for (const FString& RewardID : RewardsToApply)
    {
        ApplyDelayedReward(RewardID);
    }
}

// === 동기화 분석 구현 ===

FHSSyncStatistics UHSSynchronizationSystem::GetSyncStatistics() const
{
    FHSSyncStatistics Stats = SyncStats;
    
    // 실시간 계산
    if (Stats.PacketsSent > 0)
    {
        Stats.PacketLossRate = float(Stats.PacketsLost) / float(Stats.PacketsSent);
    }
    
    if (LatencyHistory.Num() > 0)
    {
        float TotalLatency = 0.0f;
        for (float Latency : LatencyHistory)
        {
            TotalLatency += Latency;
        }
        Stats.AverageLatency = TotalLatency / LatencyHistory.Num();
    }
    
    // 동기화 정확도 계산
    int32 TotalSyncTypes = SyncStatusMap.Num();
    int32 SyncedTypes = 0;
    
    for (const auto& StatusPair : SyncStatusMap)
    {
        if (StatusPair.Value == EHSSyncStatus::Synced)
        {
            SyncedTypes++;
        }
    }
    
    if (TotalSyncTypes > 0)
    {
        Stats.SyncAccuracy = float(SyncedTypes) / float(TotalSyncTypes);
    }
    
    return Stats;
}

float UHSSynchronizationSystem::MeasureNetworkLatency(int32 TargetPlayerID) const
{
    const float* Latency = PlayerLatencies.Find(TargetPlayerID);
    return Latency ? *Latency : 0.0f;
}

float UHSSynchronizationSystem::EvaluateSyncQuality() const
{
    FHSSyncStatistics Stats = GetSyncStatistics();
    
    // 품질 점수 계산 (0.0 ~ 1.0)
    float QualityScore = 0.0f;
    
    // 동기화 정확도 (40%)
    QualityScore += Stats.SyncAccuracy * 0.4f;
    
    // 패킷 손실률 (30%, 낮을수록 좋음)
    float PacketLossScore = FMath::Max(0.0f, 1.0f - Stats.PacketLossRate);
    QualityScore += PacketLossScore * 0.3f;
    
    // 평균 레이턴시 (20%, 낮을수록 좋음)
    float LatencyScore = FMath::Max(0.0f, 1.0f - (Stats.AverageLatency / 1000.0f)); // 1초 기준
    QualityScore += LatencyScore * 0.2f;
    
    // 충돌 횟수 (10%, 적을수록 좋음)
    float ConflictScore = FMath::Max(0.0f, 1.0f - (Stats.SyncConflicts / 100.0f)); // 100회 기준
    QualityScore += ConflictScore * 0.1f;
    
    return FMath::Clamp(QualityScore, 0.0f, 1.0f);
}

bool UHSSynchronizationSystem::ResolveConflict(EHSSyncType SyncType, int32 PlayerID, const TArray<uint8>& ConflictData)
{
    // 간단한 충돌 해결: 서버 권한 우선
    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() == NM_DedicatedServer)
        {
            // 서버의 상태를 우선으로 충돌 해결
            TArray<uint8> EmptyData;
            SendSyncPacket(SyncType, EmptyData, EHSSyncPriority::Critical, true);
            
            UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 서버 권한으로 충돌 해결됨 - Type: %d, Player: %d"), 
                   (int32)SyncType, PlayerID);
            
            return true;
        }
    }
    
    // 클라이언트에서는 서버의 결정을 따름
    UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 클라이언트에서 충돌 해결 대기 - Type: %d"), (int32)SyncType);
    
    // 충돌 델리게이트 호출
    OnSyncConflict.Broadcast(SyncType, PlayerID, TEXT("상태 충돌 감지됨"));
    
    return false;
}

// === 유틸리티 구현 ===

void UHSSynchronizationSystem::OptimizeSyncSettings()
{
    // 네트워크 품질에 따른 동기화 설정 자동 조정
    float SyncQuality = EvaluateSyncQuality();
    
    if (SyncQuality < 0.5f)
    {
        // 품질이 낮으면 더 자주 동기화
        TickRate = FMath::Min(120.0f, TickRate * 1.2f);
        PredictionTimeWindow = FMath::Min(1.0f, PredictionTimeWindow * 1.1f);
        
        UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 동기화 설정 최적화 - 품질 개선 모드"));
    }
    else if (SyncQuality > 0.8f)
    {
        // 품질이 높으면 리소스 절약
        TickRate = FMath::Max(30.0f, TickRate * 0.9f);
        PredictionTimeWindow = FMath::Max(0.1f, PredictionTimeWindow * 0.95f);
        
        UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 동기화 설정 최적화 - 효율성 모드"));
    }
    
    // 타이머 재설정
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SyncTickTimer);
        World->GetTimerManager().SetTimer(
            SyncTickTimer,
            this,
            &UHSSynchronizationSystem::ProcessSyncTick,
            1.0f / TickRate,
            true
        );
    }
}

float UHSSynchronizationSystem::GetBandwidthUsage() const
{
    // 간단한 대역폭 계산 (패킷 수 × 평균 패킷 크기)
    int32 TotalPackets = OutgoingPackets.Num() + IncomingPackets.Num();
    float AveragePacketSize = 1024.0f; // 가정: 1KB 평균 패킷 크기
    
    return TotalPackets * AveragePacketSize; // bytes per second
}

FString UHSSynchronizationSystem::GetDebugInfo() const
{
    FHSSyncStatistics Stats = GetSyncStatistics();
    
    FString DebugInfo = FString::Printf(
        TEXT("=== Synchronization Debug Info ===\n")
        TEXT("Packets Sent: %d\n")
        TEXT("Packets Received: %d\n")
        TEXT("Packets Lost: %d\n")
        TEXT("Average Latency: %.2f ms\n")
        TEXT("Packet Loss Rate: %.2f%%\n")
        TEXT("Sync Conflicts: %d\n")
        TEXT("Rollbacks Performed: %d\n")
        TEXT("Sync Accuracy: %.2f%%\n")
        TEXT("Sync Quality: %.2f%%\n")
        TEXT("Bandwidth Usage: %.2f KB/s\n")
        TEXT("Active Predictions: %d\n")
        TEXT("Rollback History: %d frames\n")
        TEXT("Delayed Rewards: %d\n"),
        Stats.PacketsSent,
        Stats.PacketsReceived,
        Stats.PacketsLost,
        Stats.AverageLatency,
        Stats.PacketLossRate * 100.0f,
        Stats.SyncConflicts,
        Stats.RollbacksPerformed,
        Stats.SyncAccuracy * 100.0f,
        EvaluateSyncQuality() * 100.0f,
        GetBandwidthUsage() / 1024.0f,
        PredictionStates.Num(),
        RollbackHistory.Num(),
        DelayedRewards.Num()
    );
    
    return DebugInfo;
}

void UHSSynchronizationSystem::ForcResync(EHSSyncType SyncType)
{
    if (SyncType == EHSSyncType::None)
    {
        // 모든 동기화 타입 재동기화
        for (auto& StatusPair : SyncStatusMap)
        {
            StatusPair.Value = EHSSyncStatus::Syncing;
        }
        
        UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 모든 동기화 타입 강제 재동기화"));
    }
    else
    {
        // 특정 타입만 재동기화
        SyncStatusMap.Add(SyncType, EHSSyncStatus::Syncing);
        
        UE_LOG(LogTemp, Log, TEXT("HSSynchronizationSystem: 동기화 타입 %d 강제 재동기화"), (int32)SyncType);
    }
    
    // 캐시 무효화
    StatusCache.Empty();
    PredictionCache.Empty();
}

// === 내부 함수 구현 ===

void UHSSynchronizationSystem::ProcessSyncTick()
{
    // 동기화 큐 처리
    ProcessPacketQueue();
    
    // 네트워크 품질 평가
    EvaluateNetworkQuality();
}

void UHSSynchronizationSystem::ProcessPacketQueue()
{
    // 나가는 패킷 처리 (우선순위별로 정렬)
    OutgoingPackets.Sort([](const FHSSyncPacket& A, const FHSSyncPacket& B)
    {
        if (A.Priority != B.Priority)
        {
            return A.Priority > B.Priority; // 높은 우선순위 먼저
        }
        return A.SequenceNumber < B.SequenceNumber; // 시퀀스 번호 순
    });
    
    // 실제 네트워크 전송 (시뮬레이션)
    int32 ProcessedPackets = 0;
    const int32 MaxPacketsPerTick = 10; // 틱당 최대 처리 패킷 수
    
    for (int32 i = OutgoingPackets.Num() - 1; i >= 0 && ProcessedPackets < MaxPacketsPerTick; i--)
    {
        const FHSSyncPacket& Packet = OutgoingPackets[i];
        
        // 패킷 전송 시뮬레이션
        // 실제 구현에서는 UE5 네트워크 시스템 사용
        
        // 풀로 반환
        PacketPool.Add(Packet);
        OutgoingPackets.RemoveAt(i);
        ProcessedPackets++;
    }
}

void UHSSynchronizationSystem::UpdatePredictions(float DeltaTime)
{
    for (auto& PredictionPair : PredictionStates)
    {
        FHSPredictionState& State = PredictionPair.Value;
        
        // 예측 타입에 따른 업데이트
        switch (State.PredictionType)
        {
            case EHSPredictionType::Linear:
                State.Position += State.Velocity * DeltaTime;
                break;
                
            case EHSPredictionType::Quadratic:
                State.Position += State.Velocity * DeltaTime + 0.5f * State.Acceleration * DeltaTime * DeltaTime;
                State.Velocity += State.Acceleration * DeltaTime;
                break;
                
            case EHSPredictionType::Physics:
                // 간단한 물리 시뮬레이션
                State.Position += State.Velocity * DeltaTime;
                State.Velocity += State.Acceleration * DeltaTime;
                
                // 회전 업데이트
                State.Rotation += State.AngularVelocity * DeltaTime;
                break;
                
            default:
                break;
        }
        
        State.PredictionTime = FDateTime::Now();
    }
    
    // 캐시 무효화
    PredictionCache.Empty();
}

void UHSSynchronizationSystem::UpdateStatistics()
{
    // 레이턴시 히스토리 업데이트
    if (PlayerLatencies.Num() > 0)
    {
        float TotalLatency = 0.0f;
        for (const auto& LatencyPair : PlayerLatencies)
        {
            TotalLatency += LatencyPair.Value;
        }
        
        float AverageLatency = TotalLatency / PlayerLatencies.Num();
        LatencyHistory.Add(AverageLatency);
        
        // 히스토리 크기 제한
        if (LatencyHistory.Num() > 100)
        {
            LatencyHistory.RemoveAt(0);
        }
    }
    
    // 패킷 손실률 업데이트
    if (SyncStats.PacketsSent > 0)
    {
        SyncStats.PacketLossRate = float(SyncStats.PacketsLost) / float(SyncStats.PacketsSent);
    }
}

void UHSSynchronizationSystem::PerformCleanup()
{
    // 오래된 패킷 정리
    FDateTime CurrentTime = FDateTime::Now();
    
    IncomingPackets.RemoveAll([CurrentTime](const FHSSyncPacket& Packet)
    {
        return (CurrentTime - Packet.Timestamp).GetTotalMinutes() > 5.0f; // 5분 이상 된 패킷 제거
    });
    
    // 오래된 예측 상태 정리
    TArray<FString> StatesToRemove;
    for (const auto& PredictionPair : PredictionStates)
    {
        const FHSPredictionState& State = PredictionPair.Value;
        if ((CurrentTime - State.PredictionTime).GetTotalMinutes() > 2.0f) // 2분 이상 된 예측 제거
        {
            StatesToRemove.Add(PredictionPair.Key);
        }
    }
    
    for (const FString& StateID : StatesToRemove)
    {
        StopPrediction(StateID);
    }
    
    // 롤백 히스토리 정리
    CleanupRollbackHistory(MaxRollbackFrames);
    
    // 캐시 정리
    if ((CurrentTime - LastCacheUpdate).GetTotalMinutes() > 1.0f)
    {
        PredictionCache.Empty();
        StatusCache.Empty();
        LastCacheUpdate = CurrentTime;
    }
}

FString UHSSynchronizationSystem::GeneratePacketID() const
{
    FGuid NewGuid = FGuid::NewGuid();
    return FString::Printf(TEXT("PKT_%s"), *NewGuid.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
}

FString UHSSynchronizationSystem::GenerateStateID() const
{
    FGuid NewGuid = FGuid::NewGuid();
    return FString::Printf(TEXT("STATE_%s"), *NewGuid.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
}

FString UHSSynchronizationSystem::GenerateRewardID() const
{
    FGuid NewGuid = FGuid::NewGuid();
    return FString::Printf(TEXT("REWARD_%s"), *NewGuid.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
}

// === 예측 알고리즘 구현 ===

FHSPredictionState UHSSynchronizationSystem::PredictLinear(const FHSPredictionState& CurrentState, float DeltaTime) const
{
    FHSPredictionState PredictedState = CurrentState;
    
    // 선형 예측: 위치 = 현재위치 + 속도 * 시간
    PredictedState.Position = CurrentState.Position + CurrentState.Velocity * DeltaTime;
    
    // 회전도 선형으로 예측
    PredictedState.Rotation = CurrentState.Rotation + CurrentState.AngularVelocity * DeltaTime;
    
    // 신뢰도는 시간에 따라 감소
    PredictedState.Confidence = FMath::Max(0.1f, CurrentState.Confidence - DeltaTime * 0.1f);
    
    return PredictedState;
}

FHSPredictionState UHSSynchronizationSystem::PredictQuadratic(const FHSPredictionState& CurrentState, float DeltaTime) const
{
    FHSPredictionState PredictedState = CurrentState;
    
    // 이차 예측: 위치 = 현재위치 + 속도*시간 + 0.5*가속도*시간^2
    PredictedState.Position = CurrentState.Position + 
                             CurrentState.Velocity * DeltaTime + 
                             0.5f * CurrentState.Acceleration * DeltaTime * DeltaTime;
    
    // 속도 = 현재속도 + 가속도*시간
    PredictedState.Velocity = CurrentState.Velocity + CurrentState.Acceleration * DeltaTime;
    
    // 회전 예측
    PredictedState.Rotation = CurrentState.Rotation + CurrentState.AngularVelocity * DeltaTime;
    
    // 신뢰도 감소
    PredictedState.Confidence = FMath::Max(0.1f, CurrentState.Confidence - DeltaTime * 0.15f);
    
    return PredictedState;
}

FHSPredictionState UHSSynchronizationSystem::PredictPhysics(const FHSPredictionState& CurrentState, float DeltaTime) const
{
    FHSPredictionState PredictedState = CurrentState;
    
    // 물리 기반 예측 (중력, 마찰 등 고려)
    FVector Gravity = FVector(0.0f, 0.0f, -980.0f); // cm/s^2
    FVector TotalAcceleration = CurrentState.Acceleration + Gravity;
    
    // 위치 업데이트
    PredictedState.Position = CurrentState.Position + 
                             CurrentState.Velocity * DeltaTime + 
                             0.5f * TotalAcceleration * DeltaTime * DeltaTime;
    
    // 속도 업데이트 (마찰 포함)
    float FrictionCoeff = 0.95f; // 5% 마찰
    PredictedState.Velocity = (CurrentState.Velocity + TotalAcceleration * DeltaTime) * FrictionCoeff;
    
    // 회전 업데이트
    PredictedState.Rotation = CurrentState.Rotation + CurrentState.AngularVelocity * DeltaTime;
    
    // 신뢰도 감소 (물리 예측은 더 정확하므로 천천히 감소)
    PredictedState.Confidence = FMath::Max(0.2f, CurrentState.Confidence - DeltaTime * 0.05f);
    
    return PredictedState;
}

bool UHSSynchronizationSystem::DetectSyncConflict(const FHSSyncPacket& Packet) const
{
    // 간단한 충돌 감지: 같은 타입의 최근 패킷과 비교
    for (const FHSSyncPacket& ExistingPacket : IncomingPackets)
    {
        if (ExistingPacket.SyncType == Packet.SyncType && 
            ExistingPacket.SourcePlayerID != Packet.SourcePlayerID)
        {
            // 시간 차이가 적으면서 다른 소스에서 온 패킷은 충돌 가능성
            float TimeDiff = FMath::Abs((Packet.Timestamp - ExistingPacket.Timestamp).GetTotalMilliseconds());
            if (TimeDiff < 100.0f) // 100ms 이내
            {
                return true;
            }
        }
    }
    
    return false;
}

void UHSSynchronizationSystem::EvaluateNetworkQuality()
{
    // 네트워크 품질 평가 및 설정 자동 조정
    float CurrentQuality = EvaluateSyncQuality();
    
    // 품질이 급격히 변하면 설정 최적화
    static float LastQuality = 1.0f;
    float QualityChange = FMath::Abs(CurrentQuality - LastQuality);
    
    if (QualityChange > 0.2f) // 20% 이상 변화
    {
        OptimizeSyncSettings();
    }
    
    LastQuality = CurrentQuality;
}
