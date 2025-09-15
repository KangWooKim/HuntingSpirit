// 사냥의 영혼(HuntingSpirit) 게임의 협동 메커니즘 시스템 구현
// 플레이어들 간의 다양한 협동 액션과 메커니즘을 관리하는 클래스 구현

#include "HSCoopMechanics.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Core/PlayerController/HSPlayerController.h"
#include "HuntingSpirit/Characters/Stats/HSStatsComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"

// 생성자
UHSCoopMechanics::UHSCoopMechanics()
{
    // 기본값 설정
    TeamManager = nullptr;
    SharedAbilitySystem = nullptr;
    CoopActionPoolSize = 50; // 메모리 풀링을 위한 기본 풀 크기
    SpatialHashCellSize = 500.0f; // 공간 해시 셀 크기 (500 유닛)
    CacheInvalidationTimer = 0.0f;
    bNetworkingEnabled = true;
    bIsInitialized = false;

    // 메모리 풀 초기화
    CoopActionPool.Reserve(CoopActionPoolSize);
    for (int32 i = 0; i < CoopActionPoolSize; ++i)
    {
        CoopActionPool.Add(FActiveCoopAction());
    }
}

// 초기화
void UHSCoopMechanics::Initialize(UHSTeamManager* InTeamManager, UHSSharedAbilitySystem* InSharedAbilitySystem)
{
    if (!InTeamManager || !InSharedAbilitySystem)
    {
        UE_LOG(LogTemp, Error, TEXT("HSCoopMechanics: 초기화 실패 - TeamManager 또는 SharedAbilitySystem이 null입니다."));
        return;
    }

    TeamManager = InTeamManager;
    SharedAbilitySystem = InSharedAbilitySystem;
    bIsInitialized = true;

    // 기본 협동 액션들 등록
    RegisterDefaultCoopActions();

    // 기본 콤보 체인 등록
    RegisterDefaultCombos();

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 성공적으로 초기화되었습니다."));
}

// 정리
void UHSCoopMechanics::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    // 모든 활성 액션 정리
    for (auto& ActionPair : ActiveCoopActions)
    {
        CancelCoopAction(ActionPair.Key);
    }

    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        
        for (auto& TimerPair : SyncTimerHandles)
        {
            TimerManager.ClearTimer(TimerPair.Value);
        }
        
        for (auto& TimerPair : ExecutionTimerHandles)
        {
            TimerManager.ClearTimer(TimerPair.Value);
        }
        
        for (auto& TimerPair : CooldownTimerHandles)
        {
            TimerManager.ClearTimer(TimerPair.Value);
        }
        
        for (auto& TimerPair : RevivalTimerHandles)
        {
            TimerManager.ClearTimer(TimerPair.Value);
        }
    }

    // 데이터 정리
    RegisteredCoopActions.Empty();
    ActiveCoopActions.Empty();
    ActionCooldowns.Empty();
    RegisteredCombos.Empty();
    RevivalPairs.Empty();
    RevivalProgress.Empty();
    TeamResourcePools.Empty();
    TeamFormations.Empty();
    FormationLeaders.Empty();

    TeamManager = nullptr;
    SharedAbilitySystem = nullptr;
    bIsInitialized = false;

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 정리 완료"));
}

// 협동 액션 등록
void UHSCoopMechanics::RegisterCoopAction(const FCoopActionData& ActionData)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 초기화되지 않았습니다."));
        return;
    }

    if (ActionData.ActionID.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 유효하지 않은 ActionID입니다."));
        return;
    }

    RegisteredCoopActions.Add(ActionData.ActionID, ActionData);
    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 등록 완료"), *ActionData.ActionID.ToString());
}

// 협동 액션 등록 해제
void UHSCoopMechanics::UnregisterCoopAction(const FName& ActionID)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 활성 상태라면 취소
    if (IsCoopActionActive(ActionID))
    {
        CancelCoopAction(ActionID);
    }

    RegisteredCoopActions.Remove(ActionID);
    ActionCooldowns.Remove(ActionID);
    
    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 등록 해제 완료"), *ActionID.ToString());
}

// 협동 액션 시작
bool UHSCoopMechanics::InitiateCoopAction(const FName& ActionID, AHSCharacterBase* Initiator, const TArray<AHSCharacterBase*>& Participants)
{
    if (!bIsInitialized || !Initiator)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 초기화되지 않았거나 Initiator가 null입니다."));
        return false;
    }

    // 등록된 액션인지 확인
    const FCoopActionData* ActionData = RegisteredCoopActions.Find(ActionID);
    if (!ActionData)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 등록되지 않은 액션 ID '%s'"), *ActionID.ToString());
        return false;
    }

    // 이미 활성 상태인지 확인
    if (IsCoopActionActive(ActionID))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 액션 '%s'가 이미 활성 상태입니다."), *ActionID.ToString());
        return false;
    }

    // 조건 확인
    FString FailureReason;
    if (!CanInitiateCoopAction(ActionID, Initiator, Participants, FailureReason))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 액션 '%s' 시작 실패: %s"), *ActionID.ToString(), *FailureReason);
        OnCoopActionFailed.Broadcast(ActionID, FailureReason);
        return false;
    }

    // 새로운 활성 액션 생성
    FActiveCoopAction* NewAction = GetPooledCoopAction();
    if (!NewAction)
    {
        UE_LOG(LogTemp, Error, TEXT("HSCoopMechanics: 메모리 풀에서 액션을 가져올 수 없습니다."));
        return false;
    }

    // 액션 데이터 설정
    NewAction->ActionID = ActionID;
    NewAction->Participants = Participants;
    NewAction->CurrentState = ECoopActionState::CAS_Preparing;
    NewAction->RemainingTime = ActionData->SyncTimeWindow;
    NewAction->StartTime = GetWorld()->GetTimeSeconds();
    NewAction->Initiator = Initiator;
    NewAction->bSuccess = false;
    NewAction->Progress = 0.0f;

    // 활성 액션 목록에 추가
    ActiveCoopActions.Add(ActionID, *NewAction);

    // 동기화 타이머 설정
    if (UWorld* World = GetWorld())
    {
        FTimerHandle& SyncTimer = SyncTimerHandles.FindOrAdd(ActionID);
        World->GetTimerManager().SetTimer(
            SyncTimer,
            FTimerDelegate::CreateUFunction(this, FName("OnSyncTimeExpired"), ActionID),
            ActionData->SyncTimeWindow,
            false
        );
    }

    // 이벤트 브로드캐스트
    OnCoopActionStarted.Broadcast(ActionID, Participants);

    // 비주얼/오디오 효과 재생
    if (ActionData->ActivationEffect)
    {
        FVector EffectLocation = CalculateGroupCenterLocation(Participants);
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ActionData->ActivationEffect, EffectLocation);
    }

    if (ActionData->ActivationSound)
    {
        FVector SoundLocation = CalculateGroupCenterLocation(Participants);
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), ActionData->ActivationSound, SoundLocation);
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 시작됨 (참여자: %d명)"), *ActionID.ToString(), Participants.Num());
    return true;
}

