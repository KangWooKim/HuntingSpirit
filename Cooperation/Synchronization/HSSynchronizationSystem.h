// HuntingSpirit 게임의 고급 동기화 시스템
// 지연 보상, 예측 시스템, 네트워크 상태 동기화 관리
// 작성자: KangWooKim (https://github.com/KangWooKim)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "HSSynchronizationSystem.generated.h"

// 전방 선언
class UHSTeamManager;
class UHSCommunicationSystem;
class UHSRewardsSystem;

// 동기화 타입
UENUM(BlueprintType)
enum class EHSSyncType : uint8
{
    None = 0,
    PlayerState,         // 플레이어 상태 동기화
    WorldState,          // 월드 상태 동기화
    CombatState,         // 전투 상태 동기화
    InventoryState,      // 인벤토리 상태 동기화
    QuestState,          // 퀘스트 상태 동기화
    TeamState,           // 팀 상태 동기화
    RewardState,         // 보상 상태 동기화
    CustomState          // 사용자 정의 상태
};

// 동기화 우선순위
UENUM(BlueprintType)
enum class EHSSyncPriority : uint8
{
    Low = 0,             // 낮은 우선순위 (5초 주기)
    Normal,              // 일반 우선순위 (1초 주기)
    High,                // 높은 우선순위 (0.1초 주기)
    Critical,            // 중요 우선순위 (즉시)
    Realtime             // 실시간 (매 틱)
};

// 예측 타입
UENUM(BlueprintType)
enum class EHSPredictionType : uint8
{
    None = 0,
    Linear,              // 선형 예측
    Quadratic,           // 이차 예측
    Cubic,               // 삼차 예측
    Physics,             // 물리 기반 예측
    AI,                  // AI 기반 예측
    Custom               // 사용자 정의 예측
};

// 동기화 상태
UENUM(BlueprintType)
enum class EHSSyncStatus : uint8
{
    None = 0,
    Syncing,             // 동기화 중
    Synced,              // 동기화 완료
    OutOfSync,           // 동기화 실패
    Conflicted,          // 충돌 상태
    Rollback,            // 롤백 중
    Correcting           // 보정 중
};

// 동기화 데이터 패킷
USTRUCT(BlueprintType)
struct FHSSyncPacket
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PacketID;

    UPROPERTY(BlueprintReadOnly)
    EHSSyncType SyncType;

    UPROPERTY(BlueprintReadOnly)
    EHSSyncPriority Priority;

    UPROPERTY(BlueprintReadOnly)
    int32 SourcePlayerID;

    UPROPERTY(BlueprintReadOnly)
    TArray<uint8> Data;

    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;

    UPROPERTY(BlueprintReadOnly)
    float NetworkLatency;

    UPROPERTY(BlueprintReadOnly)
    int32 SequenceNumber;

    UPROPERTY(BlueprintReadOnly)
    bool bReliable;

    FHSSyncPacket()
    {
        PacketID = TEXT("");
        SyncType = EHSSyncType::None;
        Priority = EHSSyncPriority::Normal;
        SourcePlayerID = -1;
        Timestamp = FDateTime::Now();
        NetworkLatency = 0.0f;
        SequenceNumber = 0;
        bReliable = true;
    }
};

// 예측 상태 정보
USTRUCT(BlueprintType)
struct FHSPredictionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString StateID;

    UPROPERTY(BlueprintReadOnly)
    EHSPredictionType PredictionType;

    UPROPERTY(BlueprintReadOnly)
    FVector Position;

    UPROPERTY(BlueprintReadOnly)
    FVector Velocity;

    UPROPERTY(BlueprintReadOnly)
    FVector Acceleration;

    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;

    UPROPERTY(BlueprintReadOnly)
    FRotator AngularVelocity;

    UPROPERTY(BlueprintReadOnly)
    FDateTime PredictionTime;

    UPROPERTY(BlueprintReadOnly)
    float Confidence;

    UPROPERTY(BlueprintReadOnly)
    TArray<uint8> CustomData;

    FHSPredictionState()
    {
        StateID = TEXT("");
        PredictionType = EHSPredictionType::Linear;
        Position = FVector::ZeroVector;
        Velocity = FVector::ZeroVector;
        Acceleration = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        AngularVelocity = FRotator::ZeroRotator;
        PredictionTime = FDateTime::Now();
        Confidence = 1.0f;
    }

    // 비교 연산자 추가 (TMap 호환성을 위해)
    bool operator==(const FHSPredictionState& Other) const
    {
        return StateID == Other.StateID &&
               PredictionTime == Other.PredictionTime;
    }

    bool operator!=(const FHSPredictionState& Other) const
    {
        return !(*this == Other);
    }
};

