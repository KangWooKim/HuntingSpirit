// HuntingSpirit 게임의 플레이어 간 통신 시스템
// 채팅, 음성채팅, 핑 시스템 등 멀티플레이어 소통 기능 제공
// 작성자: KangWooKim (https://github.com/KangWooKim)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Sound/SoundBase.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "HSCommunicationSystem.generated.h"

// 채팅 메시지 타입
UENUM(BlueprintType)
enum class EHSChatType : uint8
{
    None = 0,
    TeamChat,        // 팀 채팅
    GlobalChat,      // 전체 채팅
    SystemMessage,   // 시스템 메시지
    VoiceChat,       // 음성 채팅 상태
    Whisper          // 귓속말
};

// 핑 타입
UENUM(BlueprintType)
enum class EHSPingType : uint8
{
    None = 0,
    Attack,          // 공격 핑
    Defend,          // 방어 핑
    Help,            // 도움 요청
    Warning,         // 경고
    Item,            // 아이템 위치
    Enemy,           // 적 위치
    Move,            // 이동 지시
    Gather           // 채집 지시
};

// 음성 채팅 상태
UENUM(BlueprintType)
enum class EHSVoiceChatState : uint8
{
    Disconnected = 0,
    Connecting,
    Connected,
    Speaking,
    Muted,
    Error
};

// 채팅 메시지 구조체
USTRUCT(BlueprintType)
struct FHSChatMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SenderName;

    UPROPERTY(BlueprintReadOnly)
    FString Message;

    UPROPERTY(BlueprintReadOnly)
    EHSChatType ChatType;

    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;

    UPROPERTY(BlueprintReadOnly)
    FLinearColor MessageColor;

    UPROPERTY(BlueprintReadOnly)
    int32 SenderPlayerID;

    FHSChatMessage()
    {
        SenderName = TEXT("");
        Message = TEXT("");
        ChatType = EHSChatType::None;
        Timestamp = FDateTime::Now();
        MessageColor = FLinearColor::White;
        SenderPlayerID = -1;
    }
};

// 핑 데이터 구조체
USTRUCT(BlueprintType)
struct FHSPingData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector WorldLocation;

    UPROPERTY(BlueprintReadOnly)
    EHSPingType PingType;

    UPROPERTY(BlueprintReadOnly)
    FString SenderName;

    UPROPERTY(BlueprintReadOnly)
    int32 SenderPlayerID;

    UPROPERTY(BlueprintReadOnly)
    FDateTime CreationTime;

    UPROPERTY(BlueprintReadOnly)
    float Duration;

    UPROPERTY(BlueprintReadOnly)
    bool bIsVisible;

    FHSPingData()
    {
        WorldLocation = FVector::ZeroVector;
        PingType = EHSPingType::None;
        SenderName = TEXT("");
        SenderPlayerID = -1;
        CreationTime = FDateTime::Now();
        Duration = 5.0f;
        bIsVisible = true;
    }
};

// 음성 채팅 플레이어 정보
USTRUCT(BlueprintType)
struct FHSVoiceChatPlayerInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerID;

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly)
    EHSVoiceChatState VoiceState;

    UPROPERTY(BlueprintReadOnly)
    float VoiceLevel;

    UPROPERTY(BlueprintReadOnly)
    bool bIsMuted;

    UPROPERTY(BlueprintReadOnly)
    bool bIsDeafened;

    FHSVoiceChatPlayerInfo()
    {
        PlayerID = -1;
        PlayerName = TEXT("");
        VoiceState = EHSVoiceChatState::Disconnected;
        VoiceLevel = 0.0f;
        bIsMuted = false;
        bIsDeafened = false;
    }
};

// 채팅 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatMessageReceived, const FHSChatMessage&, ChatMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChatMessageSent, const FString&, Message, EHSChatType, ChatType);

// 핑 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPingCreated, const FHSPingData&, PingData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPingRemoved, int32, PingID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPingClicked, const FHSPingData&, PingData, int32, ClickerPlayerID);

// 음성 채팅 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceChatStateChanged, int32, PlayerID, EHSVoiceChatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceLevelChanged, int32, PlayerID, float, VoiceLevel);

