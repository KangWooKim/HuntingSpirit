// HuntingSpirit 게임의 플레이어 간 통신 시스템 구현
// 채팅, 음성채팅, 핑 시스템의 핵심 동작 로직
// 작성자: KangWooKim (https://github.com/KangWooKim)

#include "HSCommunicationSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UHSCommunicationSystem::UHSCommunicationSystem()
{
    // 기본 설정값 초기화
    MaxChatHistory = 100;
    NextPingID = 1;
    bVoiceChatEnabled = false;
    bLocalPlayerMuted = false;
    bLocalPlayerDeafened = false;
    LocalVoiceLevel = 1.0f;
    
    // 필터링 설정
    bProfanityFilterEnabled = true;
    bSpamFilterEnabled = true;
    SpamTimeLimit = 2.0f;      // 2초 내
    SpamMessageLimit = 3;       // 3개 메시지까지 허용
    
    // 풀링 초기화
    MessagePool.Reserve(50);
    PingPool.Reserve(20);
    
    // 캐시 초기화
    LastPlayerCacheUpdate = FDateTime::MinValue();
}

void UHSCommunicationSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 통신 시스템 초기화 시작"));
    
    // 핑 업데이트 타이머 설정 (0.5초마다 만료된 핑 확인)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PingUpdateTimer,
            this,
            &UHSCommunicationSystem::CheckExpiredPings,
            0.5f,
            true
        );
        
        // 배치 처리 타이머 설정 (0.1초마다 대기 중인 메시지/핑 처리)
        World->GetTimerManager().SetTimer(
            BatchProcessTimer,
            this,
            &UHSCommunicationSystem::ProcessPendingMessages,
            0.1f,
            true
        );
    }
    
    // 음성 채팅 초기화
    InitializeVoiceChat();
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 통신 시스템 초기화 완료"));
}

void UHSCommunicationSystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 통신 시스템 정리 시작"));
    
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PingUpdateTimer);
        World->GetTimerManager().ClearTimer(BatchProcessTimer);
    }
    
    // 음성 채팅 정리
    CleanupVoiceChat();
    
    // 데이터 정리
    ChatHistory.Empty();
    ActivePings.Empty();
    VoiceChatPlayers.Empty();
    MessagePool.Empty();
    PingPool.Empty();
    
    // 캐시 정리
    PlayerNameToIDCache.Empty();
    PlayerIDToNameCache.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 통신 시스템 정리 완료"));
    
    Super::Deinitialize();
}

// === 채팅 시스템 구현 ===

bool UHSCommunicationSystem::SendChatMessage(const FString& Message, EHSChatType ChatType)
{
    // 메시지 검증
    if (Message.IsEmpty() || Message.Len() > 500)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCommunicationSystem: 잘못된 메시지 길이: %d"), Message.Len());
        return false;
    }
    
    // 현재 플레이어 정보 가져오기
    int32 LocalPlayerID = -1;
    FString LocalPlayerName = TEXT("Unknown");
    
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                LocalPlayerID = PS->GetPlayerId();
                LocalPlayerName = PS->GetPlayerName();
            }
        }
    }
    
    // 스팸 필터링 확인
    if (bSpamFilterEnabled && IsSpamFiltered(LocalPlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCommunicationSystem: 스팸 필터에 의해 메시지 차단됨"));
        return false;
    }
    
    // 욕설 필터링
    FString FilteredMessage = bProfanityFilterEnabled ? FilterProfanity(Message) : Message;
    
    // 채팅 메시지 생성
    FHSChatMessage ChatMessage;
    ChatMessage.SenderName = LocalPlayerName;
    ChatMessage.Message = FilteredMessage;
    ChatMessage.ChatType = ChatType;
    ChatMessage.Timestamp = FDateTime::Now();
    ChatMessage.SenderPlayerID = LocalPlayerID;
    
    // 메시지 타입별 색상 설정
    switch (ChatType)
    {
        case EHSChatType::TeamChat:
            ChatMessage.MessageColor = FLinearColor::Green;
            break;
        case EHSChatType::GlobalChat:
            ChatMessage.MessageColor = FLinearColor::White;
            break;
        case EHSChatType::SystemMessage:
            ChatMessage.MessageColor = FLinearColor::Yellow;
            break;
        case EHSChatType::Whisper:
            ChatMessage.MessageColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f); // Magenta
            break;
        default:
            ChatMessage.MessageColor = FLinearColor::White;
            break;
    }
    
    // 배치 처리를 위해 대기 큐에 추가
    PendingChatMessages.Add(ChatMessage);
    
    // 스팸 방지 데이터 업데이트
    FDateTime CurrentTime = FDateTime::Now();
    LastMessageTime.Add(LocalPlayerID, CurrentTime);
    
    int32& MsgCount = MessageCount.FindOrAdd(LocalPlayerID, 0);
    MsgCount++;
    
    // 델리게이트 호출
    OnChatMessageSent.Broadcast(FilteredMessage, ChatType);
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 채팅 메시지 전송됨 - %s: %s"), 
           *LocalPlayerName, *FilteredMessage);
    
    return true;
}