// 협동 액션 참여
bool UHSCoopMechanics::JoinCoopAction(const FName& ActionID, AHSCharacterBase* Player)
{
    if (!bIsInitialized || !Player)
    {
        return false;
    }

    FActiveCoopAction* ActiveAction = ActiveCoopActions.Find(ActionID);
    if (!ActiveAction || ActiveAction->CurrentState != ECoopActionState::CAS_Preparing)
    {
        return false;
    }

    const FCoopActionData* ActionData = RegisteredCoopActions.Find(ActionID);
    if (!ActionData)
    {
        return false;
    }

    // 이미 참여 중인지 확인
    if (ActiveAction->Participants.Contains(Player))
    {
        return false;
    }

    // 최대 인원 확인
    if (ActiveAction->Participants.Num() >= ActionData->MaximumPlayers)
    {
        return false;
    }

    // 거리 확인
    if (!CheckPlayerProximity(TArray<AHSCharacterBase*>{Player}, ActionData->MaximumRange))
    {
        return false;
    }

    // 참여자 추가
    ActiveAction->Participants.Add(Player);
    
    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 플레이어가 액션 '%s'에 참여했습니다."), *ActionID.ToString());
    return true;
}

// 협동 액션 취소
void UHSCoopMechanics::CancelCoopAction(const FName& ActionID)
{
    if (!bIsInitialized)
    {
        return;
    }

    FActiveCoopAction* ActiveAction = ActiveCoopActions.Find(ActionID);
    if (!ActiveAction)
    {
        return;
    }

    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        
        if (FTimerHandle* SyncTimer = SyncTimerHandles.Find(ActionID))
        {
            TimerManager.ClearTimer(*SyncTimer);
            SyncTimerHandles.Remove(ActionID);
        }
        
        if (FTimerHandle* ExecutionTimer = ExecutionTimerHandles.Find(ActionID))
        {
            TimerManager.ClearTimer(*ExecutionTimer);
            ExecutionTimerHandles.Remove(ActionID);
        }
    }

    // 실패 처리
    const FCoopActionData* ActionData = RegisteredCoopActions.Find(ActionID);
    if (ActionData)
    {
        ApplyFailurePenalties(*ActionData, ActiveAction->Participants);
    }

    // 이벤트 브로드캐스트
    OnCoopActionFailed.Broadcast(ActionID, TEXT("Action Cancelled"));

    // 메모리 풀로 반환
    ReturnCoopActionToPool(ActiveAction);
    ActiveCoopActions.Remove(ActionID);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 취소됨"), *ActionID.ToString());
}

// 협동 액션 실행 가능 여부 확인
bool UHSCoopMechanics::CanInitiateCoopAction(const FName& ActionID, AHSCharacterBase* Initiator, const TArray<AHSCharacterBase*>& Participants, FString& OutFailureReason)
{
    if (!bIsInitialized || !Initiator)
    {
        OutFailureReason = TEXT("System not initialized or invalid initiator");
        return false;
    }

    const FCoopActionData* ActionData = RegisteredCoopActions.Find(ActionID);
    if (!ActionData)
    {
        OutFailureReason = TEXT("Action not registered");
        return false;
    }

    // 쿨다운 확인
    if (!CheckCooldownReady(ActionID))
    {
        OutFailureReason = TEXT("Action on cooldown");
        return false;
    }

    // 최소 인원 확인
    if (Participants.Num() < ActionData->MinimumPlayers)
    {
        OutFailureReason = FString::Printf(TEXT("Not enough players (need %d, have %d)"), ActionData->MinimumPlayers, Participants.Num());
        return false;
    }

    // 최대 인원 확인
    if (Participants.Num() > ActionData->MaximumPlayers)
    {
        OutFailureReason = FString::Printf(TEXT("Too many players (max %d, have %d)"), ActionData->MaximumPlayers, Participants.Num());
        return false;
    }

    // 플레이어 생존 확인
    if (!CheckPlayersAlive(Participants))
    {
        OutFailureReason = TEXT("Some players are dead");
        return false;
    }

    // 거리 확인
    if (!CheckPlayerProximity(Participants, ActionData->MaximumRange))
    {
        OutFailureReason = TEXT("Players too far apart");
        return false;
    }

    // 클래스 조합 확인
    if (!CheckClassCombination(ActionData->RequiredClassCombination, Participants))
    {
        OutFailureReason = TEXT("Required class combination not met");
        return false;
    }

    // 같은 팀인지 확인
    if (TeamManager)
    {
        int32 InitiatorTeamID = TeamManager->GetPlayerTeamID(Initiator->GetPlayerState());
        for (AHSCharacterBase* Participant : Participants)
        {
            if (!Participant || !Participant->GetPlayerState())
            {
                OutFailureReason = TEXT("Invalid participant");
                return false;
            }

            int32 ParticipantTeamID = TeamManager->GetPlayerTeamID(Participant->GetPlayerState());
            if (InitiatorTeamID != ParticipantTeamID)
            {
                OutFailureReason = TEXT("All participants must be in the same team");
                return false;
            }
        }
    }

    return true;
}

// 콤보 체인 등록
void UHSCoopMechanics::RegisterComboChain(const FComboChainData& ComboData)
{
    if (!bIsInitialized)
    {
        return;
    }

    if (ComboData.ComboID.IsNone() || ComboData.ChainSequence.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 유효하지 않은 콤보 데이터"));
        return;
    }

    RegisteredCombos.Add(ComboData.ComboID, ComboData);
    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 콤보 체인 '%s' 등록 완료 (%d단계)"), *ComboData.ComboID.ToString(), ComboData.ChainSequence.Num());
}

