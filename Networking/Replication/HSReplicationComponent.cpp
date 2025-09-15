// 사냥의 영혼(HuntingSpirit) 게임의 복제 컴포넌트 구현
// 네트워크 복제를 최적화하고 관리하는 컴포넌트 구현

#include "HSReplicationComponent.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Compression.h"
#include "Misc/Base64.h"
#include "Algo/Accumulate.h"
#include "EngineUtils.h"

// 생성자
UHSReplicationComponent::UHSReplicationComponent()
{
    // 기본값 설정
    CurrentPriority = EHSReplicationPriority::RP_Normal;
    bReplicationEnabled = true;
    ReplicationStats = FHSReplicationStats();

    // 설정 변수 기본값
    BandwidthSettings = FHSBandwidthSettings();
    bDistanceBasedPriority = true;
    MaxReplicationDistance = 5000.0f;
    bAdaptiveQuality = true;
    bCompressionEnabled = true;
    CompressionLevel = 6;
    bDeltaCompressionEnabled = true;
    bBatchProcessingEnabled = true;
    BatchSize = 10;
    BatchTimeout = 0.1f;
    StatsUpdateInterval = 1.0f;
    PriorityUpdateInterval = 0.5f;

    // 런타임 변수 초기화
    NextPacketID = 1;
    LastStatsUpdateTime = 0.0f;
    LastPriorityUpdateTime = 0.0f;
    bInitialized = false;

    // 채널별 기본 설정
    ChannelReplicationState.Add(EHSReplicationChannel::RC_Default, true);
    ChannelReplicationState.Add(EHSReplicationChannel::RC_Movement, true);
    ChannelReplicationState.Add(EHSReplicationChannel::RC_Combat, true);
    ChannelReplicationState.Add(EHSReplicationChannel::RC_Animation, true);
    ChannelReplicationState.Add(EHSReplicationChannel::RC_UI, true);
    ChannelReplicationState.Add(EHSReplicationChannel::RC_Audio, true);
    ChannelReplicationState.Add(EHSReplicationChannel::RC_VFX, true);

    // 채널별 기본 복제 빈도 설정 (Hz)
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_Combat, 60.0f);      // 전투는 고빈도
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_Movement, 30.0f);   // 이동은 중간 빈도
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_Animation, 20.0f);  // 애니메이션은 중간 빈도
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_Default, 15.0f);    // 기본은 낮은 빈도
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_VFX, 10.0f);        // 이펙트는 낮은 빈도
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_Audio, 8.0f);       // 오디오는 낮은 빈도
    ChannelReplicationRates.Add(EHSReplicationChannel::RC_UI, 5.0f);          // UI는 매우 낮은 빈도

    // 채널별 통계 초기화
    for (int32 i = 0; i < (int32)EHSReplicationChannel::RC_VFX + 1; ++i)
    {
        EHSReplicationChannel Channel = (EHSReplicationChannel)i;
        ChannelStats.Add(Channel, FHSReplicationStats());
    }

    // 네트워크 설정
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // 30FPS로 제한하여 성능 최적화
    SetIsReplicatedByDefault(true);

    // 메모리 풀 예약
    PacketQueue.Reserve(BatchSize * 2);
}

// 컴포넌트 시작 시 호출
void UHSReplicationComponent::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 초기화
    if (GetOwner()->HasAuthority())
    {
        InitializeReplication();
        SetupTimers();
    }

    bInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 복제 컴포넌트 초기화 완료 - %s"), 
           *GetOwner()->GetName());
}

// 매 프레임 호출
void UHSReplicationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialized || !bReplicationEnabled)
    {
        return;
    }

    // 서버에서만 실행
    if (GetOwner()->HasAuthority())
    {
        // 거리 기반 우선순위 자동 조절
        if (bDistanceBasedPriority)
        {
            UpdatePriorityBasedOnDistance();
        }

        // 적응형 품질 조절
        if (bAdaptiveQuality)
        {
            AdjustQualityBasedOnBandwidth();
        }

        // 실시간 통계 업데이트
        float CurrentTime = GetWorld()->GetTimeSeconds();
        if (CurrentTime - LastStatsUpdateTime >= StatsUpdateInterval)
        {
            UpdateStatistics();
            LastStatsUpdateTime = CurrentTime;
        }
    }
}