void UHSCommunicationSystem::ReceiveChatMessage(const FHSChatMessage& ChatMessage)
{
    // 메시지 검증
    if (ChatMessage.Message.IsEmpty() || ChatMessage.SenderName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCommunicationSystem: 잘못된 채팅 메시지 수신됨"));
        return;
    }
    
    // 오브젝트 풀링 사용
    FHSChatMessage* PooledMessage = nullptr;
    if (MessagePool.Num() > 0)
    {
        FHSChatMessage TempMessage = MessagePool.Pop();
        PooledMessage = &TempMessage;
        *PooledMessage = ChatMessage;
        ChatHistory.Add(*PooledMessage);
        PooledMessage = &ChatHistory.Last();
    }
    else
    {
        ChatHistory.Add(ChatMessage);
        PooledMessage = &ChatHistory.Last();
    }
    
    // 채팅 히스토리 크기 제한
    if (ChatHistory.Num() > MaxChatHistory)
    {
        // 가장 오래된 메시지를 풀로 반환
        if (ChatHistory.Num() > 0)
        {
            MessagePool.Add(ChatHistory[0]);
            ChatHistory.RemoveAt(0);
        }
    }
    
    // 델리게이트 호출
    OnChatMessageReceived.Broadcast(ChatMessage);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("HSCommunicationSystem: 채팅 메시지 수신됨 - %s: %s"), 
           *ChatMessage.SenderName, *ChatMessage.Message);
}

bool UHSCommunicationSystem::SendWhisper(const FString& TargetPlayerName, const FString& Message)
{
    // 대상 플레이어 확인
    int32 TargetPlayerID = GetPlayerIDByName(TargetPlayerName);
    if (TargetPlayerID == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCommunicationSystem: 귓속말 대상 플레이어를 찾을 수 없음: %s"), 
               *TargetPlayerName);
        return false;
    }
    
    // 귓속말 메시지 생성
    FString WhisperMessage = FString::Printf(TEXT("[귓속말] %s"), *Message);
    
    return SendChatMessage(WhisperMessage, EHSChatType::Whisper);
}

void UHSCommunicationSystem::CreateSystemMessage(const FString& Message, const FLinearColor& Color)
{
    FHSChatMessage SystemMessage;
    SystemMessage.SenderName = TEXT("시스템");
    SystemMessage.Message = Message;
    SystemMessage.ChatType = EHSChatType::SystemMessage;
    SystemMessage.Timestamp = FDateTime::Now();
    SystemMessage.MessageColor = Color;
    SystemMessage.SenderPlayerID = -1;
    
    ReceiveChatMessage(SystemMessage);
}

TArray<FHSChatMessage> UHSCommunicationSystem::GetChatHistory(int32 MaxMessages) const
{
    TArray<FHSChatMessage> Result;
    
    int32 StartIndex = FMath::Max(0, ChatHistory.Num() - MaxMessages);
    for (int32 i = StartIndex; i < ChatHistory.Num(); i++)
    {
        Result.Add(ChatHistory[i]);
    }
    
    return Result;
}

void UHSCommunicationSystem::ClearChatHistory()
{
    // 메시지들을 풀로 반환
    for (const FHSChatMessage& Message : ChatHistory)
    {
        MessagePool.Add(Message);
    }
    
    ChatHistory.Empty();
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 채팅 히스토리 클리어됨"));
}

// === 핑 시스템 구현 ===

int32 UHSCommunicationSystem::CreatePing(const FVector& WorldLocation, EHSPingType PingType, float Duration)
{
    // 핑 데이터 생성
    FHSPingData PingData;
    PingData.WorldLocation = WorldLocation;
    PingData.PingType = PingType;
    PingData.Duration = Duration;
    PingData.CreationTime = FDateTime::Now();
    PingData.bIsVisible = true;
    
    // 현재 플레이어 정보 설정
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                PingData.SenderPlayerID = PS->GetPlayerId();
                PingData.SenderName = PS->GetPlayerName();
            }
        }
    }
    
    // 핑 ID 할당 및 저장
    int32 PingID = NextPingID++;
    ActivePings.Add(PingID, PingData);
    
    // 델리게이트 호출
    OnPingCreated.Broadcast(PingData);
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 핑 생성됨 - ID: %d, 타입: %d, 위치: %s"), 
           PingID, (int32)PingType, *WorldLocation.ToString());
    
    return PingID;
}