// 콤보 단계 실행
bool UHSCoopMechanics::TriggerComboStep(const FName& ComboID, const FName& ActionID, AHSCharacterBase* Player)
{
    if (!bIsInitialized || !Player)
    {
        return false;
    }

    FComboChainData* ComboData = RegisteredCombos.Find(ComboID);
    if (!ComboData)
    {
        return false;
    }

    // 현재 단계가 유효한지 확인
    if (ComboData->CurrentStep >= ComboData->ChainSequence.Num())
    {
        return false;
    }

    // 올바른 액션인지 확인
    if (ComboData->ChainSequence[ComboData->CurrentStep] != ActionID)
    {
        // 잘못된 액션이면 콤보 리셋
        ResetComboChain(ComboID);
        return false;
    }

    // 타이밍 윈도우 확인 (첫 번째 단계가 아닌 경우)
    if (ComboData->CurrentStep > 0)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float TimeSinceLastAction = CurrentTime - ComboData->LastActionTime;
        float MaxTimingWindow = ComboData->TimingWindows.IsValidIndex(ComboData->CurrentStep) ? 
                                ComboData->TimingWindows[ComboData->CurrentStep] : 2.0f;

        if (TimeSinceLastAction > MaxTimingWindow)
        {
            // 타이밍 실패로 콤보 리셋
            ResetComboChain(ComboID);
            return false;
        }
    }

    // 단계 진행
    ComboData->CurrentStep++;
    ComboData->LastActionTime = GetWorld()->GetTimeSeconds();

    // 이벤트 브로드캐스트
    OnComboChainProgress.Broadcast(ComboID, ComboData->CurrentStep, ComboData->ChainSequence.Num());

    // 콤보 완성 확인
    if (ComboData->CurrentStep >= ComboData->ChainSequence.Num())
    {
        // 콤보 완성!
        OnComboChainCompleted.Broadcast(ComboID, ComboData->CompletionBonus);
        
        // 완성 보너스 적용
        if (UHSStatsComponent* StatsComp = Player->FindComponentByClass<UHSStatsComponent>())
        {
            // 임시 데미지 버프 적용
            FBuffData ComboBuffData;
            ComboBuffData.BuffID = FString::Printf(TEXT("ComboBonus_%s"), *ComboID.ToString());
            ComboBuffData.BuffType = EBuffType::Attack;
            ComboBuffData.Value = ComboData->CompletionBonus - 1.0f; // 1.5배 보너스는 0.5f 추가
            ComboBuffData.Duration = 10.0f;
            ComboBuffData.bIsPercentage = true;
            StatsComp->ApplyBuff(ComboBuffData);
        }

        // 콤보 리셋
        ResetComboChain(ComboID);
        
        UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 콤보 체인 '%s' 완성! 보너스: %.2f"), *ComboID.ToString(), ComboData->CompletionBonus);
    }

    return true;
}

// 콤보 체인 리셋
void UHSCoopMechanics::ResetComboChain(const FName& ComboID)
{
    if (!bIsInitialized)
    {
        return;
    }

    FComboChainData* ComboData = RegisteredCombos.Find(ComboID);
    if (ComboData)
    {
        ComboData->CurrentStep = 0;
        ComboData->LastActionTime = 0.0f;
    }
}

// 부활 요청
bool UHSCoopMechanics::RequestRevival(AHSCharacterBase* DeadPlayer, AHSCharacterBase* Reviver)
{
    if (!bIsInitialized || !DeadPlayer || !Reviver)
    {
        return false;
    }

    // 이미 부활 중인지 확인
    if (IsRevivalInProgress(DeadPlayer))
    {
        return false;
    }

    // 같은 팀인지 확인
    if (TeamManager)
    {
        APlayerState* DeadPlayerState = DeadPlayer->GetPlayerState();
        APlayerState* ReviverPlayerState = Reviver->GetPlayerState();
        
        if (!DeadPlayerState || !ReviverPlayerState)
        {
            return false;
        }

        if (!TeamManager->ArePlayersInSameTeam(DeadPlayerState, ReviverPlayerState))
        {
            return false;
        }
    }

    // 거리 확인
    float Distance = FVector::Dist(DeadPlayer->GetActorLocation(), Reviver->GetActorLocation());
    if (Distance > 300.0f) // 3미터 이내
    {
        return false;
    }

    // 부활 프로세스 시작
    RevivalPairs.Add(DeadPlayer, Reviver);
    RevivalProgress.Add(DeadPlayer, 0.0f);

    // 부활 타이머 설정 (5초)
    if (UWorld* World = GetWorld())
    {
        FTimerHandle& RevivalTimer = RevivalTimerHandles.FindOrAdd(DeadPlayer);
        World->GetTimerManager().SetTimer(
            RevivalTimer,
            FTimerDelegate::CreateUFunction(this, FName("HandleRevivalCompleted"), DeadPlayer),
            5.0f,
            false
        );
    }

    // 이벤트 브로드캐스트
    OnRevivalRequested.Broadcast(DeadPlayer, Reviver);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 부활 요청 시작 - 대상: %s, 부활자: %s"), 
           *DeadPlayer->GetName(), *Reviver->GetName());
    return true;
}

// 부활 취소
void UHSCoopMechanics::CancelRevival(AHSCharacterBase* DeadPlayer)
{
    if (!bIsInitialized || !DeadPlayer)
    {
        return;
    }

    if (!IsRevivalInProgress(DeadPlayer))
    {
        return;
    }

    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        if (FTimerHandle* Timer = RevivalTimerHandles.Find(DeadPlayer))
        {
            World->GetTimerManager().ClearTimer(*Timer);
            RevivalTimerHandles.Remove(DeadPlayer);
        }
    }

    // 데이터 정리
    RevivalPairs.Remove(DeadPlayer);
    RevivalProgress.Remove(DeadPlayer);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 부활 취소 - 대상: %s"), *DeadPlayer->GetName());
}

// 부활 진행 중인지 확인
bool UHSCoopMechanics::IsRevivalInProgress(AHSCharacterBase* DeadPlayer) const
{
    return DeadPlayer && RevivalPairs.Contains(DeadPlayer);
}

// 자원 공유
bool UHSCoopMechanics::ShareResource(AHSCharacterBase* Giver, AHSCharacterBase* Receiver, const FName& ResourceType, float Amount)
{
    if (!bIsInitialized || !Giver || !Receiver || Amount <= 0.0f)
    {
        return false;
    }

    // 같은 팀인지 확인
    if (TeamManager)
    {
        APlayerState* GiverState = Giver->GetPlayerState();
        APlayerState* ReceiverState = Receiver->GetPlayerState();
        
        if (!GiverState || !ReceiverState || !TeamManager->ArePlayersInSameTeam(GiverState, ReceiverState))
        {
            return false;
        }
    }

    // 거리 확인
    float Distance = FVector::Dist(Giver->GetActorLocation(), Receiver->GetActorLocation());
    if (Distance > 500.0f) // 5미터 이내
    {
        return false;
    }

    // 자원 이전 로직 (실제 구현은 인벤토리 시스템에 따라 달라짐)
    // 여기서는 예시로 스태미너 공유를 구현
    if (ResourceType == TEXT("Stamina"))
    {
        // 임시 구현 - 실제로는 스태미너 컴포넌트를 통해 처리해야 함
        UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 스태미너 %.1f 공유 - %s -> %s"), 
               Amount, *Giver->GetName(), *Receiver->GetName());
        return true;
    }

    return false;
}

// 팀 자원 풀 활성화
void UHSCoopMechanics::EnableResourcePool(int32 TeamID, const FName& ResourceType)
{
    if (!bIsInitialized || TeamID < 0)
    {
        return;
    }

    FTeamResourcePool& TeamResourcePool = TeamResourcePools.FindOrAdd(TeamID);
    TeamResourcePool.Resources.FindOrAdd(ResourceType) = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 팀 %d의 자원 풀 '%s' 활성화"), TeamID, *ResourceType.ToString());
}