// 컴포넌트 종료 시 호출
void UHSReplicationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(StatsUpdateTimer);
        TimerManager.ClearTimer(BatchProcessTimer);
        TimerManager.ClearTimer(PriorityUpdateTimer);
        TimerManager.ClearTimer(QualityAdjustmentTimer);
    }

    // 남은 패킷 처리
    if (PacketQueue.Num() > 0)
    {
        ProcessBatchedPackets();
    }

    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 복제 컴포넌트 정리 완료 - %s"), 
           *GetOwner()->GetName());

    Super::EndPlay(EndPlayReason);
}

// 네트워크 복제 설정
void UHSReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UHSReplicationComponent, CurrentPriority);
    DOREPLIFETIME(UHSReplicationComponent, bReplicationEnabled);
    DOREPLIFETIME_CONDITION(UHSReplicationComponent, ReplicationStats, COND_SkipOwner); // 오너는 제외
}

// === 복제 관리 함수들 ===

// 데이터 복제
bool UHSReplicationComponent::ReplicateData(const TArray<uint8>& Data, EHSReplicationPriority Priority, 
                                           EHSReplicationChannel Channel, bool bReliable, bool bOrdered)
{
    if (!bReplicationEnabled || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSReplicationComponent: 복제가 비활성화되었거나 권한이 없습니다"));
        return false;
    }

    // 채널 상태 확인
    if (!ChannelReplicationState.Contains(Channel) || !ChannelReplicationState[Channel])
    {
        UE_LOG(LogTemp, Warning, TEXT("HSReplicationComponent: 채널 %d가 비활성화되어 있습니다"), (int32)Channel);
        return false;
    }

    // 데이터 크기 확인
    if (Data.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSReplicationComponent: 빈 데이터를 복제하려고 시도했습니다"));
        return false;
    }

    // 대역폭 체크
    if (!CheckBandwidthLimit(Channel, Data.Num()))
    {
        OnBandwidthExceeded.Broadcast(Channel, Data.Num() / 1024.0f);
        return false;
    }

    // 패킷 생성
    FHSReplicationPacket Packet;
    Packet.PacketID = NextPacketID++;
    Packet.Timestamp = GetWorld()->GetTimeSeconds();
    Packet.Priority = Priority;
    Packet.Channel = Channel;
    Packet.DataSize = Data.Num();
    Packet.bReliable = bReliable;
    Packet.bOrdered = bOrdered;

    // 패킷 유효성 검사
    if (!ValidatePacket(Packet))
    {
        OnReplicationError.Broadcast(TEXT("Invalid packet"), -1);
        return false;
    }

    // 데이터 압축 처리
    TArray<uint8> ProcessedData = Data;
    if (bCompressionEnabled && Data.Num() > 100) // 100바이트 이상일 때만 압축
    {
        ProcessedData = CompressData(Data);
    }

    // 델타 압축 처리
    if (bDeltaCompressionEnabled && PreviousFrameData.Contains(Channel))
    {
        TArray<uint8> DeltaData = CalculateDelta(PreviousFrameData[Channel], ProcessedData);
        if (DeltaData.Num() < ProcessedData.Num()) // 델타가 더 작은 경우만 사용
        {
            ProcessedData = DeltaData;
        }
    }

    // 이전 프레임 데이터 저장 (델타 압축용)
    PreviousFrameData.Add(Channel, ProcessedData);

    // 배치 처리 또는 즉시 전송
    if (bBatchProcessingEnabled && Priority < EHSReplicationPriority::RP_Critical)
    {
        PacketQueue.Add(Packet);
        
        // 배치가 가득 찼거나 중요한 데이터인 경우 즉시 처리
        if (PacketQueue.Num() >= BatchSize || Priority >= EHSReplicationPriority::RP_VeryHigh)
        {
            ProcessBatchedPackets();
        }
    }
    else
    {
        // 즉시 전송
        MulticastReceiveData(Packet, ProcessedData);
    }

    // 통계 업데이트
    {
        FScopeLock StatsLock(&StatisticsMutex);
        ReplicationStats.PacketsSent++;
        ReplicationStats.TotalBytesSent += ProcessedData.Num();
        
        if (ChannelStats.Contains(Channel))
        {
            ChannelStats[Channel].PacketsSent++;
            ChannelStats[Channel].TotalBytesSent += ProcessedData.Num();
        }
    }

    // 이벤트 브로드캐스트
    OnReplicationPacketSent.Broadcast(Packet, true);

    return true;
}