// 예측 히스토리 래퍼 구조체 (TMap 호환성을 위해)
USTRUCT(BlueprintType)
struct FHSPredictionHistoryArray
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FHSPredictionState> PredictionStates;

    FHSPredictionHistoryArray()
    {
        PredictionStates.Empty();
    }

    // 비교 연산자 추가
    bool operator==(const FHSPredictionHistoryArray& Other) const
    {
        return PredictionStates.Num() == Other.PredictionStates.Num();
    }

    bool operator!=(const FHSPredictionHistoryArray& Other) const
    {
        return !(*this == Other);
    }

    // 유틸리티 함수들
    void AddPrediction(const FHSPredictionState& State)
    {
        PredictionStates.Add(State);
    }

    void ClearHistory()
    {
        PredictionStates.Empty();
    }

    int32 GetCount() const
    {
        return PredictionStates.Num();
    }

    const FHSPredictionState* GetLatest() const
    {
        return PredictionStates.Num() > 0 ? &PredictionStates.Last() : nullptr;
    }
};

// 롤백 상태 정보
USTRUCT(BlueprintType)
struct FHSRollbackState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString StateID;

    UPROPERTY(BlueprintReadOnly)
    FDateTime StateTime;

    UPROPERTY(BlueprintReadOnly)
    TArray<uint8> StateData;

    UPROPERTY(BlueprintReadOnly)
    int32 FrameNumber;

    UPROPERTY(BlueprintReadOnly)
    float DeltaTime;

    FHSRollbackState()
    {
        StateID = TEXT("");
        StateTime = FDateTime::Now();
        FrameNumber = 0;
        DeltaTime = 0.0f;
    }
};

// 지연 보상 정보
USTRUCT(BlueprintType)
struct FHSDelayedReward
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RewardID;

    UPROPERTY(BlueprintReadOnly)
    int32 RecipientPlayerID;

    UPROPERTY(BlueprintReadOnly)
    TArray<uint8> RewardData;

    UPROPERTY(BlueprintReadOnly)
    FDateTime ScheduledTime;

    UPROPERTY(BlueprintReadOnly)
    float DelaySeconds;

    UPROPERTY(BlueprintReadOnly)
    bool bAutoApply;

    UPROPERTY(BlueprintReadOnly)
    int32 RetryCount;

    FHSDelayedReward()
    {
        RewardID = TEXT("");
        RecipientPlayerID = -1;
        ScheduledTime = FDateTime::Now();
        DelaySeconds = 0.0f;
        bAutoApply = true;
        RetryCount = 0;
    }
};

// 동기화 통계
USTRUCT(BlueprintType)
struct FHSSyncStatistics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 PacketsSent;

    UPROPERTY(BlueprintReadOnly)
    int32 PacketsReceived;

    UPROPERTY(BlueprintReadOnly)
    int32 PacketsLost;

    UPROPERTY(BlueprintReadOnly)
    float AverageLatency;

    UPROPERTY(BlueprintReadOnly)
    float PacketLossRate;

    UPROPERTY(BlueprintReadOnly)
    int32 SyncConflicts;

    UPROPERTY(BlueprintReadOnly)
    int32 RollbacksPerformed;

    UPROPERTY(BlueprintReadOnly)
    float SyncAccuracy;

    FHSSyncStatistics()
    {
        PacketsSent = 0;
        PacketsReceived = 0;
        PacketsLost = 0;
        AverageLatency = 0.0f;
        PacketLossRate = 0.0f;
        SyncConflicts = 0;
        RollbacksPerformed = 0;
        SyncAccuracy = 1.0f;
    }
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSyncPacketReceived, const FHSSyncPacket&, Packet, EHSSyncStatus, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSyncConflict, EHSSyncType, SyncType, int32, PlayerID, const FString&, ConflictInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRollbackPerformed, const FString&, StateID, int32, FramesRolledBack);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDelayedRewardApplied, const FString&, RewardID, int32, PlayerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionCorrected, const FString&, StateID, float, ErrorMagnitude);