bool UHSCommunicationSystem::RemovePing(int32 PingID)
{
    if (ActivePings.Contains(PingID))
    {
        // 사용이 끝난 핑 데이터를 풀로 반환
        FHSPingData RemovedPing = ActivePings[PingID];
        PingPool.Add(RemovedPing);
        
        ActivePings.Remove(PingID);
        
        // 델리게이트 호출
        OnPingRemoved.Broadcast(PingID);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("HSCommunicationSystem: 핑 제거됨 - ID: %d"), PingID);
        return true;
    }
    
    return false;
}

void UHSCommunicationSystem::HandlePingClicked(int32 PingID, int32 ClickerPlayerID)
{
    if (FHSPingData* PingData = ActivePings.Find(PingID))
    {
        // 델리게이트 호출
        OnPingClicked.Broadcast(*PingData, ClickerPlayerID);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("HSCommunicationSystem: 핑 클릭됨 - ID: %d, 클릭한 플레이어: %d"), 
               PingID, ClickerPlayerID);
    }
}

TArray<FHSPingData> UHSCommunicationSystem::GetActivePings() const
{
    TArray<FHSPingData> Result;
    
    for (const auto& PingPair : ActivePings)
    {
        if (PingPair.Value.bIsVisible)
        {
            Result.Add(PingPair.Value);
        }
    }
    
    return Result;
}

void UHSCommunicationSystem::UpdatePings()
{
    CheckExpiredPings();
}

// === 음성 채팅 시스템 구현 ===

bool UHSCommunicationSystem::StartVoiceChat()
{
    if (bVoiceChatEnabled)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCommunicationSystem: 음성 채팅이 이미 활성화됨"));
        return false;
    }
    
    // 음성 채팅 초기화 시도
    // UE5 VoiceChat 서브시스템 연동 지점
    bVoiceChatEnabled = true;
    
    // 로컬 플레이어 음성 정보 초기화
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                int32 LocalPlayerID = PS->GetPlayerId();
                
                FHSVoiceChatPlayerInfo LocalVoiceInfo;
                LocalVoiceInfo.PlayerID = LocalPlayerID;
                LocalVoiceInfo.PlayerName = PS->GetPlayerName();
                LocalVoiceInfo.VoiceState = EHSVoiceChatState::Connected;
                LocalVoiceInfo.VoiceLevel = LocalVoiceLevel;
                LocalVoiceInfo.bIsMuted = bLocalPlayerMuted;
                LocalVoiceInfo.bIsDeafened = bLocalPlayerDeafened;
                
                VoiceChatPlayers.Add(LocalPlayerID, LocalVoiceInfo);
                
                // 델리게이트 호출
                OnVoiceChatStateChanged.Broadcast(LocalPlayerID, EHSVoiceChatState::Connected);
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 음성 채팅 시작됨"));
    return true;
}

bool UHSCommunicationSystem::StopVoiceChat()
{
    if (!bVoiceChatEnabled)
    {
        return false;
    }
    
    // 모든 플레이어의 음성 상태를 연결 해제로 변경
    for (auto& VoicePair : VoiceChatPlayers)
    {
        UpdatePlayerVoiceState(VoicePair.Key, EHSVoiceChatState::Disconnected);
    }
    
    bVoiceChatEnabled = false;
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 음성 채팅 중지됨"));
    return true;
}

bool UHSCommunicationSystem::ToggleMute()
{
    bLocalPlayerMuted = !bLocalPlayerMuted;
    
    // 로컬 플레이어 음성 상태 업데이트
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                int32 LocalPlayerID = PS->GetPlayerId();
                
                if (FHSVoiceChatPlayerInfo* VoiceInfo = VoiceChatPlayers.Find(LocalPlayerID))
                {
                    VoiceInfo->bIsMuted = bLocalPlayerMuted;
                    
                    EHSVoiceChatState NewState = bLocalPlayerMuted ? 
                        EHSVoiceChatState::Muted : EHSVoiceChatState::Connected;
                    
                    UpdatePlayerVoiceState(LocalPlayerID, NewState);
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 마이크 음소거 %s"), 
           bLocalPlayerMuted ? TEXT("활성화") : TEXT("비활성화"));
    
    return bLocalPlayerMuted;
}