// 팀 자원 풀 비활성화
void UHSCoopMechanics::DisableResourcePool(int32 TeamID, const FName& ResourceType)
{
    if (!bIsInitialized)
    {
        return;
    }

    if (FTeamResourcePool* TeamResourcePool = TeamResourcePools.Find(TeamID))
    {
        TeamResourcePool->Resources.Remove(ResourceType);
        
        // 팀 자원이 모두 비어있으면 팀 엔트리 제거
        if (TeamResourcePool->Resources.Num() == 0)
        {
            TeamResourcePools.Remove(TeamID);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 팀 %d의 자원 풀 '%s' 비활성화"), TeamID, *ResourceType.ToString());
}

// 팀 대형 설정
bool UHSCoopMechanics::SetTeamFormation(int32 TeamID, const FName& FormationType, AHSCharacterBase* Leader)
{
    if (!bIsInitialized || !Leader || TeamID < 0)
    {
        return false;
    }

    // 팀 유효성 확인
    if (TeamManager)
    {
        FHSTeamInfo TeamInfo = TeamManager->GetTeamInfo(TeamID);
        if (TeamInfo.TeamID == -1)
        {
            return false;
        }

        // 리더가 해당 팀에 속하는지 확인
        if (!TeamInfo.IsPlayerInTeam(Leader->GetPlayerState()))
        {
            return false;
        }
    }

    // 대형 설정
    TeamFormations.Add(TeamID, FormationType);
    FormationLeaders.Add(TeamID, Leader);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 팀 %d 대형 설정 - %s (리더: %s)"), 
           TeamID, *FormationType.ToString(), *Leader->GetName());
    return true;
}

// 대형 이동 업데이트
void UHSCoopMechanics::UpdateFormationMovement(int32 TeamID, const FVector& TargetLocation)
{
    if (!bIsInitialized || !TeamManager)
    {
        return;
    }

    // 대형이 설정되어 있는지 확인
    if (!TeamFormations.Contains(TeamID))
    {
        return;
    }

    // 팀원들 가져오기
    FHSTeamInfo TeamInfo = TeamManager->GetTeamInfo(TeamID);
    TArray<APlayerState*> TeamMembers = TeamManager->GetTeamMembers(TeamID);

    // 각 팀원에게 대형 위치 계산 및 이동 명령 (간소화된 구현)
    for (int32 i = 0; i < TeamMembers.Num(); ++i)
    {
        if (APlayerState* MemberState = TeamMembers[i])
        {
            if (APawn* MemberPawn = MemberState->GetPawn())
            {
                // 간단한 원형 대형 계산
                float Angle = (2.0f * PI * i) / TeamMembers.Num();
                FVector Offset = FVector(FMath::Cos(Angle) * 200.0f, FMath::Sin(Angle) * 200.0f, 0.0f);
                FVector FormationPosition = TargetLocation + Offset;

                // AI 이동 명령 (실제 구현에서는 AI 컨트롤러를 통해 처리)
                if (AHSCharacterBase* Character = Cast<AHSCharacterBase>(MemberPawn))
                {
                    // 이동 로직 구현 필요
                }
            }
        }
    }
}

// 대형 해제
void UHSCoopMechanics::BreakFormation(int32 TeamID)
{
    if (!bIsInitialized)
    {
        return;
    }

    TeamFormations.Remove(TeamID);
    FormationLeaders.Remove(TeamID);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 팀 %d 대형 해제"), TeamID);
}

// 활성 협동 액션 가져오기
FActiveCoopAction UHSCoopMechanics::GetActiveCoopAction(const FName& ActionID) const
{
    if (const FActiveCoopAction* Action = ActiveCoopActions.Find(ActionID))
    {
        return *Action;
    }
    return FActiveCoopAction();
}

// 모든 활성 협동 액션 가져오기
TArray<FActiveCoopAction> UHSCoopMechanics::GetAllActiveCoopActions() const
{
    TArray<FActiveCoopAction> Result;
    for (const auto& ActionPair : ActiveCoopActions)
    {
        Result.Add(ActionPair.Value);
    }
    return Result;
}

// 협동 액션 활성 상태 확인
bool UHSCoopMechanics::IsCoopActionActive(const FName& ActionID) const
{
    return ActiveCoopActions.Contains(ActionID);
}

// 협동 액션 쿨다운 가져오기
float UHSCoopMechanics::GetCoopActionCooldown(const FName& ActionID) const
{
    if (const float* Cooldown = ActionCooldowns.Find(ActionID))
    {
        return FMath::Max(0.0f, *Cooldown);
    }
    return 0.0f;
}

// 콤보 체인 데이터 가져오기
FComboChainData UHSCoopMechanics::GetComboChainData(const FName& ComboID) const
{
    if (const FComboChainData* ComboData = RegisteredCombos.Find(ComboID))
    {
        return *ComboData;
    }
    return FComboChainData();
}

// 콤보 체인 진행률 가져오기
int32 UHSCoopMechanics::GetComboChainProgress(const FName& ComboID) const
{
    if (const FComboChainData* ComboData = RegisteredCombos.Find(ComboID))
    {
        return ComboData->CurrentStep;
    }
    return 0;
}

// 틱 업데이트
void UHSCoopMechanics::TickCoopMechanics(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 쿨다운 업데이트
    for (auto& CooldownPair : ActionCooldowns)
    {
        CooldownPair.Value = FMath::Max(0.0f, CooldownPair.Value - DeltaTime);
    }

    // 활성 액션 업데이트
    TArray<FName> ActionsToRemove;
    for (auto& ActionPair : ActiveCoopActions)
    {
        FActiveCoopAction& Action = ActionPair.Value;
        
        // 시간 업데이트
        Action.RemainingTime = FMath::Max(0.0f, Action.RemainingTime - DeltaTime);
        
        // 상태에 따른 처리
        switch (Action.CurrentState)
        {
            case ECoopActionState::CAS_Preparing:
                // 준비 중 - 동기화 대기
                break;
                
            case ECoopActionState::CAS_Executing:
                // 실행 중 - 진행률 업데이트
                if (const FCoopActionData* ActionData = RegisteredCoopActions.Find(Action.ActionID))
                {
                    float ElapsedTime = GetWorld()->GetTimeSeconds() - Action.StartTime;
                    Action.Progress = FMath::Clamp(ElapsedTime / ActionData->ExecutionDuration, 0.0f, 1.0f);
                }
                break;
                
            case ECoopActionState::CAS_Completed:
            case ECoopActionState::CAS_Failed:
                // 완료/실패 상태는 곧 제거됨
                ActionsToRemove.Add(ActionPair.Key);
                break;
                
            default:
                break;
        }
    }

    // 완료된 액션들 정리
    for (const FName& ActionID : ActionsToRemove)
    {
        if (FActiveCoopAction* Action = ActiveCoopActions.Find(ActionID))
        {
            ReturnCoopActionToPool(Action);
        }
        ActiveCoopActions.Remove(ActionID);
    }

    // 부활 진행률 업데이트
    for (auto& RevivalPair : RevivalProgress)
    {
        RevivalPair.Value = FMath::Min(1.0f, RevivalPair.Value + DeltaTime / 5.0f); // 5초 동안 부활
    }

    // 캐시 무효화 타이머 업데이트
    CacheInvalidationTimer += DeltaTime;
    if (CacheInvalidationTimer >= 1.0f) // 1초마다 캐시 무효화
    {
        InvalidateCache();
        CacheInvalidationTimer = 0.0f;
    }
}

// === 보호된 함수들 ===

// 동시 공격 처리
void UHSCoopMechanics::ProcessSimultaneousAttack(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 모든 참여자가 동시에 공격을 수행
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (UHSCombatComponent* CombatComp = Participant->FindComponentByClass<UHSCombatComponent>())
        {
            // 다음 공격에 데미지 배율 적용
            CombatComp->SetNextAttackDamageMultiplier(ActionData.SuccessRewardMultiplier);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 동시 공격 처리 완료 - 참여자 %d명"), ActiveAction.Participants.Num());
}

// 거리 확인
bool UHSCoopMechanics::CheckPlayerProximity(const TArray<AHSCharacterBase*>& Players, float MaxRange) const
{
    if (Players.Num() < 2)
    {
        return true; // 한 명이면 거리 제한 없음
    }

    // 캐시 확인
    uint32 CacheKey = GetProximityCheckHash(Players, MaxRange);
    if (const bool* CachedResult = ProximityCheckCache.Find(CacheKey))
    {
        return *CachedResult;
    }

    // 모든 플레이어 간 거리 확인
    bool bResult = true;
    for (int32 i = 0; i < Players.Num() && bResult; ++i)
    {
        for (int32 j = i + 1; j < Players.Num(); ++j)
        {
            if (!Players[i] || !Players[j])
            {
                bResult = false;
                break;
            }

            float Distance = FVector::Dist(Players[i]->GetActorLocation(), Players[j]->GetActorLocation());
            if (Distance > MaxRange)
            {
                bResult = false;
                break;
            }
        }
    }

    // 캐시에 저장
    ProximityCheckCache.Add(CacheKey, bResult);
    return bResult;
}

// 클래스 조합 확인
bool UHSCoopMechanics::CheckClassCombination(const TArray<FName>& RequiredClasses, const TArray<AHSCharacterBase*>& Players) const
{
    if (RequiredClasses.Num() == 0)
    {
        return true; // 특별한 클래스 조합 요구사항 없음
    }

    // 플레이어들의 클래스 수집
    TArray<FName> PlayerClasses;
    for (AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            FName PlayerClass = Player->GetClass()->GetFName();
            PlayerClasses.Add(PlayerClass);
        }
    }

    // 모든 필요한 클래스가 있는지 확인
    for (const FName& RequiredClass : RequiredClasses)
    {
        if (!PlayerClasses.Contains(RequiredClass))
        {
            return false;
        }
    }

    return true;
}

// 쿨다운 확인
bool UHSCoopMechanics::CheckCooldownReady(const FName& ActionID) const
{
    if (const float* Cooldown = ActionCooldowns.Find(ActionID))
    {
        return *Cooldown <= 0.0f;
    }
    return true; // 쿨다운 정보가 없으면 사용 가능
}

// 플레이어 생존 확인
bool UHSCoopMechanics::CheckPlayersAlive(const TArray<AHSCharacterBase*>& Players) const
{
    for (AHSCharacterBase* Player : Players)
    {
        if (!Player)
        {
            return false;
        }

        if (UHSCombatComponent* CombatComp = Player->FindComponentByClass<UHSCombatComponent>())
        {
            if (CombatComp->IsDead())
            {
                return false;
            }
        }
    }
    return true;
}

// 성공 보상 적용
void UHSCoopMechanics::ApplySuccessRewards(const FCoopActionData& ActionData, const TArray<AHSCharacterBase*>& Participants)
{
    for (AHSCharacterBase* Participant : Participants)
    {
        if (!Participant)
        {
            continue;
        }

        // 경험치 보너스 적용 (예시)
        if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
        {
            float BonusXP = 100.0f * ActionData.SuccessRewardMultiplier;
            // StatsComp->AddExperience(BonusXP); // 실제 구현에서는 경험치 시스템에 맞게 수정
        }

        UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: %s에게 성공 보상 적용 (배율: %.2f)"), 
               *Participant->GetName(), ActionData.SuccessRewardMultiplier);
    }
}