/**
 * 고급 동기화 시스템
 * 지연 보상, 예측, 롤백, 네트워크 상태 동기화 관리
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSSynchronizationSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSSynchronizationSystem();

    // UGameInstanceSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // 커스텀 틱 처리 메서드
    void TickSynchronization(float DeltaTime);

    // === 동기화 패킷 관리 ===

    // 동기화 패킷 전송
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Packets")
    bool SendSyncPacket(EHSSyncType SyncType, const TArray<uint8>& Data, EHSSyncPriority Priority = EHSSyncPriority::Normal, bool bReliable = true);

    // 동기화 패킷 수신 처리
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Packets")
    void ReceiveSyncPacket(const FHSSyncPacket& Packet);

    // 패킷 우선순위 설정
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Packets")
    void SetSyncPriority(EHSSyncType SyncType, EHSSyncPriority Priority);

    // 동기화 상태 확인
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Packets")
    EHSSyncStatus GetSyncStatus(EHSSyncType SyncType) const;

    // === 예측 시스템 ===

    // 상태 예측 시작
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Prediction")
    FString StartPrediction(const FHSPredictionState& InitialState, EHSPredictionType PredictionType);

    // 예측 상태 업데이트
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Prediction")
    bool UpdatePrediction(const FString& StateID, const FHSPredictionState& NewState);

    // 예측 상태 가져오기
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Prediction")
    FHSPredictionState GetPredictedState(const FString& StateID, float FutureTime = 0.0f) const;

    // 예측 보정
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Prediction")
    void CorrectPrediction(const FString& StateID, const FHSPredictionState& AuthoritativeState);

    // 예측 중지
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Prediction")
    bool StopPrediction(const FString& StateID);

    // === 롤백 시스템 ===

    // 상태 스냅샷 저장
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Rollback")
    FString SaveStateSnapshot(const TArray<uint8>& StateData, int32 FrameNumber);

    // 상태 롤백 실행
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Rollback")
    bool RollbackToState(const FString& StateID);

    // 프레임 기반 롤백
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Rollback")
    bool RollbackToFrame(int32 TargetFrame);

    // 롤백 히스토리 가져오기
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Rollback")
    TArray<FHSRollbackState> GetRollbackHistory(int32 MaxFrames = 60) const;

    // 롤백 히스토리 정리
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Rollback")
    void CleanupRollbackHistory(int32 KeepFrames = 300);

    // === 지연 보상 시스템 ===

    // 지연 보상 스케줄링
    UFUNCTION(BlueprintCallable, Category = "Synchronization|DelayedReward")
    FString ScheduleDelayedReward(int32 PlayerID, const TArray<uint8>& RewardData, float DelaySeconds);

    // 지연 보상 즉시 적용
    UFUNCTION(BlueprintCallable, Category = "Synchronization|DelayedReward")
    bool ApplyDelayedReward(const FString& RewardID);

    // 지연 보상 취소
    UFUNCTION(BlueprintCallable, Category = "Synchronization|DelayedReward")
    bool CancelDelayedReward(const FString& RewardID);

    // 플레이어 지연 보상 목록
    UFUNCTION(BlueprintCallable, Category = "Synchronization|DelayedReward")
    TArray<FHSDelayedReward> GetPlayerDelayedRewards(int32 PlayerID) const;

    // 지연 보상 처리
    UFUNCTION(BlueprintCallable, Category = "Synchronization|DelayedReward")
    void ProcessDelayedRewards();

    // === 동기화 분석 ===

    // 동기화 통계 가져오기
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Analysis")
    FHSSyncStatistics GetSyncStatistics() const;

    // 네트워크 레이턴시 측정
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Analysis")
    float MeasureNetworkLatency(int32 TargetPlayerID) const;

    // 동기화 품질 평가
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Analysis")
    float EvaluateSyncQuality() const;

    // 충돌 해결
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Analysis")
    bool ResolveConflict(EHSSyncType SyncType, int32 PlayerID, const TArray<uint8>& ConflictData);

    // === 유틸리티 ===

    // 동기화 설정 최적화
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Utility")
    void OptimizeSyncSettings();

    // 대역폭 사용량 분석
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Utility")
    float GetBandwidthUsage() const;

    // 동기화 디버그 정보
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Utility")
    FString GetDebugInfo() const;

    // 강제 재동기화
    UFUNCTION(BlueprintCallable, Category = "Synchronization|Utility")
    void ForcResync(EHSSyncType SyncType = EHSSyncType::None);

    // === 델리게이트 ===

    UPROPERTY(BlueprintAssignable, Category = "Synchronization|Events")
    FOnSyncPacketReceived OnSyncPacketReceived;

    UPROPERTY(BlueprintAssignable, Category = "Synchronization|Events")
    FOnSyncConflict OnSyncConflict;

    UPROPERTY(BlueprintAssignable, Category = "Synchronization|Events")
    FOnRollbackPerformed OnRollbackPerformed;

    UPROPERTY(BlueprintAssignable, Category = "Synchronization|Events")
    FOnDelayedRewardApplied OnDelayedRewardApplied;

    UPROPERTY(BlueprintAssignable, Category = "Synchronization|Events")
    FOnPredictionCorrected OnPredictionCorrected;

private:
    // === 동기화 상태 ===

    UPROPERTY()
    TMap<EHSSyncType, EHSSyncStatus> SyncStatusMap;

    UPROPERTY()
    TMap<EHSSyncType, EHSSyncPriority> SyncPriorityMap;

    // === 패킷 관리 ===

    UPROPERTY()
    TArray<FHSSyncPacket> OutgoingPackets;

    UPROPERTY()
    TArray<FHSSyncPacket> IncomingPackets;

    UPROPERTY()
    int32 NextSequenceNumber;

    // === 예측 시스템 데이터 ===

    UPROPERTY()
    TMap<FString, FHSPredictionState> PredictionStates;

    UPROPERTY()
    TMap<FString, FHSPredictionHistoryArray> PredictionHistory;

    // === 롤백 시스템 데이터 ===

    UPROPERTY()
    TArray<FHSRollbackState> RollbackHistory;

    UPROPERTY()
    int32 CurrentFrameNumber;

    UPROPERTY()
    int32 MaxRollbackFrames;

    // === 지연 보상 데이터 ===

    UPROPERTY()
    TArray<FHSDelayedReward> DelayedRewards;

    // === 통계 및 분석 ===

    UPROPERTY()
    FHSSyncStatistics SyncStats;

    UPROPERTY()
    TMap<int32, float> PlayerLatencies;

    UPROPERTY()
    TArray<float> LatencyHistory;

    // === 설정 ===

    UPROPERTY()
    float TickRate;

    UPROPERTY()
    float PredictionTimeWindow;

    UPROPERTY()
    float RollbackTimeWindow;

    UPROPERTY()
    int32 MaxPacketQueueSize;

    UPROPERTY()
    float BandwidthLimit;

    // === 성능 최적화 ===

    // 패킷 풀링
    UPROPERTY()
    TArray<FHSSyncPacket> PacketPool;

    // 상태 풀링
    UPROPERTY()
    TArray<FHSPredictionState> StatePool;

    UPROPERTY()
    TArray<FHSRollbackState> RollbackPool;

    // 캐시 시스템
    mutable TMap<FString, FHSPredictionState> PredictionCache;
    mutable TMap<EHSSyncType, EHSSyncStatus> StatusCache;
    mutable FDateTime LastCacheUpdate;

    // 타이머
    UPROPERTY()
    FTimerHandle SyncTickTimer;

    UPROPERTY()
    FTimerHandle DelayedRewardTimer;

    UPROPERTY()
    FTimerHandle StatisticsTimer;

    UPROPERTY()
    FTimerHandle CleanupTimer;

    // === 내부 함수 ===

    // 동기화 틱 처리
    void ProcessSyncTick();

    // 패킷 처리
    void ProcessPacketQueue();

    // 예측 업데이트
    void UpdatePredictions(float DeltaTime);

    // 통계 업데이트
    void UpdateStatistics();

    // 메모리 정리
    void PerformCleanup();

    // 패킷 ID 생성
    FString GeneratePacketID() const;

    // 상태 ID 생성
    FString GenerateStateID() const;

    // 보상 ID 생성
    FString GenerateRewardID() const;

    // 예측 알고리즘
    FHSPredictionState PredictLinear(const FHSPredictionState& CurrentState, float DeltaTime) const;
    FHSPredictionState PredictQuadratic(const FHSPredictionState& CurrentState, float DeltaTime) const;
    FHSPredictionState PredictPhysics(const FHSPredictionState& CurrentState, float DeltaTime) const;

    // 충돌 감지
    bool DetectSyncConflict(const FHSSyncPacket& Packet) const;

    // 네트워크 품질 평가
    void EvaluateNetworkQuality();

    friend class UHSTeamManager;
    friend class UHSCommunicationSystem;
    friend class UHSRewardsSystem;
};
