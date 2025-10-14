// 사냥의 영혼(HuntingSpirit) 게임의 복제 컴포넌트
// 네트워크 복제를 최적화하고 관리하는 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/DataReplication.h"
#include "HSReplicationComponent.generated.h"

// 전방 선언
class UNetConnection;
class AHSCharacterBase;

// 복제 우선순위 레벨
UENUM(BlueprintType)
enum class EHSReplicationPriority : uint8
{
    RP_VeryLow      UMETA(DisplayName = "Very Low"),        // 매우 낮음 (장식품 등)
    RP_Low          UMETA(DisplayName = "Low"),             // 낮음 (환경 오브젝트)
    RP_Normal       UMETA(DisplayName = "Normal"),          // 보통 (일반 게임 오브젝트)
    RP_High         UMETA(DisplayName = "High"),            // 높음 (플레이어, 중요한 적)
    RP_VeryHigh     UMETA(DisplayName = "Very High"),       // 매우 높음 (보스, 중요한 이벤트)
    RP_Critical     UMETA(DisplayName = "Critical")         // 치명적 (즉시 복제 필요)
};

// 복제 채널 타입
UENUM(BlueprintType)
enum class EHSReplicationChannel : uint8
{
    RC_Default      UMETA(DisplayName = "Default"),         // 기본 채널
    RC_Movement     UMETA(DisplayName = "Movement"),        // 이동 관련
    RC_Combat       UMETA(DisplayName = "Combat"),          // 전투 관련
    RC_Animation    UMETA(DisplayName = "Animation"),       // 애니메이션 관련
    RC_UI          UMETA(DisplayName = "UI"),               // UI 관련
    RC_Audio       UMETA(DisplayName = "Audio"),            // 오디오 관련
    RC_VFX         UMETA(DisplayName = "VFX")               // 비주얼 이펙트 관련
};

// 복제 데이터 패킷 구조체
USTRUCT(BlueprintType)
struct FHSReplicationPacket
{
    GENERATED_BODY()

    // 패킷 ID
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    int32 PacketID;

    // 타임스탬프
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    float Timestamp;

    // 우선순위
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    EHSReplicationPriority Priority;

    // 채널
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    EHSReplicationChannel Channel;

    // 데이터 크기 (바이트)
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    int32 DataSize;

    // 압축 사용 여부
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    bool bWasCompressed;

    // 델타 압축 사용 여부
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    bool bWasDeltaCompressed;

    // 원본 데이터 크기
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    int32 UncompressedSize;

    // 신뢰성 필요 여부
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    bool bReliable;

    // 순서 보장 필요 여부
    UPROPERTY(BlueprintReadOnly, Category = "Replication Packet")
    bool bOrdered;

    // 기본값 설정
    FHSReplicationPacket()
    {
        PacketID = 0;
        Timestamp = 0.0f;
        Priority = EHSReplicationPriority::RP_Normal;
        Channel = EHSReplicationChannel::RC_Default;
        DataSize = 0;
        bWasCompressed = false;
        bWasDeltaCompressed = false;
        UncompressedSize = 0;
        bReliable = true;
        bOrdered = true;
    }
};

// 복제 통계 구조체
USTRUCT(BlueprintType)
struct FHSReplicationStats
{
    GENERATED_BODY()

    // 송신된 패킷 수
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    int32 PacketsSent;

    // 수신된 패킷 수
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    int32 PacketsReceived;

    // 손실된 패킷 수
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    int32 PacketsLost;

    // 중복된 패킷 수
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    int32 PacketsDuplicate;

    // 총 전송 데이터 크기 (바이트)
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    int64 TotalBytesSent;

    // 총 수신 데이터 크기 (바이트)
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    int64 TotalBytesReceived;

    // 평균 RTT (Round Trip Time, 밀리초)
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    float AverageRTT;

    // 대역폭 사용률 (KB/s)
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    float BandwidthUsage;

    // 복제 빈도 (패킷/초)
    UPROPERTY(BlueprintReadOnly, Category = "Replication Stats")
    float ReplicationRate;

    // 기본값 설정
    FHSReplicationStats()
    {
        PacketsSent = 0;
        PacketsReceived = 0;
        PacketsLost = 0;
        PacketsDuplicate = 0;
        TotalBytesSent = 0;
        TotalBytesReceived = 0;
        AverageRTT = 0.0f;
        BandwidthUsage = 0.0f;
        ReplicationRate = 0.0f;
    }
};