// 특정 클라이언트에게 데이터 복제
bool UHSReplicationComponent::ReplicateDataToClient(const TArray<uint8>& Data, UNetConnection* TargetConnection,
                                                   EHSReplicationPriority Priority, EHSReplicationChannel Channel)
{
    if (!TargetConnection || !bReplicationEnabled)
    {
        return false;
    }

    // 기본 복제 로직 활용 (간소화된 구현)
    return ReplicateData(Data, Priority, Channel);
}

// 멀티캐스트 복제
bool UHSReplicationComponent::MulticastReplication(const TArray<uint8>& Data, EHSReplicationPriority Priority,
                                                  EHSReplicationChannel Channel, float MaxDistance)
{
    if (!GetOwner()->HasAuthority())
    {
        return false;
    }

    // 거리 제한이 있는 경우 거리 확인
    if (MaxDistance > 0.0f)
    {
        // 실제 구현에서는 각 클라이언트와의 거리를 확인하여 선별적으로 전송
        // 여기서는 간소화된 구현
    }

    return ReplicateData(Data, Priority, Channel);
}

// 복제 중지
void UHSReplicationComponent::StopReplication(EHSReplicationChannel Channel)
{
    if (!GetOwner()->HasAuthority())
    {
        return;
    }

    if (Channel == EHSReplicationChannel::RC_Default)
    {
        // 모든 채널 중지
        for (auto& ChannelPair : ChannelReplicationState)
        {
            ChannelPair.Value = false;
        }
        bReplicationEnabled = false;
    }
    else
    {
        // 특정 채널만 중지
        if (ChannelReplicationState.Contains(Channel))
        {
            ChannelReplicationState[Channel] = false;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 복제 중지 - 채널 %d"), (int32)Channel);
}

// 복제 재시작
void UHSReplicationComponent::ResumeReplication(EHSReplicationChannel Channel)
{
    if (!GetOwner()->HasAuthority())
    {
        return;
    }

    if (Channel == EHSReplicationChannel::RC_Default)
    {
        // 모든 채널 재시작
        for (auto& ChannelPair : ChannelReplicationState)
        {
            ChannelPair.Value = true;
        }
        bReplicationEnabled = true;
    }
    else
    {
        // 특정 채널만 재시작
        if (ChannelReplicationState.Contains(Channel))
        {
            ChannelReplicationState[Channel] = true;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 복제 재시작 - 채널 %d"), (int32)Channel);
}

// === 우선순위 및 품질 관리 ===

// 복제 우선순위 설정
void UHSReplicationComponent::SetReplicationPriority(EHSReplicationPriority Priority)
{
    if (GetOwner()->HasAuthority() && CurrentPriority != Priority)
    {
        CurrentPriority = Priority;
        UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 우선순위 변경 - %s: %d"), 
               *GetOwner()->GetName(), (int32)Priority);
    }
}

// 거리 기반 우선순위 설정
void UHSReplicationComponent::SetDistanceBasedPriority(bool bEnable, float MaxDistance)
{
    bDistanceBasedPriority = bEnable;
    MaxReplicationDistance = MaxDistance;
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 거리 기반 우선순위 %s - 최대 거리: %.1f"), 
           bEnable ? TEXT("활성화") : TEXT("비활성화"), MaxDistance);
}

// 적응형 품질 설정
void UHSReplicationComponent::SetAdaptiveQuality(bool bEnable)
{
    bAdaptiveQuality = bEnable;
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 적응형 품질 %s"), 
           bEnable ? TEXT("활성화") : TEXT("비활성화"));
}

// 복제 빈도 설정
void UHSReplicationComponent::SetReplicationRate(EHSReplicationChannel Channel, float Rate)
{
    if (ChannelReplicationRates.Contains(Channel))
    {
        ChannelReplicationRates[Channel] = FMath::Clamp(Rate, 1.0f, 120.0f); // 1-120Hz 제한
        UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 채널 %d 복제 빈도 설정 - %.1fHz"), 
               (int32)Channel, Rate);
    }
}

// === 압축 및 최적화 ===

// 압축 설정
void UHSReplicationComponent::SetCompressionEnabled(bool bEnable, int32 NewCompressionLevel)
{
    bCompressionEnabled = bEnable;
    CompressionLevel = FMath::Clamp(NewCompressionLevel, 1, 9);
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 압축 %s - 레벨: %d"), 
           bEnable ? TEXT("활성화") : TEXT("비활성화"), NewCompressionLevel);
}

