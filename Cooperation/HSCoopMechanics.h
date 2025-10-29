// 사냥의 영혼(HuntingSpirit) 게임의 협동 메커니즘 시스템
// 플레이어들 간의 다양한 협동 액션과 메커니즘을 관리하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "HSTeamManager.h"
#include "SharedAbilities/HSSharedAbilitySystem.h"
#include "HSCoopMechanics.generated.h"

// 전방 선언
class AHSCharacterBase;
class UHSCombatComponent;
class AHSPlayerController;

// 협동 액션 타입 정의
UENUM(BlueprintType)
enum class ECoopActionType : uint8
{
    CAT_SimultaneousAttack      UMETA(DisplayName = "Simultaneous Attack"),      // 동시 공격
    CAT_ComboChain             UMETA(DisplayName = "Combo Chain"),               // 콤보 연계
    CAT_CooperativePuzzle      UMETA(DisplayName = "Cooperative Puzzle"),        // 협동 퍼즐
    CAT_SharedObjective        UMETA(DisplayName = "Shared Objective"),          // 공동 목표
    CAT_RevivalAssistance      UMETA(DisplayName = "Revival Assistance"),        // 부활 지원
    CAT_ResourceSharing        UMETA(DisplayName = "Resource Sharing"),          // 자원 공유
    CAT_FormationMovement      UMETA(DisplayName = "Formation Movement"),        // 대형 이동
    CAT_SynchronizedDefense    UMETA(DisplayName = "Synchronized Defense"),      // 동기화된 방어
    CAT_ChainReaction          UMETA(DisplayName = "Chain Reaction"),            // 연쇄 반응
    CAT_UltimateCombo          UMETA(DisplayName = "Ultimate Combo")             // 궁극기 콤보
};

// 협동 액션 상태
UENUM(BlueprintType)
enum class ECoopActionState : uint8
{
    CAS_Inactive        UMETA(DisplayName = "Inactive"),         // 비활성
    CAS_Preparing       UMETA(DisplayName = "Preparing"),        // 준비 중
    CAS_WaitingSync     UMETA(DisplayName = "Waiting Sync"),     // 동기화 대기
    CAS_Executing       UMETA(DisplayName = "Executing"),        // 실행 중
    CAS_Completed       UMETA(DisplayName = "Completed"),        // 완료
    CAS_Failed          UMETA(DisplayName = "Failed"),           // 실패
    CAS_Cooldown        UMETA(DisplayName = "Cooldown")          // 쿨다운
};

// 협동 액션 데이터
USTRUCT(BlueprintType)
struct FCoopActionData
{
    GENERATED_BODY()

    // 액션 고유 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    FName ActionID;

    // 액션 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    FText ActionName;

    // 액션 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    FText Description;

    // 액션 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    ECoopActionType ActionType;

    // 필요한 최소 플레이어 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    int32 MinimumPlayers;

    // 필요한 최대 플레이어 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    int32 MaximumPlayers;

    // 동기화 시간 윈도우 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    float SyncTimeWindow;

    // 실행 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    float ExecutionDuration;

    // 쿨다운 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    float CooldownTime;

    // 성공 시 보상 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    float SuccessRewardMultiplier;

    // 최대 거리 제한
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    float MaximumRange;

    // 필요한 클래스 조합
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    TArray<FName> RequiredClassCombination;

    // 비주얼 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    UNiagaraSystem* ActivationEffect;

    // 사운드 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop Action")
    USoundBase* ActivationSound;

    // 기본값 설정
    FCoopActionData()
    {
        MinimumPlayers = 2;
        MaximumPlayers = 4;
        SyncTimeWindow = 3.0f;
        ExecutionDuration = 5.0f;
        CooldownTime = 30.0f;
        SuccessRewardMultiplier = 1.5f;
        MaximumRange = 1000.0f;
        ActivationEffect = nullptr;
        ActivationSound = nullptr;
    }
};

// 활성 협동 액션 정보
USTRUCT(BlueprintType)
struct FActiveCoopAction
{
    GENERATED_BODY()