// 실패 패널티 적용
void UHSCoopMechanics::ApplyFailurePenalties(const FCoopActionData& ActionData, const TArray<AHSCharacterBase*>& Participants)
{
    // 현재는 로그만 출력, 필요에 따라 패널티 구현
    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 실패 - 참여자 %d명"), 
           *ActionData.ActionID.ToString(), Participants.Num());
}

// === 프라이빗 함수들 ===

// 메모리 풀에서 액션 가져오기
FActiveCoopAction* UHSCoopMechanics::GetPooledCoopAction()
{
    for (FActiveCoopAction& Action : CoopActionPool)
    {
        if (Action.CurrentState == ECoopActionState::CAS_Inactive)
        {
            // 기본값으로 초기화
            Action = FActiveCoopAction();
            return &Action;
        }
    }

    // 풀이 가득 찬 경우 새로운 액션 추가
    CoopActionPool.Add(FActiveCoopAction());
    return &CoopActionPool.Last();
}

// 메모리 풀로 액션 반환
void UHSCoopMechanics::ReturnCoopActionToPool(FActiveCoopAction* Action)
{
    if (Action)
    {
        *Action = FActiveCoopAction(); // 초기화
        Action->CurrentState = ECoopActionState::CAS_Inactive;
    }
}

// 근접도 확인 해시 생성
uint32 UHSCoopMechanics::GetProximityCheckHash(const TArray<AHSCharacterBase*>& Players, float Range) const
{
    uint32 Hash = 0;
    for (AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            Hash = HashCombine(Hash, GetTypeHash(Player));
        }
    }
    Hash = HashCombine(Hash, GetTypeHash(Range));
    return Hash;
}

// 캐시 무효화
void UHSCoopMechanics::InvalidateCache()
{
    ProximityCheckCache.Empty();
}

// 그룹 중심 위치 계산
FVector UHSCoopMechanics::CalculateGroupCenterLocation(const TArray<AHSCharacterBase*>& Players) const
{
    if (Players.Num() == 0)
    {
        return FVector::ZeroVector;
    }

    FVector Center = FVector::ZeroVector;
    int32 ValidPlayerCount = 0;

    for (AHSCharacterBase* Player : Players)
    {
        if (Player)
        {
            Center += Player->GetActorLocation();
            ValidPlayerCount++;
        }
    }

    return ValidPlayerCount > 0 ? Center / ValidPlayerCount : FVector::ZeroVector;
}