bool UHSCommunicationSystem::ToggleDeafen()
{
    bLocalPlayerDeafened = !bLocalPlayerDeafened;
    
    // 로컬 플레이어 음성 상태 업데이트
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                int32 LocalPlayerID = PS->GetPlayerId();
                
                if (FHSVoiceChatPlayerInfo* VoiceInfo = VoiceChatPlayers.Find(LocalPlayerID))
                {
                    VoiceInfo->bIsDeafened = bLocalPlayerDeafened;
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 스피커 음소거 %s"), 
           bLocalPlayerDeafened ? TEXT("활성화") : TEXT("비활성화"));
    
    return bLocalPlayerDeafened;
}

void UHSCommunicationSystem::SetVoiceLevel(float Level)
{
    LocalVoiceLevel = FMath::Clamp(Level, 0.0f, 2.0f);
    
    // 로컬 플레이어 음성 레벨 업데이트
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
            {
                int32 LocalPlayerID = PS->GetPlayerId();
                
                if (FHSVoiceChatPlayerInfo* VoiceInfo = VoiceChatPlayers.Find(LocalPlayerID))
                {
                    VoiceInfo->VoiceLevel = LocalVoiceLevel;
                    
                    // 델리게이트 호출
                    OnVoiceLevelChanged.Broadcast(LocalPlayerID, LocalVoiceLevel);
                }
            }
        }
    }
}

FHSVoiceChatPlayerInfo UHSCommunicationSystem::GetPlayerVoiceInfo(int32 PlayerID) const
{
    if (const FHSVoiceChatPlayerInfo* VoiceInfo = VoiceChatPlayers.Find(PlayerID))
    {
        return *VoiceInfo;
    }
    
    return FHSVoiceChatPlayerInfo();
}

TArray<FHSVoiceChatPlayerInfo> UHSCommunicationSystem::GetAllVoiceInfo() const
{
    TArray<FHSVoiceChatPlayerInfo> Result;
    
    for (const auto& VoicePair : VoiceChatPlayers)
    {
        Result.Add(VoicePair.Value);
    }
    
    return Result;
}

// === 유틸리티 함수 구현 ===

int32 UHSCommunicationSystem::GetPlayerIDByName(const FString& PlayerName) const
{
    // 캐시 사용
    if (const int32* CachedID = PlayerNameToIDCache.Find(PlayerName))
    {
        FDateTime CurrentTime = FDateTime::Now();
        if ((CurrentTime - LastPlayerCacheUpdate).GetTotalSeconds() < 10.0f)
        {
            return *CachedID;
        }
    }
    
    // 캐시 미스 또는 만료된 경우 데이터 재조회
    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS->GetPlayerName() == PlayerName)
                {
                    int32 PlayerID = PS->GetPlayerId();
                    
                    // 캐시 업데이트 (mutable 변수 직접 수정)
                    PlayerNameToIDCache.Add(PlayerName, PlayerID);
                    PlayerIDToNameCache.Add(PlayerID, PlayerName);
                    LastPlayerCacheUpdate = FDateTime::Now();
                    
                    return PlayerID;
                }
            }
        }
    }
    
    return -1;
}

FString UHSCommunicationSystem::GetPlayerNameByID(int32 PlayerID) const
{
    // 캐시 사용
    if (const FString* CachedName = PlayerIDToNameCache.Find(PlayerID))
    {
        FDateTime CurrentTime = FDateTime::Now();
        if ((CurrentTime - LastPlayerCacheUpdate).GetTotalSeconds() < 10.0f)
        {
            return *CachedName;
        }
    }
    
    // 캐시 미스 또는 만료된 경우 데이터 재조회
    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS->GetPlayerId() == PlayerID)
                {
                    FString PlayerName = PS->GetPlayerName();
                    
                    // 캐시 업데이트 (mutable 변수 직접 수정)
                    PlayerIDToNameCache.Add(PlayerID, PlayerName);
                    PlayerNameToIDCache.Add(PlayerName, PlayerID);
                    LastPlayerCacheUpdate = FDateTime::Now();
                    
                    return PlayerName;
                }
            }
        }
    }
    
    return TEXT("Unknown");
}