    // 액션 ID
    UPROPERTY(BlueprintReadOnly)
    FName ActionID;

    // 참여 플레이어들
    UPROPERTY(BlueprintReadOnly)
    TArray<AHSCharacterBase*> Participants;

    // 현재 상태
    UPROPERTY(BlueprintReadOnly)
    ECoopActionState CurrentState;

    // 남은 시간 (상태에 따라 다른 의미)
    UPROPERTY(BlueprintReadOnly)
    float RemainingTime;

    // 시작 시간
    UPROPERTY(BlueprintReadOnly)
    float StartTime;

    // 액션 발동자
    UPROPERTY(BlueprintReadOnly)
    AHSCharacterBase* Initiator;

    // 성공 여부
    UPROPERTY(BlueprintReadOnly)
    bool bSuccess;

    // 진행률 (0.0 ~ 1.0)
    UPROPERTY(BlueprintReadOnly)
    float Progress;

    // 기본값 설정
    FActiveCoopAction()
    {
        CurrentState = ECoopActionState::CAS_Inactive;
        RemainingTime = 0.0f;
        StartTime = 0.0f;
        Initiator = nullptr;
        bSuccess = false;
        Progress = 0.0f;
    }
};

// 팀 자원 풀 구조체
USTRUCT(BlueprintType)
struct FTeamResourcePool
{
    GENERATED_BODY()

    // 자원 타입별 보유량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Pool")
    TMap<FName, float> Resources;

    // 기본 생성자
    FTeamResourcePool()
    {
        Resources.Empty();
    }
};

// 콤보 체인 데이터
USTRUCT(BlueprintType)
struct FComboChainData
{
    GENERATED_BODY()

    // 콤보 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
    FName ComboID;

    // 콤보 단계별 액션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
    TArray<FName> ChainSequence;

    // 각 단계별 타이밍 윈도우
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
    TArray<float> TimingWindows;

    // 콤보 완성 시 보너스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
    float CompletionBonus;

    // 현재 진행 단계
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentStep;

    // 마지막 액션 시간
    UPROPERTY(BlueprintReadOnly)
    float LastActionTime;

    // 기본값 설정
    FComboChainData()
    {
        CompletionBonus = 2.0f;
        CurrentStep = 0;
        LastActionTime = 0.0f;
    }
};

// 협동 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoopActionStarted, const FName&, ActionID, const TArray<AHSCharacterBase*>&, Participants);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoopActionCompleted, const FName&, ActionID, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoopActionFailed, const FName&, ActionID, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnComboChainProgress, const FName&, ComboID, int32, CurrentStep, int32, TotalSteps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboChainCompleted, const FName&, ComboID, float, BonusMultiplier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRevivalRequested, AHSCharacterBase*, DeadPlayer, AHSCharacterBase*, Reviver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRevivalCompleted, AHSCharacterBase*, RevivedPlayer);