/**
 * 플레이어 간 통신 시스템
 * 채팅, 음성채팅, 핑 시스템을 통합 관리
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSCommunicationSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSCommunicationSystem();

    // UGameInstanceSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 채팅 시스템 ===
    
    // 채팅 메시지 전송
    UFUNCTION(BlueprintCallable, Category = "Communication|Chat")
    bool SendChatMessage(const FString& Message, EHSChatType ChatType = EHSChatType::TeamChat);

    // 채팅 메시지 수신 처리
    UFUNCTION(BlueprintCallable, Category = "Communication|Chat")
    void ReceiveChatMessage(const FHSChatMessage& ChatMessage);

    // 귓속말 전송
    UFUNCTION(BlueprintCallable, Category = "Communication|Chat")
    bool SendWhisper(const FString& TargetPlayerName, const FString& Message);

    // 시스템 메시지 생성
    UFUNCTION(BlueprintCallable, Category = "Communication|Chat")
    void CreateSystemMessage(const FString& Message, const FLinearColor& Color = FLinearColor::Yellow);

    // 채팅 히스토리 가져오기
    UFUNCTION(BlueprintCallable, Category = "Communication|Chat")
    TArray<FHSChatMessage> GetChatHistory(int32 MaxMessages = 50) const;

    // 채팅 히스토리 지우기
    UFUNCTION(BlueprintCallable, Category = "Communication|Chat")
    void ClearChatHistory();

    // === 핑 시스템 ===

    // 핑 생성
    UFUNCTION(BlueprintCallable, Category = "Communication|Ping")
    int32 CreatePing(const FVector& WorldLocation, EHSPingType PingType, float Duration = 5.0f);

    // 핑 제거
    UFUNCTION(BlueprintCallable, Category = "Communication|Ping")
    bool RemovePing(int32 PingID);

    // 핑 클릭 처리
    UFUNCTION(BlueprintCallable, Category = "Communication|Ping")
    void HandlePingClicked(int32 PingID, int32 ClickerPlayerID);

    // 활성 핑 목록 가져오기
    UFUNCTION(BlueprintCallable, Category = "Communication|Ping")
    TArray<FHSPingData> GetActivePings() const;

    // 핑 업데이트 (만료 확인)
    UFUNCTION(BlueprintCallable, Category = "Communication|Ping")
    void UpdatePings();

    // === 음성 채팅 시스템 ===

    // 음성 채팅 시작
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    bool StartVoiceChat();

    // 음성 채팅 중지
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    bool StopVoiceChat();

    // 마이크 음소거 토글
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    bool ToggleMute();

    // 스피커 음소거 토글
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    bool ToggleDeafen();

    // 음성 레벨 설정
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    void SetVoiceLevel(float Level);

    // 플레이어 음성 상태 가져오기
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    FHSVoiceChatPlayerInfo GetPlayerVoiceInfo(int32 PlayerID) const;

    // 모든 플레이어 음성 상태 가져오기
    UFUNCTION(BlueprintCallable, Category = "Communication|Voice")
    TArray<FHSVoiceChatPlayerInfo> GetAllVoiceInfo() const;

    // === 유틸리티 함수 ===

    // 플레이어 이름으로 ID 찾기
    UFUNCTION(BlueprintCallable, Category = "Communication|Utility")
    int32 GetPlayerIDByName(const FString& PlayerName) const;

    // 플레이어 ID로 이름 찾기
    UFUNCTION(BlueprintCallable, Category = "Communication|Utility")
    FString GetPlayerNameByID(int32 PlayerID) const;

    // 욕설 필터링
    UFUNCTION(BlueprintCallable, Category = "Communication|Utility")
    FString FilterProfanity(const FString& Input) const;

    // 스팸 방지 확인
    UFUNCTION(BlueprintCallable, Category = "Communication|Utility")
    bool IsSpamFiltered(int32 PlayerID) const;

    // === 델리게이트 ===

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnChatMessageReceived OnChatMessageReceived;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnChatMessageSent OnChatMessageSent;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnPingCreated OnPingCreated;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnPingRemoved OnPingRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnPingClicked OnPingClicked;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnVoiceChatStateChanged OnVoiceChatStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnVoiceLevelChanged OnVoiceLevelChanged;

private:
    // === 채팅 시스템 데이터 ===
    
    // 채팅 히스토리
    UPROPERTY()
    TArray<FHSChatMessage> ChatHistory;

    // 최대 채팅 히스토리 크기
    UPROPERTY()
    int32 MaxChatHistory;

    // 스팸 방지 데이터
    mutable TMap<int32, FDateTime> LastMessageTime;
    mutable TMap<int32, int32> MessageCount;

    // === 핑 시스템 데이터 ===

    // 활성 핑 목록
    UPROPERTY()
    TMap<int32, FHSPingData> ActivePings;

    // 다음 핑 ID
    UPROPERTY()
    int32 NextPingID;

    // 핑 업데이트 타이머
    UPROPERTY()
    FTimerHandle PingUpdateTimer;

    // === 음성 채팅 데이터 ===

    // 플레이어 음성 정보
    UPROPERTY()
    TMap<int32, FHSVoiceChatPlayerInfo> VoiceChatPlayers;

    // 음성 채팅 활성화 상태
    UPROPERTY()
    bool bVoiceChatEnabled;

    // 로컬 플레이어 음성 상태
    UPROPERTY()
    bool bLocalPlayerMuted;

    UPROPERTY()
    bool bLocalPlayerDeafened;

    UPROPERTY()
    float LocalVoiceLevel;

    // === 설정 ===

    // 욕설 필터링 활성화
    UPROPERTY()
    bool bProfanityFilterEnabled;

    // 스팸 방지 활성화
    UPROPERTY()
    bool bSpamFilterEnabled;

    // 스팸 방지 시간 제한 (초)
    UPROPERTY()
    float SpamTimeLimit;

    // 스팸 방지 메시지 제한
    UPROPERTY()
    int32 SpamMessageLimit;

    // === 내부 함수 ===

    // 채팅 메시지 검증
    bool ValidateChatMessage(const FString& Message, int32 SenderID) const;

    // 핑 만료 확인
    void CheckExpiredPings();

    // 음성 채팅 초기화
    void InitializeVoiceChat();

    // 음성 채팅 정리
    void CleanupVoiceChat();

    // 플레이어 음성 상태 업데이트
    void UpdatePlayerVoiceState(int32 PlayerID, EHSVoiceChatState NewState);

    // 현업 최적화: 메시지 풀링
    UPROPERTY()
    TArray<FHSChatMessage> MessagePool;

    // 현업 최적화: 핑 풀링
    UPROPERTY()
    TArray<FHSPingData> PingPool;

    // 성능 캐싱: 플레이어 정보 캐시
    mutable TMap<FString, int32> PlayerNameToIDCache;
    mutable TMap<int32, FString> PlayerIDToNameCache;
    mutable FDateTime LastPlayerCacheUpdate;

    // 네트워크 최적화: 배치 처리
    UPROPERTY()
    TArray<FHSChatMessage> PendingChatMessages;

    UPROPERTY()
    TArray<FHSPingData> PendingPings;

    UPROPERTY()
    FTimerHandle BatchProcessTimer;

    void ProcessPendingMessages();

    friend class AHSGameStateBase;
    friend class UHSTeamManager;
};