// 델타 압축 설정
void UHSReplicationComponent::SetDeltaCompressionEnabled(bool bEnable)
{
    bDeltaCompressionEnabled = bEnable;
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 델타 압축 %s"), 
           bEnable ? TEXT("활성화") : TEXT("비활성화"));
}

// 배치 처리 설정
void UHSReplicationComponent::SetBatchProcessing(bool bEnable, int32 NewBatchSize, float NewBatchTimeout)
{
    bBatchProcessingEnabled = bEnable;
    BatchSize = FMath::Max(1, NewBatchSize);
    BatchTimeout = FMath::Max(0.01f, NewBatchTimeout);
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 배치 처리 %s - 크기: %d, 타임아웃: %.3f초"), 
           bEnable ? TEXT("활성화") : TEXT("비활성화"), NewBatchSize, NewBatchTimeout);
}

// === 통계 및 모니터링 ===

// 채널별 통계 반환
FHSReplicationStats UHSReplicationComponent::GetChannelStats(EHSReplicationChannel Channel) const
{
    if (ChannelStats.Contains(Channel))
    {
        return ChannelStats[Channel];
    }
    return FHSReplicationStats();
}

// 패킷 손실률 계산
float UHSReplicationComponent::GetPacketLossRate() const
{
    int32 TotalPackets = ReplicationStats.PacketsSent + ReplicationStats.PacketsReceived;
    if (TotalPackets <= 0)
    {
        return 0.0f;
    }
    
    return (float)ReplicationStats.PacketsLost / TotalPackets;
}

// 통계 초기화
void UHSReplicationComponent::ResetStatistics()
{
    FScopeLock StatsLock(&StatisticsMutex);
    
    ReplicationStats = FHSReplicationStats();
    
    for (auto& ChannelStatPair : ChannelStats)
    {
        ChannelStatPair.Value = FHSReplicationStats();
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 통계 초기화 완료"));
}

// === 대역폭 관리 ===

// 대역폭 설정 적용
void UHSReplicationComponent::SetBandwidthSettings(const FHSBandwidthSettings& NewBandwidthSettings)
{
    BandwidthSettings = NewBandwidthSettings;
    
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 대역폭 설정 적용 - 최대: %.1fKB/s"), 
           BandwidthSettings.MaxBandwidth);
}

// 채널별 대역폭 제한 설정
void UHSReplicationComponent::SetChannelBandwidthLimit(EHSReplicationChannel Channel, float BandwidthLimit)
{
    if (BandwidthSettings.ChannelBandwidthRatio.Contains(Channel))
    {
        float Ratio = BandwidthLimit / BandwidthSettings.MaxBandwidth;
        BandwidthSettings.ChannelBandwidthRatio[Channel] = FMath::Clamp(Ratio, 0.001f, 1.0f);
        
        UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 채널 %d 대역폭 제한 - %.1fKB/s"), 
               (int32)Channel, BandwidthLimit);
    }
}

// === 유틸리티 함수들 ===

// 연결 품질 반환
int32 UHSReplicationComponent::GetConnectionQuality() const
{
    return CalculateConnectionQuality();
}

// 복제 정보 문자열 반환
FString UHSReplicationComponent::GetReplicationInfoString() const
{
    return FString::Printf(TEXT("Replication: %s | Priority: %d | Bandwidth: %.1fKB/s | Loss: %.2f%% | RTT: %.1fms"),
        bReplicationEnabled ? TEXT("Enabled") : TEXT("Disabled"),
        (int32)CurrentPriority,
        ReplicationStats.BandwidthUsage,
        GetPacketLossRate() * 100.0f,
        ReplicationStats.AverageRTT
    );
}

// === 프라이빗 함수들 ===

// 복제 시스템 초기화
void UHSReplicationComponent::InitializeReplication()
{
    // 기본 설정 적용
    for (auto& ChannelPair : ChannelReplicationState)
    {
        ChannelPair.Value = true;
    }

    // 이전 프레임 데이터 초기화
    PreviousFrameData.Empty();
    for (int32 i = 0; i < (int32)EHSReplicationChannel::RC_VFX + 1; ++i)
    {
        EHSReplicationChannel Channel = (EHSReplicationChannel)i;
        PreviousFrameData.Add(Channel, TArray<uint8>());
    }

    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 복제 시스템 초기화 완료"));
}