/**
 * 협동 메커니즘 시스템 클래스
 * 
 * 주요 기능:
 * - 동시 액션 및 콤보 시스템 관리
 * - 협동 퍼즐 및 공동 목표 처리
 * - 플레이어 부활 시스템
 * - 자원 공유 메커니즘
 * - 대형 이동 및 동기화된 방어
 * - 연쇄 반응 및 궁극기 콤보
 * - 메모리 최적화 및 오브젝트 풀링 적용
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSCoopMechanics : public UObject
{
    GENERATED_BODY()

public:
    // 생성자
    UHSCoopMechanics();

    // 초기화 및 정리
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    void Initialize(UHSTeamManager* InTeamManager, UHSSharedAbilitySystem* InSharedAbilitySystem);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    void Shutdown();

    // 협동 액션 등록 및 관리
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    void RegisterCoopAction(const FCoopActionData& ActionData);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    void UnregisterCoopAction(const FName& ActionID);

    // 협동 액션 실행
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    bool InitiateCoopAction(const FName& ActionID, AHSCharacterBase* Initiator, const TArray<AHSCharacterBase*>& Participants);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    bool JoinCoopAction(const FName& ActionID, AHSCharacterBase* Player);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    void CancelCoopAction(const FName& ActionID);

    // 협동 액션 조건 확인
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics")
    bool CanInitiateCoopAction(const FName& ActionID, AHSCharacterBase* Initiator, const TArray<AHSCharacterBase*>& Participants, FString& OutFailureReason);

    // 콤보 체인 시스템
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Combo")
    void RegisterComboChain(const FComboChainData& ComboData);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Combo")
    bool TriggerComboStep(const FName& ComboID, const FName& ActionID, AHSCharacterBase* Player);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Combo")
    void ResetComboChain(const FName& ComboID);

    // 부활 시스템
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Revival")
    bool RequestRevival(AHSCharacterBase* DeadPlayer, AHSCharacterBase* Reviver);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Revival")
    void CancelRevival(AHSCharacterBase* DeadPlayer);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Revival")
    bool IsRevivalInProgress(AHSCharacterBase* DeadPlayer) const;

    // 자원 공유 시스템
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Resource")
    bool ShareResource(AHSCharacterBase* Giver, AHSCharacterBase* Receiver, const FName& ResourceType, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Resource")
    void EnableResourcePool(int32 TeamID, const FName& ResourceType);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Resource")
    void DisableResourcePool(int32 TeamID, const FName& ResourceType);

    // 대형 이동 시스템
    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Formation")
    bool SetTeamFormation(int32 TeamID, const FName& FormationType, AHSCharacterBase* Leader);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Formation")
    void UpdateFormationMovement(int32 TeamID, const FVector& TargetLocation);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Formation")
    void BreakFormation(int32 TeamID);

    // 정보 조회 함수들
    UFUNCTION(BlueprintPure, Category = "Coop Mechanics")
    FActiveCoopAction GetActiveCoopAction(const FName& ActionID) const;

    UFUNCTION(BlueprintPure, Category = "Coop Mechanics")
    TArray<FActiveCoopAction> GetAllActiveCoopActions() const;

    UFUNCTION(BlueprintPure, Category = "Coop Mechanics")
    bool IsCoopActionActive(const FName& ActionID) const;

    UFUNCTION(BlueprintPure, Category = "Coop Mechanics")
    float GetCoopActionCooldown(const FName& ActionID) const;

    UFUNCTION(BlueprintPure, Category = "Coop Mechanics|Combo")
    FComboChainData GetComboChainData(const FName& ComboID) const;

    UFUNCTION(BlueprintPure, Category = "Coop Mechanics|Combo")
    int32 GetComboChainProgress(const FName& ComboID) const;

    // 틱 업데이트
    void TickCoopMechanics(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Coop Mechanics|Performance")
    void RequestCacheInvalidation();

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnCoopActionStarted OnCoopActionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnCoopActionCompleted OnCoopActionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnCoopActionFailed OnCoopActionFailed;

    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnComboChainProgress OnComboChainProgress;

    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnComboChainCompleted OnComboChainCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnRevivalRequested OnRevivalRequested;

    UPROPERTY(BlueprintAssignable, Category = "Coop Events")
    FOnRevivalCompleted OnRevivalCompleted;

protected:
    // 협동 액션 처리 함수들
    void ProcessSimultaneousAttack(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessComboChain(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessCooperativePuzzle(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessSharedObjective(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessRevivalAssistance(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessResourceSharing(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessFormationMovement(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessSynchronizedDefense(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessChainReaction(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);
    void ProcessUltimateCombo(const FCoopActionData& ActionData, FActiveCoopAction& ActiveAction);

    // 조건 확인 함수들
    bool CheckPlayerProximity(const TArray<AHSCharacterBase*>& Players, float MaxRange) const;
    bool CheckClassCombination(const TArray<FName>& RequiredClasses, const TArray<AHSCharacterBase*>& Players) const;
    bool CheckCooldownReady(const FName& ActionID) const;
    bool CheckPlayersAlive(const TArray<AHSCharacterBase*>& Players) const;

    // 타이머 콜백 함수들
    void OnSyncTimeExpired(const FName& ActionID);
    void OnExecutionCompleted(const FName& ActionID);
    void OnCooldownExpired(const FName& ActionID);
    void HandleRevivalCompleted(AHSCharacterBase* DeadPlayer);

    // 보상 및 효과 적용
    void ApplySuccessRewards(const FCoopActionData& ActionData, const TArray<AHSCharacterBase*>& Participants);
    void ApplyFailurePenalties(const FCoopActionData& ActionData, const TArray<AHSCharacterBase*>& Participants);

private:
    // 팀 매니저 참조
    UPROPERTY()
    UHSTeamManager* TeamManager;

    // 공유 능력 시스템 참조
    UPROPERTY()
    UHSSharedAbilitySystem* SharedAbilitySystem;

    // 등록된 협동 액션 데이터
    UPROPERTY()
    TMap<FName, FCoopActionData> RegisteredCoopActions;

    // 활성 협동 액션들
    UPROPERTY()
    TMap<FName, FActiveCoopAction> ActiveCoopActions;

    // 협동 액션 쿨다운 관리
    UPROPERTY()
    TMap<FName, float> ActionCooldowns;

    // 콤보 체인 시스템
    UPROPERTY()
    TMap<FName, FComboChainData> RegisteredCombos;

    // 부활 진행 상황
    UPROPERTY()
    TMap<AHSCharacterBase*, AHSCharacterBase*> RevivalPairs; // DeadPlayer -> Reviver

    UPROPERTY()
    TMap<AHSCharacterBase*, float> RevivalProgress; // DeadPlayer -> Progress

    // 자원 풀 시스템
    UPROPERTY()
    TMap<int32, FTeamResourcePool> TeamResourcePools; // TeamID -> ResourcePool

    // 대형 시스템
    UPROPERTY()
    TMap<int32, FName> TeamFormations; // TeamID -> FormationType

    UPROPERTY()
    TMap<int32, AHSCharacterBase*> FormationLeaders; // TeamID -> Leader

    // 타이머 핸들들
    TMap<FName, FTimerHandle> SyncTimerHandles;
    TMap<FName, FTimerHandle> ExecutionTimerHandles;
    TMap<FName, FTimerHandle> CooldownTimerHandles;
    TMap<AHSCharacterBase*, FTimerHandle> RevivalTimerHandles;

    // 메모리 풀링
    TArray<FActiveCoopAction> CoopActionPool;
    int32 CoopActionPoolSize;

    // 성능 최적화를 위한 캐시
    mutable TMap<uint32, bool> ProximityCheckCache;
    mutable float CacheInvalidationTimer;

    // 공간 분할을 위한 해시 그리드
    TMap<int32, TArray<AHSCharacterBase*>> SpatialHashGrid;
    float SpatialHashCellSize;

    // 헬퍼 함수들
    FActiveCoopAction* GetPooledCoopAction();
    void ReturnCoopActionToPool(FActiveCoopAction* Action);
    void UpdateSpatialHash(const TArray<AHSCharacterBase*>& Players);
    uint32 GetProximityCheckHash(const TArray<AHSCharacterBase*>& Players, float Range) const;
    void InvalidateCache();
    FVector CalculateGroupCenterLocation(const TArray<AHSCharacterBase*>& Players) const;
    void RegisterDefaultCoopActions();
    void RegisterDefaultCombos();
    
    // UObject 인터페이스
    virtual UWorld* GetWorld() const override;
    
    // 네트워킹 지원
    bool bNetworkingEnabled;
    void BroadcastCoopActionToClients(const FName& ActionID, const FActiveCoopAction& Action);
    void SendCoopActionUpdate(const FName& ActionID, ECoopActionState NewState);

    // 디버그 및 로깅
    void LogCoopActionStatus(const FName& ActionID) const;
    void DrawDebugCoopAction(const FActiveCoopAction& Action) const;

    // 초기화 확인
    bool bIsInitialized;
};