// 기본 협동 액션 등록
void UHSCoopMechanics::RegisterDefaultCoopActions()
{
    // 동시 공격
    FCoopActionData SimultaneousAttack;
    SimultaneousAttack.ActionID = TEXT("SimultaneousAttack");
    SimultaneousAttack.ActionName = FText::FromString(TEXT("동시 공격"));
    SimultaneousAttack.Description = FText::FromString(TEXT("모든 팀원이 동시에 공격하여 추가 데미지를 가합니다."));
    SimultaneousAttack.ActionType = ECoopActionType::CAT_SimultaneousAttack;
    SimultaneousAttack.MinimumPlayers = 2;
    SimultaneousAttack.MaximumPlayers = 4;
    SimultaneousAttack.SyncTimeWindow = 3.0f;
    SimultaneousAttack.ExecutionDuration = 2.0f;
    SimultaneousAttack.CooldownTime = 30.0f;
    SimultaneousAttack.SuccessRewardMultiplier = 1.5f;
    SimultaneousAttack.MaximumRange = 800.0f;
    RegisterCoopAction(SimultaneousAttack);

    // 부활 지원
    FCoopActionData RevivalAssist;
    RevivalAssist.ActionID = TEXT("RevivalAssist");
    RevivalAssist.ActionName = FText::FromString(TEXT("부활 지원"));
    RevivalAssist.Description = FText::FromString(TEXT("쓰러진 동료를 부활시킵니다."));
    RevivalAssist.ActionType = ECoopActionType::CAT_RevivalAssistance;
    RevivalAssist.MinimumPlayers = 1;
    RevivalAssist.MaximumPlayers = 2;
    RevivalAssist.SyncTimeWindow = 1.0f;
    RevivalAssist.ExecutionDuration = 5.0f;
    RevivalAssist.CooldownTime = 10.0f;
    RevivalAssist.SuccessRewardMultiplier = 1.0f;
    RevivalAssist.MaximumRange = 300.0f;
    RegisterCoopAction(RevivalAssist);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 기본 협동 액션들 등록 완료"));
}

// 기본 콤보 등록
void UHSCoopMechanics::RegisterDefaultCombos()
{
    // 전사-시프 콤보
    FComboChainData WarriorThiefCombo;
    WarriorThiefCombo.ComboID = TEXT("WarriorThiefCombo");
    WarriorThiefCombo.ChainSequence = {TEXT("WarriorStun"), TEXT("ThiefBackstab")};
    WarriorThiefCombo.TimingWindows = {3.0f, 2.0f};
    WarriorThiefCombo.CompletionBonus = 2.0f;
    RegisterComboChain(WarriorThiefCombo);

    // 마법사-전사 콤보
    FComboChainData MageWarriorCombo;
    MageWarriorCombo.ComboID = TEXT("MageWarriorCombo");
    MageWarriorCombo.ChainSequence = {TEXT("MageWeaken"), TEXT("WarriorFinisher")};
    MageWarriorCombo.TimingWindows = {4.0f, 3.0f};
    MageWarriorCombo.CompletionBonus = 2.5f;
    RegisterComboChain(MageWarriorCombo);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 기본 콤보 체인들 등록 완료"));
}

// 타이머 콜백 함수들
void UHSCoopMechanics::OnSyncTimeExpired(const FName& ActionID)
{
    FActiveCoopAction* ActiveAction = ActiveCoopActions.Find(ActionID);
    if (!ActiveAction)
    {
        return;
    }

    const FCoopActionData* ActionData = RegisteredCoopActions.Find(ActionID);
    if (!ActionData)
    {
        return;
    }

    // 최소 인원 확인
    if (ActiveAction->Participants.Num() >= ActionData->MinimumPlayers)
    {
        // 실행 단계로 전환
        ActiveAction->CurrentState = ECoopActionState::CAS_Executing;
        ActiveAction->RemainingTime = ActionData->ExecutionDuration;

        // 실행 타이머 설정
        if (UWorld* World = GetWorld())
        {
            FTimerHandle& ExecutionTimer = ExecutionTimerHandles.FindOrAdd(ActionID);
            World->GetTimerManager().SetTimer(
                ExecutionTimer,
                FTimerDelegate::CreateUFunction(this, FName("OnExecutionCompleted"), ActionID),
                ActionData->ExecutionDuration,
                false
            );
        }

        // 액션별 처리
        switch (ActionData->ActionType)
        {
            case ECoopActionType::CAT_SimultaneousAttack:
                ProcessSimultaneousAttack(*ActionData, *ActiveAction);
                break;
            case ECoopActionType::CAT_RevivalAssistance:
                ProcessRevivalAssistance(*ActionData, *ActiveAction);
                break;
            // 다른 액션 타입들도 필요에 따라 추가
            default:
                break;
        }

        UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 실행 시작"), *ActionID.ToString());
    }
    else
    {
        // 인원 부족으로 실패
        ActiveAction->CurrentState = ECoopActionState::CAS_Failed;
        OnCoopActionFailed.Broadcast(ActionID, TEXT("Not enough participants"));
        
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 협동 액션 '%s' 실패 - 인원 부족"), *ActionID.ToString());
    }

    // 동기화 타이머 정리
    SyncTimerHandles.Remove(ActionID);
}

void UHSCoopMechanics::OnExecutionCompleted(const FName& ActionID)
{
    FActiveCoopAction* ActiveAction = ActiveCoopActions.Find(ActionID);
    if (!ActiveAction)
    {
        return;
    }

    const FCoopActionData* ActionData = RegisteredCoopActions.Find(ActionID);
    if (!ActionData)
    {
        return;
    }

    // 성공 처리
    ActiveAction->CurrentState = ECoopActionState::CAS_Completed;
    ActiveAction->bSuccess = true;
    ActiveAction->Progress = 1.0f;

    // 보상 적용
    ApplySuccessRewards(*ActionData, ActiveAction->Participants);

    // 쿨다운 설정
    ActionCooldowns.Add(ActionID, ActionData->CooldownTime);

    // 이벤트 브로드캐스트
    OnCoopActionCompleted.Broadcast(ActionID, true);

    // 타이머 정리
    ExecutionTimerHandles.Remove(ActionID);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 액션 '%s' 성공적으로 완료"), *ActionID.ToString());
}