// 타이머 설정
void UHSReplicationComponent::SetupTimers()
{
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();

    // 통계 업데이트 타이머
    TimerManager.SetTimer(StatsUpdateTimer, this, 
                        &UHSReplicationComponent::UpdateStatistics, 
                        StatsUpdateInterval, true);

    // 배치 처리 타이머
    if (bBatchProcessingEnabled)
    {
        TimerManager.SetTimer(BatchProcessTimer, this, 
                            &UHSReplicationComponent::ProcessBatchedPackets, 
                            BatchTimeout, true);
    }

    // 우선순위 업데이트 타이머
    if (bDistanceBasedPriority)
    {
        TimerManager.SetTimer(PriorityUpdateTimer, this, 
                            &UHSReplicationComponent::UpdatePriority, 
                            PriorityUpdateInterval, true);
    }

    // 품질 조절 타이머
    if (bAdaptiveQuality)
    {
        TimerManager.SetTimer(QualityAdjustmentTimer, this, 
                            &UHSReplicationComponent::AdjustQuality, 
                            2.0f, true); // 2초마다 품질 조절
    }

    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 타이머 설정 완료"));
}

// 거리 기반 우선순위 업데이트
void UHSReplicationComponent::UpdatePriorityBasedOnDistance()
{
    if (!GetOwner())
    {
        return;
    }

    // 가장 가까운 플레이어와의 거리 계산
    float MinDistance = MaxReplicationDistance;
    
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AHSCharacterBase> ActorItr(World); ActorItr; ++ActorItr)
        {
            AHSCharacterBase* Character = *ActorItr;
            if (Character && Character != GetOwner())
            {
                float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Character->GetActorLocation());
                MinDistance = FMath::Min(MinDistance, Distance);
            }
        }
    }

    // 거리에 따른 우선순위 계산
    EHSReplicationPriority NewPriority;
    if (MinDistance < MaxReplicationDistance * 0.2f)        // 20% 이내
    {
        NewPriority = EHSReplicationPriority::RP_VeryHigh;
    }
    else if (MinDistance < MaxReplicationDistance * 0.4f)   // 40% 이내
    {
        NewPriority = EHSReplicationPriority::RP_High;
    }
    else if (MinDistance < MaxReplicationDistance * 0.7f)   // 70% 이내
    {
        NewPriority = EHSReplicationPriority::RP_Normal;
    }
    else if (MinDistance < MaxReplicationDistance)          // 100% 이내
    {
        NewPriority = EHSReplicationPriority::RP_Low;
    }
    else                                                    // 범위 밖
    {
        NewPriority = EHSReplicationPriority::RP_VeryLow;
    }

    SetReplicationPriority(NewPriority);
}

// 대역폭 기반 품질 조절
void UHSReplicationComponent::AdjustQualityBasedOnBandwidth()
{
    float CurrentUsage = ReplicationStats.BandwidthUsage;
    float MaxBandwidth = BandwidthSettings.MaxBandwidth;

    if (CurrentUsage > MaxBandwidth * 0.9f) // 90% 이상 사용 중
    {
        // 품질 낮추기
        for (auto& RatePair : ChannelReplicationRates)
        {
            RatePair.Value = FMath::Max(1.0f, RatePair.Value * 0.8f); // 20% 감소
        }
        
        UE_LOG(LogTemp, Warning, TEXT("HSReplicationComponent: 대역폭 초과로 품질 저하 - %.1f%%"), 
               (CurrentUsage / MaxBandwidth) * 100.0f);
    }
    else if (CurrentUsage < MaxBandwidth * 0.5f) // 50% 이하 사용 중
    {
        // 품질 높이기
        for (auto& RatePair : ChannelReplicationRates)
        {
            RatePair.Value = FMath::Min(120.0f, RatePair.Value * 1.1f); // 10% 증가
        }
    }
}

// 배치 처리 실행
void UHSReplicationComponent::ProcessBatchedPackets()
{
    if (PacketQueue.Num() == 0)
    {
        return;
    }

    // 우선순위별로 정렬
    PacketQueue.Sort([](const FHSReplicationPacket& A, const FHSReplicationPacket& B)
    {
        return A.Priority > B.Priority; // 높은 우선순위가 먼저
    });

    // 패킷들을 일괄 처리
    int32 ProcessedCount = 0;
    for (const FHSReplicationPacket& Packet : PacketQueue)
    {
        // 실제 데이터 전송 (간소화된 구현)
        // MulticastReceiveData(Packet, ProcessedData);
        ProcessedCount++;
    }

    // 큐 정리
    PacketQueue.Empty();

    UE_LOG(LogTemp, Verbose, TEXT("HSReplicationComponent: 배치 처리 완료 - %d개 패킷"), ProcessedCount);
}