// 대역폭 제한 설정
USTRUCT(BlueprintType)
struct FHSBandwidthSettings
{
    GENERATED_BODY()

    // 최대 대역폭 (KB/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bandwidth Settings")
    float MaxBandwidth;

    // 우선순위별 대역폭 할당 비율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bandwidth Settings")
    TMap<EHSReplicationPriority, float> PriorityBandwidthRatio;

    // 채널별 대역폭 할당 비율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bandwidth Settings")
    TMap<EHSReplicationChannel, float> ChannelBandwidthRatio;

    // 적응형 대역폭 조절 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bandwidth Settings")
    bool bAdaptiveBandwidth;

    // 기본값 설정
    FHSBandwidthSettings()
    {
        MaxBandwidth = 1024.0f; // 1MB/s
        bAdaptiveBandwidth = true;

        // 우선순위별 기본 비율
        PriorityBandwidthRatio.Add(EHSReplicationPriority::RP_Critical, 0.4f);
        PriorityBandwidthRatio.Add(EHSReplicationPriority::RP_VeryHigh, 0.3f);
        PriorityBandwidthRatio.Add(EHSReplicationPriority::RP_High, 0.2f);
        PriorityBandwidthRatio.Add(EHSReplicationPriority::RP_Normal, 0.08f);
        PriorityBandwidthRatio.Add(EHSReplicationPriority::RP_Low, 0.015f);
        PriorityBandwidthRatio.Add(EHSReplicationPriority::RP_VeryLow, 0.005f);

        // 채널별 기본 비율
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_Combat, 0.3f);
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_Movement, 0.25f);
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_Animation, 0.2f);
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_Default, 0.15f);
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_VFX, 0.05f);
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_Audio, 0.03f);
        ChannelBandwidthRatio.Add(EHSReplicationChannel::RC_UI, 0.02f);
    }
};

// 복제 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReplicationPacketSent, const FHSReplicationPacket&, Packet, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReplicationPacketReceived, const FHSReplicationPacket&, Packet, bool, bValid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplicationStatsUpdated, const FHSReplicationStats&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBandwidthExceeded, EHSReplicationChannel, Channel, float, ExcessUsage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReplicationError, const FString&, ErrorMessage, int32, ErrorCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnReplicationPayloadReady, const FHSReplicationPacket&, Packet, const TArray<uint8>&, Payload, bool, bWasDelta);

/**
 * 사냥의 영혼 복제 컴포넌트
 * 
 * 주요 기능:
 * - 네트워크 복제 최적화 및 관리
 * - 우선순위 기반 대역폭 할당
 * - 채널별 복제 관리
 * - 적응형 품질 조절
 * - 패킷 손실 복구
 * - 압축 및 델타 압축
 * - 지연 보상 및 예측
 * - 실시간 통계 모니터링
 */
UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSReplicationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 생성자
    UHSReplicationComponent();

    // UActorComponent interface
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 네트워크 복제 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // === 복제 관리 함수들 ===

    /**
     * 데이터를 복제합니다
     * @param Data 복제할 데이터
     * @param Priority 복제 우선순위
     * @param Channel 복제 채널
     * @param bReliable 신뢰성 보장 여부
     * @param bOrdered 순서 보장 여부
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Replication")
    bool ReplicateData(const TArray<uint8>& Data, EHSReplicationPriority Priority = EHSReplicationPriority::RP_Normal, 
                      EHSReplicationChannel Channel = EHSReplicationChannel::RC_Default, 
                      bool bReliable = true, bool bOrdered = true);

    /**
     * 특정 클라이언트에게만 데이터를 복제합니다
     * @param Data 복제할 데이터
     * @param TargetConnection 대상 연결
     * @param Priority 복제 우선순위
     * @param Channel 복제 채널
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Replication")
    bool ReplicateDataToClient(const TArray<uint8>& Data, UNetConnection* TargetConnection,
                              EHSReplicationPriority Priority = EHSReplicationPriority::RP_Normal,
                              EHSReplicationChannel Channel = EHSReplicationChannel::RC_Default);

    /**
     * 멀티캐스트 복제를 수행합니다
     * @param Data 복제할 데이터
     * @param Priority 복제 우선순위
     * @param Channel 복제 채널
     * @param MaxDistance 최대 거리 (0이면 무제한)
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Replication")
    bool MulticastReplication(const TArray<uint8>& Data, EHSReplicationPriority Priority = EHSReplicationPriority::RP_Normal,
                             EHSReplicationChannel Channel = EHSReplicationChannel::RC_Default, float MaxDistance = 0.0f);

    /**
     * 복제를 중지합니다
     * @param Channel 중지할 채널 (RC_Default면 모든 채널)
     */
    UFUNCTION(BlueprintCallable, Category = "Replication")
    void StopReplication(EHSReplicationChannel Channel = EHSReplicationChannel::RC_Default);

    /**
     * 복제를 재시작합니다
     * @param Channel 재시작할 채널 (RC_Default면 모든 채널)
     */
    UFUNCTION(BlueprintCallable, Category = "Replication")
    void ResumeReplication(EHSReplicationChannel Channel = EHSReplicationChannel::RC_Default);

    // === 우선순위 및 품질 관리 ===

    /**
     * 액터의 복제 우선순위를 설정합니다
     * @param Priority 새로운 우선순위
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Priority")
    void SetReplicationPriority(EHSReplicationPriority Priority);

    /**
     * 현재 복제 우선순위를 반환합니다
     * @return 현재 우선순위
     */
    UFUNCTION(BlueprintPure, Category = "Replication Priority")
    EHSReplicationPriority GetReplicationPriority() const { return CurrentPriority; }

    /**
     * 거리 기반 우선순위 자동 조절을 활성화/비활성화합니다
     * @param bEnable 활성화 여부
     * @param MaxDistance 최대 거리
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Priority")
    void SetDistanceBasedPriority(bool bEnable, float MaxDistance = 5000.0f);

    /**
     * 대역폭 기반 품질 조절을 활성화/비활성화합니다
     * @param bEnable 활성화 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Quality")
    void SetAdaptiveQuality(bool bEnable);

    /**
     * 복제 빈도를 설정합니다 (초당 복제 횟수)
     * @param Channel 대상 채널
     * @param Rate 복제 빈도 (Hz)
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Quality")
    void SetReplicationRate(EHSReplicationChannel Channel, float Rate);

    // === 압축 및 최적화 ===

    /**
     * 데이터 압축을 활성화/비활성화합니다
     * @param bEnable 압축 활성화 여부
     * @param CompressionLevel 압축 레벨 (1-9, 높을수록 더 압축)
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Compression")
    void SetCompressionEnabled(bool bEnable, int32 CompressionLevel = 6);

    /**
     * 델타 압축을 활성화/비활성화합니다
     * @param bEnable 델타 압축 활성화 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Compression")
    void SetDeltaCompressionEnabled(bool bEnable);

    /**
     * 배치 처리를 설정합니다
     * @param bEnable 배치 처리 활성화 여부
     * @param BatchSize 배치 크기
     * @param BatchTimeout 배치 타임아웃 (초)
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Optimization")
    void SetBatchProcessing(bool bEnable, int32 BatchSize = 10, float BatchTimeout = 0.1f);

    // === 통계 및 모니터링 ===

    /**
     * 현재 복제 통계를 반환합니다
     * @return 복제 통계
     */
    UFUNCTION(BlueprintPure, Category = "Replication Stats")
    FHSReplicationStats GetReplicationStats() const { return ReplicationStats; }

    /**
     * 채널별 통계를 반환합니다
     * @param Channel 조회할 채널
     * @return 채널 통계
     */
    UFUNCTION(BlueprintPure, Category = "Replication Stats")
    FHSReplicationStats GetChannelStats(EHSReplicationChannel Channel) const;

    /**
     * 현재 대역폭 사용률을 반환합니다 (KB/s)
     * @return 대역폭 사용률
     */
    UFUNCTION(BlueprintPure, Category = "Replication Stats")
    float GetCurrentBandwidthUsage() const { return ReplicationStats.BandwidthUsage; }

    /**
     * 패킷 손실률을 반환합니다 (0.0 ~ 1.0)
     * @return 패킷 손실률
     */
    UFUNCTION(BlueprintPure, Category = "Replication Stats")
    float GetPacketLossRate() const;

    /**
     * 평균 지연시간을 반환합니다 (밀리초)
     * @return 평균 지연시간
     */
    UFUNCTION(BlueprintPure, Category = "Replication Stats")
    float GetAverageLatency() const { return ReplicationStats.AverageRTT; }

    /**
     * 복제 통계를 초기화합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Replication Stats")
    void ResetStatistics();

    // === 대역폭 관리 ===

    /**
     * 대역폭 설정을 적용합니다
     * @param BandwidthSettings 새로운 대역폭 설정
     */
    UFUNCTION(BlueprintCallable, Category = "Bandwidth Management")
    void SetBandwidthSettings(const FHSBandwidthSettings& BandwidthSettings);

    /**
     * 현재 대역폭 설정을 반환합니다
     * @return 현재 대역폭 설정
     */
    UFUNCTION(BlueprintPure, Category = "Bandwidth Management")
    FHSBandwidthSettings GetBandwidthSettings() const { return BandwidthSettings; }

    /**
     * 채널별 대역폭 제한을 설정합니다
     * @param Channel 대상 채널
     * @param BandwidthLimit 대역폭 제한 (KB/s)
     */
    UFUNCTION(BlueprintCallable, Category = "Bandwidth Management")
    void SetChannelBandwidthLimit(EHSReplicationChannel Channel, float BandwidthLimit);

    // === 유틸리티 함수들 ===

    /**
     * 복제 컴포넌트가 활성화되어 있는지 확인합니다
     * @return 활성화 여부
     */
    UFUNCTION(BlueprintPure, Category = "Replication State")
    bool IsReplicationEnabled() const { return bReplicationEnabled; }

    /**
     * 현재 연결 품질을 반환합니다 (0-4 스케일)
     * @return 연결 품질
     */
    UFUNCTION(BlueprintPure, Category = "Replication State")
    int32 GetConnectionQuality() const;

    /**
     * 복제 컴포넌트 정보를 문자열로 반환합니다 (디버그용)
     * @return 정보 문자열
     */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    FString GetReplicationInfoString() const;

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Replication Events")
    FOnReplicationPacketSent OnReplicationPacketSent;

    UPROPERTY(BlueprintAssignable, Category = "Replication Events")
    FOnReplicationPacketReceived OnReplicationPacketReceived;

    UPROPERTY(BlueprintAssignable, Category = "Replication Events")
    FOnReplicationStatsUpdated OnReplicationStatsUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Replication Events")
    FOnBandwidthExceeded OnBandwidthExceeded;

    UPROPERTY(BlueprintAssignable, Category = "Replication Events")
    FOnReplicationError OnReplicationError;

    UPROPERTY(BlueprintAssignable, Category = "Replication Events")
    FOnReplicationPayloadReady OnReplicationPayloadReady;