void UHSCoopMechanics::HandleRevivalCompleted(AHSCharacterBase* DeadPlayer)
{
    if (!DeadPlayer || !IsRevivalInProgress(DeadPlayer))
    {
        return;
    }

    // 부활 처리 (실제 구현에서는 캐릭터의 부활 메서드 호출)
    if (UHSCombatComponent* CombatComp = DeadPlayer->FindComponentByClass<UHSCombatComponent>())
    {
        // 체력 일부 회복하여 부활
        float RevivalHealth = CombatComp->GetMaxHealth() * 0.3f; // 최대 체력의 30%로 부활
        CombatComp->SetCurrentHealth(RevivalHealth);
    }

    // 이벤트 브로드캐스트
    OnRevivalCompleted.Broadcast(DeadPlayer);

    // 데이터 정리
    RevivalPairs.Remove(DeadPlayer);
    RevivalProgress.Remove(DeadPlayer);
    RevivalTimerHandles.Remove(DeadPlayer);

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: %s 부활 완료"), *DeadPlayer->GetName());
}

// 부활 지원 처리
void UHSCoopMechanics::ProcessRevivalAssistance(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 부활 대상과 부활자 확인
    if (ActiveAction.Participants.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 부활 지원 - 참여자 부족"));
        return;
    }

    AHSCharacterBase* DeadPlayer = nullptr;
    AHSCharacterBase* Reviver = nullptr;

    // 죽은 플레이어와 부활자 구분
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        if (UHSCombatComponent* CombatComp = Participant->FindComponentByClass<UHSCombatComponent>())
        {
            if (CombatComp->IsDead())
            {
                DeadPlayer = Participant;
            }
            else
            {
                Reviver = Participant;
            }
        }
    }

    if (!DeadPlayer || !Reviver)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 부활 대상 또는 부활자를 찾을 수 없음"));
        return;
    }

    // 부활 프로세스 시작
    if (!IsRevivalInProgress(DeadPlayer))
    {
        RequestRevival(DeadPlayer, Reviver);
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 부활 지원 처리 - %s가 %s를 부활 중"), 
           *Reviver->GetName(), *DeadPlayer->GetName());
}