// 통계 업데이트
void UHSReplicationComponent::UpdateStatistics()
{
    FScopeLock StatsLock(&StatisticsMutex);

    // RTT 계산 (간소화된 구현)
    if (UNetDriver* NetDriver = GetWorld()->GetNetDriver())
    {
        float TotalRTT = 0.0f;
        int32 ConnectionCount = 0;

        for (UNetConnection* Connection : NetDriver->ClientConnections)
        {
            if (Connection)
            {
                TotalRTT += Connection->AvgLag * 1000.0f; // 밀리초 단위
                ConnectionCount++;
            }
        }

        if (ConnectionCount > 0)
        {
            ReplicationStats.AverageRTT = TotalRTT / ConnectionCount;
        }
    }

    // 대역폭 사용률 계산
    static int64 LastTotalBytes = 0;
    static float LastUpdateTime = 0.0f;
    
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float TimeDelta = CurrentTime - LastUpdateTime;
    
    if (TimeDelta > 0.0f)
    {
        int64 BytesDelta = ReplicationStats.TotalBytesSent - LastTotalBytes;
        ReplicationStats.BandwidthUsage = (BytesDelta / TimeDelta) / 1024.0f; // KB/s
        
        LastTotalBytes = ReplicationStats.TotalBytesSent;
        LastUpdateTime = CurrentTime;
    }

    // 복제 빈도 계산
    static int32 LastPacketCount = 0;
    static float LastRateUpdate = 0.0f;
    
    if (CurrentTime - LastRateUpdate >= 1.0f) // 1초마다 업데이트
    {
        int32 PacketDelta = ReplicationStats.PacketsSent - LastPacketCount;
        ReplicationStats.ReplicationRate = PacketDelta;
        
        LastPacketCount = ReplicationStats.PacketsSent;
        LastRateUpdate = CurrentTime;
    }

    // 이벤트 브로드캐스트
    OnReplicationStatsUpdated.Broadcast(ReplicationStats);
}

// 우선순위 업데이트
void UHSReplicationComponent::UpdatePriority()
{
    if (bDistanceBasedPriority)
    {
        UpdatePriorityBasedOnDistance();
    }
}

// 품질 조절
void UHSReplicationComponent::AdjustQuality()
{
    if (bAdaptiveQuality)
    {
        AdjustQualityBasedOnBandwidth();
    }
}

// 데이터 압축
TArray<uint8> UHSReplicationComponent::CompressData(const TArray<uint8>& Data) const
{
    if (Data.Num() == 0)
    {
        return Data;
    }

    // Unreal Engine의 압축 시스템 사용
    TArray<uint8> CompressedData;
    int32 UncompressedSize = Data.Num();
    int32 CompressedSize = FMath::Max(1, UncompressedSize); // 최소 크기 보장

    // 압축 버퍼 할당
    CompressedData.SetNumUninitialized(CompressedSize);

    // zlib 압축 사용 (UE5에서 기본 지원)
    if (FCompression::CompressMemory(NAME_Zlib, CompressedData.GetData(), CompressedSize, 
                                   Data.GetData(), UncompressedSize, COMPRESS_BiasMemory))
    {
        CompressedData.SetNum(CompressedSize);
        return CompressedData;
    }

    // 압축 실패 시 원본 반환
    return Data;
}

// 데이터 압축 해제
TArray<uint8> UHSReplicationComponent::DecompressData(const TArray<uint8>& CompressedData) const
{
    if (CompressedData.Num() == 0)
    {
        return CompressedData;
    }

    // 압축 해제 구현 (간소화된 버전)
    // 실제 구현에서는 압축 시 저장한 메타데이터를 사용해야 함
    TArray<uint8> DecompressedData;
    
    // 압축 해제 로직
    // ...

    return DecompressedData;
}