FString UHSCommunicationSystem::FilterProfanity(const FString& Input) const
{
    if (!bProfanityFilterEnabled)
    {
        return Input;
    }
    
    // 간단한 욕설 필터링 구현
    // 추가 필터링 로직 확장 가능
    TArray<FString> ProfanityWords = {
        TEXT("욕설1"), TEXT("욕설2"), TEXT("욕설3")
        // 욕설 목록을 프로젝트 요구사항에 맞게 확장
    };
    
    FString FilteredText = Input;
    
    for (const FString& BadWord : ProfanityWords)
    {
        FString Replacement;
        for (int32 i = 0; i < BadWord.Len(); i++)
        {
            Replacement += TEXT("*");
        }
        
        FilteredText = FilteredText.Replace(*BadWord, *Replacement, ESearchCase::IgnoreCase);
    }
    
    return FilteredText;
}

bool UHSCommunicationSystem::IsSpamFiltered(int32 PlayerID) const
{
    if (!bSpamFilterEnabled)
    {
        return false;
    }
    
    // 최근 메시지 시간 확인
    const FDateTime* LastTime = LastMessageTime.Find(PlayerID);
    if (!LastTime)
    {
        return false;
    }
    
    FDateTime CurrentTime = FDateTime::Now();
    float TimeDiff = (CurrentTime - *LastTime).GetTotalSeconds();
    
    // 시간 제한 내의 메시지 수 확인
    if (TimeDiff < SpamTimeLimit)
    {
        const int32* MsgCount = MessageCount.Find(PlayerID);
        if (MsgCount && *MsgCount >= SpamMessageLimit)
        {
            return true;
        }
    }
    else
    {
        // 시간이 지났으면 카운터 리셋
        MessageCount.Add(PlayerID, 0);
    }
    
    return false;
}

// === 내부 함수 구현 ===

bool UHSCommunicationSystem::ValidateChatMessage(const FString& Message, int32 SenderID) const
{
    // 메시지 길이 검증
    if (Message.IsEmpty() || Message.Len() > 500)
    {
        return false;
    }
    
    // 스팸 필터링
    if (IsSpamFiltered(SenderID))
    {
        return false;
    }
    
    // 추가 검증 로직
    // 예: 금지된 문자 확인, 플레이어 상태 확인 등
    
    return true;
}

void UHSCommunicationSystem::CheckExpiredPings()
{
    FDateTime CurrentTime = FDateTime::Now();
    TArray<int32> ExpiredPings;
    
    for (const auto& PingPair : ActivePings)
    {
        const FHSPingData& PingData = PingPair.Value;
        float ElapsedTime = (CurrentTime - PingData.CreationTime).GetTotalSeconds();
        
        if (ElapsedTime >= PingData.Duration)
        {
            ExpiredPings.Add(PingPair.Key);
        }
    }
    
    // 만료된 핑 제거
    for (int32 PingID : ExpiredPings)
    {
        RemovePing(PingID);
    }
}

void UHSCommunicationSystem::InitializeVoiceChat()
{
    // UE5 음성 채팅 서브시스템 초기화
    // VoiceChat 모듈 연동 지점
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 음성 채팅 시스템 초기화됨"));
}

void UHSCommunicationSystem::CleanupVoiceChat()
{
    // 음성 채팅 정리
    VoiceChatPlayers.Empty();
    bVoiceChatEnabled = false;
    bLocalPlayerMuted = false;
    bLocalPlayerDeafened = false;
    
    UE_LOG(LogTemp, Log, TEXT("HSCommunicationSystem: 음성 채팅 시스템 정리됨"));
}

void UHSCommunicationSystem::UpdatePlayerVoiceState(int32 PlayerID, EHSVoiceChatState NewState)
{
    if (FHSVoiceChatPlayerInfo* VoiceInfo = VoiceChatPlayers.Find(PlayerID))
    {
        VoiceInfo->VoiceState = NewState;
        
        // 델리게이트 호출
        OnVoiceChatStateChanged.Broadcast(PlayerID, NewState);
    }
}

void UHSCommunicationSystem::ProcessPendingMessages()
{
    // 배치 처리로 네트워크 부하 감소
    
    // 대기 중인 채팅 메시지 처리
    if (PendingChatMessages.Num() > 0)
    {
        for (const FHSChatMessage& Message : PendingChatMessages)
        {
            ReceiveChatMessage(Message);
        }
        PendingChatMessages.Empty();
    }
    
    // 대기 중인 핑 처리
    if (PendingPings.Num() > 0)
    {
        for (const FHSPingData& Ping : PendingPings)
        {
            // 핑 네트워크 전송 로직
            // 네트워크 복제 처리 지점
        }
        PendingPings.Empty();
    }
}