// 콤보 체인 처리
void UHSCoopMechanics::ProcessComboChain(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 콤보 체인 진행 로직
    if (ActiveAction.Participants.Num() < 2)
    {
        return;
    }

    // 각 참여자에게 콤보 버프 적용
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
        {
            // 콤보 체인 버프 적용
            FBuffData ComboChainBuff;
            ComboChainBuff.BuffID = TEXT("ComboChainBuff");
            ComboChainBuff.BuffType = EBuffType::Attack;
            ComboChainBuff.Value = 0.3f; // 30% 공격력 증가
            ComboChainBuff.Duration = 15.0f;
            ComboChainBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(ComboChainBuff);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 콤보 체인 처리 완료 - 참여자 %d명"), ActiveAction.Participants.Num());
}

// 협동 퍼즐 처리
void UHSCoopMechanics::ProcessCooperativePuzzle(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 협동 퍼즐 해결 로직
    if (ActiveAction.Participants.Num() < ActionData.MinimumPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 협동 퍼즐 - 최소 인원 부족"));
        return;
    }

    // 퍼즐 해결 시뮬레이션 (실제로는 게임 상황에 맞게 구현)
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        // 퍼즐 해결 경험치 보상
        if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
        {
            // 지혜 버프 적용
            FBuffData WisdomBuff;
            WisdomBuff.BuffID = TEXT("PuzzleSolverBuff");
            WisdomBuff.BuffType = EBuffType::Defense; // 임시로 방어력 버프 사용
            WisdomBuff.Value = 0.2f; // 20% 방어력 증가
            WisdomBuff.Duration = 30.0f;
            WisdomBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(WisdomBuff);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 협동 퍼즐 해결 완료"));
}

// 공동 목표 처리
void UHSCoopMechanics::ProcessSharedObjective(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 공동 목표 달성 로직
    if (ActiveAction.Participants.Num() == 0)
    {
        return;
    }

    // 모든 참여자에게 목표 달성 보너스 적용
    float BonusMultiplier = ActionData.SuccessRewardMultiplier;
    
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        // 목표 달성 보너스 적용
        if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
        {
            // 올라운드 버프 적용
            FBuffData ObjectiveBuff;
            ObjectiveBuff.BuffID = TEXT("SharedObjectiveBuff");
            ObjectiveBuff.BuffType = EBuffType::Attack;
            ObjectiveBuff.Value = (BonusMultiplier - 1.0f) * 0.5f; // 보너스의 절반을 공격력에
            ObjectiveBuff.Duration = 60.0f;
            ObjectiveBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(ObjectiveBuff);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 공동 목표 달성 - 보너스 배율: %.2f"), BonusMultiplier);
}

// 자원 공유 처리
void UHSCoopMechanics::ProcessResourceSharing(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 자원 공유 활성화
    if (ActiveAction.Participants.Num() < 2)
    {
        return;
    }

    // 팀 자원 풀 활성화
    if (TeamManager && ActiveAction.Participants.Num() > 0)
    {
        APlayerState* FirstPlayerState = ActiveAction.Participants[0]->GetPlayerState();
        if (FirstPlayerState)
        {
            int32 TeamID = TeamManager->GetPlayerTeamID(FirstPlayerState);
            if (TeamID >= 0)
            {
                EnableResourcePool(TeamID, TEXT("Health"));
                EnableResourcePool(TeamID, TEXT("Stamina"));
                EnableResourcePool(TeamID, TEXT("Mana"));
            }
        }
    }

    // 참여자 간 자원 균등화 (예시)
    float TotalHealth = 0.0f;
    float TotalStamina = 0.0f;
    int32 ValidParticipants = 0;

    // 총 자원 계산
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        if (UHSCombatComponent* CombatComp = Participant->FindComponentByClass<UHSCombatComponent>())
        {
            TotalHealth += CombatComp->GetCurrentHealth();
            ValidParticipants++;
        }
        // 스태미너도 비슷하게 처리 (스태미너 컴포넌트가 있다면)
    }

    // 평균 자원으로 재분배 (간소화된 예시)
    if (ValidParticipants > 0)
    {
        float AverageHealth = TotalHealth / ValidParticipants;
        
        for (AHSCharacterBase* Participant : ActiveAction.Participants)
        {
            if (!Participant)
                continue;

            if (UHSCombatComponent* CombatComp = Participant->FindComponentByClass<UHSCombatComponent>())
            {
                // 평균보다 낮은 체력을 가진 플레이어에게 일부 회복
                float CurrentHealth = CombatComp->GetCurrentHealth();
                if (CurrentHealth < AverageHealth * 0.8f)
                {
                    float HealAmount = (AverageHealth - CurrentHealth) * 0.3f;
                    CombatComp->ApplyHealing(HealAmount);
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 자원 공유 처리 완료"));
}

// 대형 이동 처리
void UHSCoopMechanics::ProcessFormationMovement(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 대형 이동 시작
    if (ActiveAction.Participants.Num() < 2 || !ActiveAction.Initiator)
    {
        return;
    }

    // 팀 ID 확인
    int32 TeamID = -1;
    if (TeamManager && ActiveAction.Initiator->GetPlayerState())
    {
        TeamID = TeamManager->GetPlayerTeamID(ActiveAction.Initiator->GetPlayerState());
    }

    if (TeamID >= 0)
    {
        // 대형 설정
        SetTeamFormation(TeamID, TEXT("CircleFormation"), ActiveAction.Initiator);
        
        // 대형 이동 속도 버프 적용
        for (AHSCharacterBase* Participant : ActiveAction.Participants)
        {
            if (!Participant)
                continue;

            if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
            {
                // 이동 속도 버프
                FBuffData SpeedBuff;
                SpeedBuff.BuffID = TEXT("FormationMovementBuff");
                SpeedBuff.BuffType = EBuffType::MovementSpeed;
                SpeedBuff.Value = 0.25f; // 25% 속도 증가
                SpeedBuff.Duration = ActionData.ExecutionDuration;
                SpeedBuff.bIsPercentage = true;
                StatsComp->ApplyBuff(SpeedBuff);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 대형 이동 활성화 - 팀 %d"), TeamID);
}

// 동기화된 방어 처리
void UHSCoopMechanics::ProcessSynchronizedDefense(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 동기화된 방어 활성화
    if (ActiveAction.Participants.Num() < 2)
    {
        return;
    }

    // 모든 참여자에게 방어 버프 적용
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
        {
            // 공동 방어 버프
            FBuffData DefenseBuff;
            DefenseBuff.BuffID = TEXT("SynchronizedDefenseBuff");
            DefenseBuff.BuffType = EBuffType::Defense;
            DefenseBuff.Value = 0.4f * (ActiveAction.Participants.Num() - 1); // 참여자 수에 따라 방어력 증가
            DefenseBuff.Duration = ActionData.ExecutionDuration + 5.0f;
            DefenseBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(DefenseBuff);
        }

        // 데미지 분산 효과 (간소화된 구현)
        if (UHSCombatComponent* CombatComp = Participant->FindComponentByClass<UHSCombatComponent>())
        {
            // 실제로는 데미지 분산 로직을 CombatComponent에 구현해야 함
            UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: %s에게 데미지 분산 효과 적용"), *Participant->GetName());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 동기화된 방어 활성화 - 참여자 %d명"), ActiveAction.Participants.Num());
}

// 연쇄 반응 처리
void UHSCoopMechanics::ProcessChainReaction(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 연쇄 반응 시작
    if (ActiveAction.Participants.Num() == 0)
    {
        return;
    }

    // 연쇄 반응 효과 - 각 참여자가 다음 참여자에게 버프 전파
    for (int32 i = 0; i < ActiveAction.Participants.Num(); ++i)
    {
        AHSCharacterBase* CurrentParticipant = ActiveAction.Participants[i];
        if (!CurrentParticipant)
            continue;

        // 연쇄 반응 계수 (참여자가 많을수록 강해짐)
        float ChainMultiplier = 1.0f + (i * 0.2f); // 20%씩 증가
        
        if (UHSStatsComponent* StatsComp = CurrentParticipant->FindComponentByClass<UHSStatsComponent>())
        {
            // 연쇄 반응 버프
            FBuffData ChainBuff;
            ChainBuff.BuffID = FString::Printf(TEXT("ChainReactionBuff_%d"), i);
            ChainBuff.BuffType = EBuffType::Attack;
            ChainBuff.Value = ChainMultiplier - 1.0f;
            ChainBuff.Duration = 20.0f;
            ChainBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(ChainBuff);
        }

        // 다음 참여자에게 에너지 전파 (시각 효과 등)
        if (i < ActiveAction.Participants.Num() - 1)
        {
            AHSCharacterBase* NextParticipant = ActiveAction.Participants[i + 1];
            if (NextParticipant)
            {
                // 파티클 효과 등 시각적 피드백
                UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 연쇄 반응 전파 %s -> %s"), 
                       *CurrentParticipant->GetName(), *NextParticipant->GetName());
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 연쇄 반응 처리 완료 - %d단계"), ActiveAction.Participants.Num());
}

// 궁극기 콤보 처리
void UHSCoopMechanics::ProcessUltimateCombo(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction)
{
    // 궁극기 콤보 실행
    if (ActiveAction.Participants.Num() < 3) // 최소 3명 필요
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCoopMechanics: 궁극기 콤보 - 최소 3명 필요"));
        return;
    }

    // 모든 참여자에게 강력한 궁극기 버프 적용
    float UltimateMultiplier = ActionData.SuccessRewardMultiplier * ActiveAction.Participants.Num();
    
    for (AHSCharacterBase* Participant : ActiveAction.Participants)
    {
        if (!Participant)
            continue;

        if (UHSStatsComponent* StatsComp = Participant->FindComponentByClass<UHSStatsComponent>())
        {
            // 궁극기 콤보 버프 - 모든 능력치 상승
            FBuffData UltimateBuff;
            UltimateBuff.BuffID = TEXT("UltimateCombo");
            UltimateBuff.BuffType = EBuffType::Attack;
            UltimateBuff.Value = UltimateMultiplier - 1.0f;
            UltimateBuff.Duration = 30.0f;
            UltimateBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(UltimateBuff);

            // 추가 방어 버프
            FBuffData DefenseBuff;
            DefenseBuff.BuffID = TEXT("UltimateComboDefense");
            DefenseBuff.BuffType = EBuffType::Defense;
            DefenseBuff.Value = (UltimateMultiplier - 1.0f) * 0.5f;
            DefenseBuff.Duration = 30.0f;
            DefenseBuff.bIsPercentage = true;
            StatsComp->ApplyBuff(DefenseBuff);
        }

        // 체력과 마나 완전 회복
        if (UHSCombatComponent* CombatComp = Participant->FindComponentByClass<UHSCombatComponent>())
        {
            CombatComp->SetCurrentHealth(CombatComp->GetMaxHealth());
            // 마나도 회복 (마나 시스템이 구현되어 있다면)
        }
    }

    // 강력한 시각/오디오 효과
    FVector ComboCenter = CalculateGroupCenterLocation(ActiveAction.Participants);
    if (ActionData.ActivationEffect)
    {
        // 더 크고 화려한 효과
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), ActionData.ActivationEffect, ComboCenter, 
            FRotator::ZeroRotator, FVector(3.0f, 3.0f, 3.0f) // 3배 크기
        );
    }

    UE_LOG(LogTemp, Log, TEXT("HSCoopMechanics: 궁극기 콤보 발동! 배율: %.2f, 참여자: %d명"), 
           UltimateMultiplier, ActiveAction.Participants.Num());
}

// 월드 가져오기
UWorld* UHSCoopMechanics::GetWorld() const
{
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        return nullptr;
    }

    if (UObject* Outer = GetOuter())
    {
        return Outer->GetWorld();
    }

    return nullptr;
}