protected:
    // === 복제되는 변수들 ===

    // 현재 복제 우선순위
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Replication State")
    EHSReplicationPriority CurrentPriority;

    // 복제 활성화 상태
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Replication State")
    bool bReplicationEnabled;

    // 복제 통계
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Replication State")
    FHSReplicationStats ReplicationStats;

    // === 설정 변수들 ===

    // 대역폭 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bandwidth Configuration")
    FHSBandwidthSettings BandwidthSettings;

    // 거리 기반 우선순위 조절 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Priority Configuration")
    bool bDistanceBasedPriority;

    // 최대 복제 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Priority Configuration")
    float MaxReplicationDistance;

    // 적응형 품질 조절 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality Configuration")
    bool bAdaptiveQuality;

    // 데이터 압축 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compression Configuration")
    bool bCompressionEnabled;

    // 압축 레벨 (1-9)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compression Configuration", meta = (ClampMin = "1", ClampMax = "9"))
    int32 CompressionLevel;

    // 델타 압축 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compression Configuration")
    bool bDeltaCompressionEnabled;

    // 배치 처리 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization Configuration")
    bool bBatchProcessingEnabled;

    // 배치 크기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization Configuration")
    int32 BatchSize;

    // 배치 타임아웃 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization Configuration")
    float BatchTimeout;

    // 통계 업데이트 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    float StatsUpdateInterval;

    // 우선순위 업데이트 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    float PriorityUpdateInterval;

    // === 런타임 변수들 ===

    // 채널별 복제 상태
    UPROPERTY()
    TMap<EHSReplicationChannel, bool> ChannelReplicationState;

    // 채널별 복제 빈도
    UPROPERTY()
    TMap<EHSReplicationChannel, float> ChannelReplicationRates;

    // 채널별 통계
    UPROPERTY()
    TMap<EHSReplicationChannel, FHSReplicationStats> ChannelStats;

    // 패킷 큐 (배치 처리용)
    struct FHSQueuedReplicationPacket
    {
        FHSReplicationPacket Packet;
        TArray<uint8> Payload;
    };

    TArray<FHSQueuedReplicationPacket> PacketQueue;

    // 다음 패킷 ID
    int32 NextPacketID;

    // 마지막 통계 업데이트 시간
    float LastStatsUpdateTime;

    // 마지막 우선순위 업데이트 시간
    float LastPriorityUpdateTime;

    // === 타이머 핸들들 ===

    // 통계 업데이트 타이머
    FTimerHandle StatsUpdateTimer;

    // 배치 처리 타이머
    FTimerHandle BatchProcessTimer;

    // 우선순위 업데이트 타이머
    FTimerHandle PriorityUpdateTimer;

    // 품질 조절 타이머
    FTimerHandle QualityAdjustmentTimer;