// 델타 압축 계산
TArray<uint8> UHSReplicationComponent::CalculateDelta(const TArray<uint8>& OldData, const TArray<uint8>& NewData) const
{
    TArray<uint8> DeltaData;
    
    // 간단한 델타 압축 구현 (XOR 기반)
    int32 MinSize = FMath::Min(OldData.Num(), NewData.Num());
    DeltaData.Reserve(NewData.Num());
    
    // XOR 델타 계산
    for (int32 i = 0; i < MinSize; ++i)
    {
        uint8 Delta = OldData[i] ^ NewData[i];
        DeltaData.Add(Delta);
    }
    
    // 새로운 데이터가 더 큰 경우 나머지 추가
    for (int32 i = MinSize; i < NewData.Num(); ++i)
    {
        DeltaData.Add(NewData[i]);
    }
    
    return DeltaData;
}

// 델타 적용
TArray<uint8> UHSReplicationComponent::ApplyDelta(const TArray<uint8>& OldData, const TArray<uint8>& DeltaData) const
{
    TArray<uint8> NewData;
    NewData.Reserve(DeltaData.Num());
    
    // XOR 델타 적용
    int32 MinSize = FMath::Min(OldData.Num(), DeltaData.Num());
    for (int32 i = 0; i < MinSize; ++i)
    {
        uint8 NewByte = OldData[i] ^ DeltaData[i];
        NewData.Add(NewByte);
    }
    
    // 델타가 더 큰 경우 나머지 추가
    for (int32 i = MinSize; i < DeltaData.Num(); ++i)
    {
        NewData.Add(DeltaData[i]);
    }
    
    return NewData;
}

// 패킷 유효성 검사
bool UHSReplicationComponent::ValidatePacket(const FHSReplicationPacket& Packet) const
{
    // 기본 유효성 검사
    if (Packet.PacketID <= 0 || Packet.DataSize < 0)
    {
        return false;
    }
    
    // 최대 패킷 크기 확인
    const int32 MaxPacketSize = 1024 * 1024; // 1MB
    if (Packet.DataSize > MaxPacketSize)
    {
        return false;
    }
    
    return true;
}

// 대역폭 제한 확인
bool UHSReplicationComponent::CheckBandwidthLimit(EHSReplicationChannel Channel, int32 DataSize) const
{
    if (!BandwidthSettings.ChannelBandwidthRatio.Contains(Channel))
    {
        return true; // 제한 없음
    }
    
    float ChannelLimit = BandwidthSettings.MaxBandwidth * BandwidthSettings.ChannelBandwidthRatio[Channel];
    float CurrentUsage = CalculateChannelBandwidthUsage(Channel);
    
    return (CurrentUsage + DataSize / 1024.0f) <= ChannelLimit;
}

// 채널별 대역폭 사용량 계산
float UHSReplicationComponent::CalculateChannelBandwidthUsage(EHSReplicationChannel Channel) const
{
    if (ChannelStats.Contains(Channel))
    {
        return ChannelStats[Channel].BandwidthUsage;
    }
    return 0.0f;
}

// 연결 품질 계산
int32 UHSReplicationComponent::CalculateConnectionQuality() const
{
    float PacketLoss = GetPacketLossRate();
    float RTT = ReplicationStats.AverageRTT;
    
    // 연결 품질 점수 계산 (0-4)
    int32 Quality = 4; // 시작점: 최고 품질
    
    // 패킷 손실률에 따른 점수 감소
    if (PacketLoss > 0.1f)      Quality -= 3; // 10% 이상
    else if (PacketLoss > 0.05f) Quality -= 2; // 5% 이상
    else if (PacketLoss > 0.02f) Quality -= 1; // 2% 이상
    
    // RTT에 따른 점수 감소
    if (RTT > 300.0f)      Quality -= 2; // 300ms 이상
    else if (RTT > 150.0f) Quality -= 1; // 150ms 이상
    
    return FMath::Clamp(Quality, 0, 4);
}

// === 네트워크 RPC 함수들 ===

// 멀티캐스트 데이터 수신
void UHSReplicationComponent::MulticastReceiveData_Implementation(const FHSReplicationPacket& Packet, const TArray<uint8>& Data)
{
    // 클라이언트에서 데이터 수신 처리
    if (!GetOwner()->HasAuthority())
    {
        FScopeLock StatsLock(&StatisticsMutex);
        ReplicationStats.PacketsReceived++;
        ReplicationStats.TotalBytesReceived += Data.Num();
        
        OnReplicationPacketReceived.Broadcast(Packet, true);
        
        // 서버에 수신 확인 전송
        ServerReceiveAcknowledgment(Packet.PacketID, true);
    }
}