private:
    // === 내부 함수들 ===

    // 복제 시스템 초기화
    void InitializeReplication();

    // 타이머 설정
    void SetupTimers();

    // 우선순위 자동 조절
    void UpdatePriorityBasedOnDistance();

    // 품질 자동 조절
    void AdjustQualityBasedOnBandwidth();

    // 배치 처리 실행
    UFUNCTION()
    void ProcessBatchedPackets();

    // 통계 업데이트
    UFUNCTION()
    void UpdateStatistics();

    // 우선순위 업데이트
    UFUNCTION()
    void UpdatePriority();

    // 품질 조절
    UFUNCTION()
    void AdjustQuality();

    // 데이터 압축
    TArray<uint8> CompressData(const TArray<uint8>& Data) const;

    // 데이터 압축 해제
    TArray<uint8> DecompressData(const TArray<uint8>& CompressedData, int32 UncompressedSize) const;

    // 델타 압축 계산
    TArray<uint8> CalculateDelta(const TArray<uint8>& OldData, const TArray<uint8>& NewData) const;

    // 델타 적용
    TArray<uint8> ApplyDelta(const TArray<uint8>& OldData, const TArray<uint8>& DeltaData) const;

    // 패킷 유효성 검사
    bool ValidatePacket(const FHSReplicationPacket& Packet) const;

    // 대역폭 체크
    bool CheckBandwidthLimit(EHSReplicationChannel Channel, int32 DataSize) const;

    // 채널별 대역폭 사용량 계산
    float CalculateChannelBandwidthUsage(EHSReplicationChannel Channel) const;

    // 연결 품질 계산
    int32 CalculateConnectionQuality() const;

    // === 네트워크 RPC 함수들 ===

    // 서버에서 클라이언트로 데이터 전송
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastReceiveData(const FHSReplicationPacket& Packet, const TArray<uint8>& Data);

    UFUNCTION(Client, Reliable)
    void ClientReceiveDataReliable(const FHSReplicationPacket& Packet, const TArray<uint8>& Data);

    UFUNCTION(Client, Unreliable)
    void ClientReceiveDataUnreliable(const FHSReplicationPacket& Packet, const TArray<uint8>& Data);

    // 클라이언트에서 서버로 응답 전송
    UFUNCTION(Server, Unreliable)
    void ServerReceiveAcknowledgment(int32 PacketID, bool bReceived);

    // === 네트워크 복제 콜백 함수들 ===

    UFUNCTION()
    void OnRep_CurrentPriority();

    UFUNCTION()
    void OnRep_ReplicationEnabled();

    UFUNCTION()
    void OnRep_ReplicationStats();

    // === 디버그 및 로깅 함수들 ===

    // 복제 상태 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogReplicationState() const;

    // 통계 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogReplicationStatistics() const;

    // === 메모리 최적화 관련 ===

    // 사용하지 않는 데이터 정리
    void CleanupUnusedData();

    // 패킷 큐 최적화
    void OptimizePacketQueue();

    // 델타/압축 복원을 위한 이전 데이터
    TMap<EHSReplicationChannel, TArray<uint8>> LastCompressedFrameData;
    TMap<EHSReplicationChannel, TArray<uint8>> LastRawFrameData;

    bool PreparePayloadForTransmission(const TArray<uint8>& Data, EHSReplicationPriority Priority,
        EHSReplicationChannel Channel, bool bReliable, bool bOrdered,
        FHSReplicationPacket& OutPacket, TArray<uint8>& OutPayload);

    void HandleIncomingPayload(const FHSReplicationPacket& Packet, const TArray<uint8>& Data);

    bool DispatchPacketToClient(UNetConnection* TargetConnection, const FHSReplicationPacket& Packet, const TArray<uint8>& Payload);

    // 스레드 안전성을 위한 뮤텍스
    mutable FCriticalSection ReplicationMutex;
    mutable FCriticalSection StatisticsMutex;

    // 초기화 완료 플래그
    bool bInitialized;
};