// 서버 수신 확인
void UHSReplicationComponent::ServerReceiveAcknowledgment_Implementation(int32 PacketID, bool bReceived)
{
    if (GetOwner()->HasAuthority())
    {
        // 수신 확인 처리
        if (!bReceived)
        {
            FScopeLock StatsLock(&StatisticsMutex);
            ReplicationStats.PacketsLost++;
        }
    }
}

// === 네트워크 복제 콜백 함수들 ===

void UHSReplicationComponent::OnRep_CurrentPriority()
{
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 우선순위 복제됨 - %s: %d"), 
           *GetOwner()->GetName(), (int32)CurrentPriority);
}

void UHSReplicationComponent::OnRep_ReplicationEnabled()
{
    UE_LOG(LogTemp, Log, TEXT("HSReplicationComponent: 복제 상태 복제됨 - %s: %s"), 
           *GetOwner()->GetName(), bReplicationEnabled ? TEXT("활성화") : TEXT("비활성화"));
}

void UHSReplicationComponent::OnRep_ReplicationStats()
{
    OnReplicationStatsUpdated.Broadcast(ReplicationStats);
}

// === 디버그 및 로깅 함수들 ===

// 복제 상태 로그 출력
void UHSReplicationComponent::LogReplicationState() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 복제 상태: %s ==="), *GetOwner()->GetName());
    UE_LOG(LogTemp, Warning, TEXT("활성화: %s, 우선순위: %d"), 
           bReplicationEnabled ? TEXT("예") : TEXT("아니오"), (int32)CurrentPriority);
    UE_LOG(LogTemp, Warning, TEXT("거리 기반 우선순위: %s"), 
           bDistanceBasedPriority ? TEXT("활성화") : TEXT("비활성화"));
    UE_LOG(LogTemp, Warning, TEXT("적응형 품질: %s"), 
           bAdaptiveQuality ? TEXT("활성화") : TEXT("비활성화"));
    UE_LOG(LogTemp, Warning, TEXT("압축: %s (레벨 %d)"), 
           bCompressionEnabled ? TEXT("활성화") : TEXT("비활성화"), CompressionLevel);
    UE_LOG(LogTemp, Warning, TEXT("배치 처리: %s"), 
           bBatchProcessingEnabled ? TEXT("활성화") : TEXT("비활성화"));
}

// 통계 로그 출력
void UHSReplicationComponent::LogReplicationStatistics() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== 복제 통계: %s ==="), *GetOwner()->GetName());
    UE_LOG(LogTemp, Warning, TEXT("패킷: 송신 %d, 수신 %d, 손실 %d"), 
           ReplicationStats.PacketsSent, ReplicationStats.PacketsReceived, ReplicationStats.PacketsLost);
    UE_LOG(LogTemp, Warning, TEXT("데이터: 송신 %.1fKB, 수신 %.1fKB"), 
           ReplicationStats.TotalBytesSent / 1024.0f, ReplicationStats.TotalBytesReceived / 1024.0f);
    UE_LOG(LogTemp, Warning, TEXT("대역폭: %.1fKB/s, RTT: %.1fms"), 
           ReplicationStats.BandwidthUsage, ReplicationStats.AverageRTT);
    UE_LOG(LogTemp, Warning, TEXT("복제 빈도: %.1f패킷/초"), ReplicationStats.ReplicationRate);
    UE_LOG(LogTemp, Warning, TEXT("연결 품질: %d/4"), GetConnectionQuality());
}

// === 메모리 최적화 관련 ===

// 사용하지 않는 데이터 정리
void UHSReplicationComponent::CleanupUnusedData()
{
    // 오래된 이전 프레임 데이터 정리
    const float MaxAge = 5.0f; // 5초 이상 된 데이터 제거
    float CurrentTime = GetWorld()->GetTimeSeconds();
    
    // 패킷 큐 최적화
    OptimizePacketQueue();
    
    // 메모리 압축
    PacketQueue.Shrink();
}

// 패킷 큐 최적화
void UHSReplicationComponent::OptimizePacketQueue()
{
    if (PacketQueue.Num() > BatchSize * 2)
    {
        // 오래된 패킷 제거
        float CurrentTime = GetWorld()->GetTimeSeconds();
        PacketQueue.RemoveAll([CurrentTime](const FHSReplicationPacket& Packet)
        {
            return (CurrentTime - Packet.Timestamp) > 1.0f; // 1초 이상 된 패킷 제거
        });
        
        PacketQueue.Shrink();
    }
}
