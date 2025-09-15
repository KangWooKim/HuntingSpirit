// 사냥의 영혼(HuntingSpirit) 게임의 플레이어 상태 클래스
// 개별 플레이어의 상태와 통계를 관리하고 네트워크 복제를 담당하는 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HuntingSpirit/Characters/Player/HSPlayerTypes.h"
#include "HSPlayerState.generated.h"

// 전방 선언
class AHSCharacterBase;
class UHSStatsComponent;

// 플레이어 상태 타입
UENUM(BlueprintType)
enum class EHSPlayerStatus : uint8
{
    PS_Alive            UMETA(DisplayName = "Alive"),               // 생존
    PS_Dead             UMETA(DisplayName = "Dead"),                // 사망
    PS_Reviving         UMETA(DisplayName = "Reviving"),           // 부활 중
    PS_Spectating       UMETA(DisplayName = "Spectating"),         // 관전 중
    PS_Disconnected     UMETA(DisplayName = "Disconnected"),       // 연결 끊김
    PS_Loading          UMETA(DisplayName = "Loading")             // 로딩 중
};

// 플레이어 역할
UENUM(BlueprintType)
enum class EHSPlayerRole : uint8
{
    PR_None         UMETA(DisplayName = "None"),           // 역할 없음
    PR_TeamLeader   UMETA(DisplayName = "Team Leader"),    // 팀 리더
    PR_Support      UMETA(DisplayName = "Support"),        // 서포터
    PR_DPS          UMETA(DisplayName = "DPS"),            // 딜러
    PR_Tank         UMETA(DisplayName = "Tank"),           // 탱커
    PR_Healer       UMETA(DisplayName = "Healer")          // 힐러
};

// 플레이어 세션 통계 구조체 (게임 세션 중 통계)
USTRUCT(BlueprintType)
struct FHSPlayerSessionStatistics
{
    GENERATED_BODY()

    // 킬 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 Kills;

    // 데스 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 Deaths;

    // 어시스트 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 Assists;

    // 가한 총 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    float TotalDamageDealt;

    // 받은 총 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    float TotalDamageTaken;

    // 힐링한 총량
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    float TotalHealingDone;

    // 받은 힐링량
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    float TotalHealingReceived;

    // 수집한 자원량
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 ResourcesGathered;

    // 참여한 협동 액션 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 CoopActionsParticipated;

    // 성공한 협동 액션 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 SuccessfulCoopActions;

    // 부활시킨 플레이어 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 PlayersRevived;

    // 부활당한 횟수
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    int32 TimesRevived;

    // 생존 시간 (초)
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    float SurvivalTime;

    // 보스 전투 참여 시간
    UPROPERTY(BlueprintReadOnly, Category = "Player Statistics")
    float BossFightTime;

    // 기본값 설정
    FHSPlayerSessionStatistics()
    {
        Kills = 0;
        Deaths = 0;
        Assists = 0;
        TotalDamageDealt = 0.0f;
        TotalDamageTaken = 0.0f;
        TotalHealingDone = 0.0f;
        TotalHealingReceived = 0.0f;
        ResourcesGathered = 0;
        CoopActionsParticipated = 0;
        SuccessfulCoopActions = 0;
        PlayersRevived = 0;
        TimesRevived = 0;
        SurvivalTime = 0.0f;
        BossFightTime = 0.0f;
    }
};

// 플레이어 레벨 정보
USTRUCT(BlueprintType)
struct FHSPlayerLevelInfo
{
    GENERATED_BODY()

    // 현재 레벨
    UPROPERTY(BlueprintReadOnly, Category = "Player Level")
    int32 CurrentLevel;

    // 현재 경험치
    UPROPERTY(BlueprintReadOnly, Category = "Player Level")
    float CurrentExperience;

    // 다음 레벨까지 필요한 경험치
    UPROPERTY(BlueprintReadOnly, Category = "Player Level")
    float ExperienceToNextLevel;

    // 총 경험치
    UPROPERTY(BlueprintReadOnly, Category = "Player Level")
    float TotalExperience;

    // 사용 가능한 스킬 포인트
    UPROPERTY(BlueprintReadOnly, Category = "Player Level")
    int32 SkillPoints;

    // 현재 레벨의 시작 시간
    UPROPERTY(BlueprintReadOnly, Category = "Player Level")
    float LevelStartTime;

    // 기본값 설정
    FHSPlayerLevelInfo()
    {
        CurrentLevel = 1;
        CurrentExperience = 0.0f;
        ExperienceToNextLevel = 100.0f;
        TotalExperience = 0.0f;
        SkillPoints = 0;
        LevelStartTime = 0.0f;
    }
};

// 소모품 정보 구조체
USTRUCT(BlueprintType)
struct FConsumableItemInfo
{
    GENERATED_BODY()

    // 아이템 타입
    UPROPERTY(BlueprintReadWrite, Category = "Consumable")
    FName ItemType;

    // 보유 수량
    UPROPERTY(BlueprintReadWrite, Category = "Consumable")
    int32 Quantity;

    // 기본 생성자
    FConsumableItemInfo()
    {
        ItemType = NAME_None;
        Quantity = 0;
    }

    // 매개변수 생성자
    FConsumableItemInfo(const FName& InItemType, int32 InQuantity)
        : ItemType(InItemType), Quantity(InQuantity)
    {
    }
};

// 플레이어 인벤토리 상태 (간소화된 버전)
USTRUCT(BlueprintType)
struct FHSPlayerInventoryState
{
    GENERATED_BODY()

    // 소지 중인 아이템 수
    UPROPERTY(BlueprintReadOnly, Category = "Player Inventory")
    int32 ItemCount;

    // 인벤토리 최대 슬롯
    UPROPERTY(BlueprintReadOnly, Category = "Player Inventory")
    int32 MaxSlots;

    // 현재 무기 타입
    UPROPERTY(BlueprintReadOnly, Category = "Player Inventory")
    FName CurrentWeaponType;

    // 소지 중인 소모품 목록
    UPROPERTY(BlueprintReadOnly, Category = "Player Inventory")
    TArray<FConsumableItemInfo> Consumables;

    // 기본값 설정
    FHSPlayerInventoryState()
    {
        ItemCount = 0;
        MaxSlots = 20;
        CurrentWeaponType = NAME_None;
    }
};

// 플레이어 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatusChanged, EHSPlayerStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerLevelUp, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerExperienceGained, float, ExperienceGained, float, TotalExperience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatisticsUpdated, const FHSPlayerSessionStatistics&, NewStatistics);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerTeamChanged, int32, OldTeamID, int32, NewTeamID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerRoleChanged, EHSPlayerRole, OldRole, EHSPlayerRole, NewRole);

/**
 * 사냥의 영혼 플레이어 상태 클래스
 * 
 * 주요 기능:
 * - 개별 플레이어의 상태 및 통계 관리
 * - 플레이어 레벨 및 경험치 시스템
 * - 팀 소속 및 역할 관리
 * - 인벤토리 상태 추적
 * - 네트워크 복제를 통한 클라이언트 동기화
 * - 플레이어별 성과 및 업적 추적
 * - 메모리 최적화 및 성능 모니터링
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API AHSPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    // 생성자
    AHSPlayerState();

    // AActor interface
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 네트워크 복제 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // === 플레이어 상태 관리 ===

    /**
     * 플레이어 상태를 설정합니다
     * @param NewStatus 새로운 플레이어 상태
     */
    UFUNCTION(BlueprintCallable, Category = "Player State")
    void SetPlayerStatus(EHSPlayerStatus NewStatus);

    /**
     * 현재 플레이어 상태를 반환합니다
     * @return 현재 플레이어 상태
     */
    UFUNCTION(BlueprintPure, Category = "Player State")
    EHSPlayerStatus GetPlayerStatus() const { return PlayerStatus; }

    /**
     * 플레이어가 생존 상태인지 확인합니다
     * @return 생존 여부
     */
    UFUNCTION(BlueprintPure, Category = "Player State")
    bool IsPlayerAlive() const { return PlayerStatus == EHSPlayerStatus::PS_Alive; }

    /**
     * 플레이어가 사망 상태인지 확인합니다
     * @return 사망 여부
     */
    UFUNCTION(BlueprintPure, Category = "Player State")
    bool IsPlayerDead() const { return PlayerStatus == EHSPlayerStatus::PS_Dead; }

    /**
     * 플레이어가 부활 중인지 확인합니다
     * @return 부활 중 여부
     */
    UFUNCTION(BlueprintPure, Category = "Player State")
    bool IsPlayerReviving() const { return PlayerStatus == EHSPlayerStatus::PS_Reviving; }

    // === 플레이어 클래스 및 역할 관리 ===

    /**
     * 플레이어 클래스를 설정합니다
     * @param NewPlayerClass 새로운 플레이어 클래스
     */
    UFUNCTION(BlueprintCallable, Category = "Player Class")
    void SetPlayerClass(EHSPlayerClass NewPlayerClass);

    /**
     * 현재 플레이어 클래스를 반환합니다
     * @return 현재 플레이어 클래스
     */
    UFUNCTION(BlueprintPure, Category = "Player Class")
    EHSPlayerClass GetPlayerClass() const { return PlayerClass; }

    /**
     * 플레이어 역할을 설정합니다
     * @param NewRole 새로운 플레이어 역할
     */
    UFUNCTION(BlueprintCallable, Category = "Player Role")
    void SetPlayerRole(EHSPlayerRole NewRole);

    /**
     * 현재 플레이어 역할을 반환합니다
     * @return 현재 플레이어 역할
     */
    UFUNCTION(BlueprintPure, Category = "Player Role")
    EHSPlayerRole GetPlayerRole() const { return PlayerRole; }

    // === 팀 관리 ===

    /**
     * 플레이어의 팀 ID를 설정합니다
     * @param NewTeamID 새로운 팀 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SetTeamID(int32 NewTeamID);

    /**
     * 플레이어의 팀 ID를 반환합니다
     * @return 팀 ID (-1이면 팀 없음)
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    int32 GetTeamID() const { return TeamID; }

    /**
     * 플레이어가 팀에 속해있는지 확인합니다
     * @return 팀 소속 여부
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsInTeam() const { return TeamID >= 0; }

    /**
     * 플레이어가 팀 리더인지 확인합니다
     * @return 팀 리더 여부
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsTeamLeader() const { return PlayerRole == EHSPlayerRole::PR_TeamLeader; }

    // === 레벨 및 경험치 시스템 ===

    /**
     * 경험치를 추가합니다
     * @param ExperienceAmount 추가할 경험치
     * @param bBroadcastEvent 이벤트 브로드캐스트 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Experience")
    void AddExperience(float ExperienceAmount, bool bBroadcastEvent = true);

    /**
     * 레벨을 설정합니다 (관리자 전용)
     * @param NewLevel 새로운 레벨
     */
    UFUNCTION(BlueprintCallable, Category = "Experience")
    void SetLevel(int32 NewLevel);

    /**
     * 현재 레벨을 반환합니다
     * @return 현재 레벨
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    int32 GetCurrentLevel() const { return LevelInfo.CurrentLevel; }

    /**
     * 현재 경험치를 반환합니다
     * @return 현재 경험치
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    float GetCurrentExperience() const { return LevelInfo.CurrentExperience; }

    /**
     * 다음 레벨까지 필요한 경험치를 반환합니다
     * @return 필요 경험치
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    float GetExperienceToNextLevel() const { return LevelInfo.ExperienceToNextLevel; }

    /**
     * 레벨 진행률을 반환합니다 (0.0 ~ 1.0)
     * @return 레벨 진행률
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    float GetLevelProgress() const;

    /**
     * 레벨 정보를 반환합니다
     * @return 레벨 정보 구조체
     */
    UFUNCTION(BlueprintPure, Category = "Experience")
    FHSPlayerLevelInfo GetLevelInfo() const { return LevelInfo; }

    // === 통계 관리 ===

    /**
     * 킬 수를 증가시킵니다
     * @param KillCount 증가시킬 킬 수
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void IncrementKills(int32 KillCount = 1);

    /**
     * 데스 수를 증가시킵니다
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void IncrementDeaths();

    /**
     * 어시스트 수를 증가시킵니다
     * @param AssistCount 증가시킬 어시스트 수
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void IncrementAssists(int32 AssistCount = 1);

    /**
     * 데미지 통계를 업데이트합니다
     * @param DamageDealt 가한 데미지
     * @param DamageTaken 받은 데미지
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void UpdateDamageStatistics(float DamageDealt = 0.0f, float DamageTaken = 0.0f);

    /**
     * 힐링 통계를 업데이트합니다
     * @param HealingDone 한 힐링량
     * @param HealingReceived 받은 힐링량
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void UpdateHealingStatistics(float HealingDone = 0.0f, float HealingReceived = 0.0f);

    /**
     * 자원 수집 통계를 업데이트합니다
     * @param ResourceAmount 수집한 자원량
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void UpdateResourceStatistics(int32 ResourceAmount);

    /**
     * 협동 액션 통계를 업데이트합니다
     * @param bSuccess 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void UpdateCoopActionStatistics(bool bSuccess = true);

    /**
     * 부활 관련 통계를 업데이트합니다
     * @param bRevived 부활당했는지 여부 (false면 부활시킨 것)
     */
    UFUNCTION(BlueprintCallable, Category = "Player Statistics")
    void UpdateRevivalStatistics(bool bRevived = true);

    /**
     * 플레이어 통계를 반환합니다
     * @return 현재 플레이어 통계
     */
    UFUNCTION(BlueprintPure, Category = "Player Statistics")
    FHSPlayerSessionStatistics GetPlayerStatistics() const { return PlayerStatistics; }

    /**
     * KDA 비율을 계산합니다
     * @return KDA 비율
     */
    UFUNCTION(BlueprintPure, Category = "Player Statistics")
    float GetKDARate() const;

    /**
     * 분당 평균 데미지를 계산합니다
     * @return 분당 평균 데미지
     */
    UFUNCTION(BlueprintPure, Category = "Player Statistics")
    float GetDamagePerMinute() const;

    // === 인벤토리 상태 관리 ===

    /**
     * 인벤토리 상태를 업데이트합니다
     * @param NewInventoryState 새로운 인벤토리 상태
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void UpdateInventoryState(const FHSPlayerInventoryState& NewInventoryState);

    /**
     * 현재 무기 타입을 설정합니다
     * @param WeaponType 무기 타입
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetCurrentWeapon(const FName& WeaponType);

    /**
     * 소모품을 추가합니다
     * @param ItemType 아이템 타입
     * @param Amount 수량
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddConsumable(const FName& ItemType, int32 Amount);

    /**
     * 인벤토리 상태를 반환합니다
     * @return 현재 인벤토리 상태
     */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    FHSPlayerInventoryState GetInventoryState() const { return InventoryState; }

    // === 성능 및 네트워크 정보 ===

    /**
     * 플레이어의 핑을 반환합니다
     * @return 현재 핑 (밀리초)
     */
    UFUNCTION(BlueprintPure, Category = "Network")
    float GetPlayerPing() const;

    /**
     * 패킷 손실률을 반환합니다
     * @return 패킷 손실률 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Network")
    float GetPacketLossRate() const;

    /**
     * 연결 품질을 반환합니다
     * @return 연결 품질 (0 = 매우 나쁨, 4 = 매우 좋음)
     */
    UFUNCTION(BlueprintPure, Category = "Network")
    int32 GetConnectionQuality() const;

    // === 유틸리티 함수들 ===

    /**
     * 플레이어의 총 플레이 시간을 반환합니다 (초 단위)
     * @return 총 플레이 시간
     */
    UFUNCTION(BlueprintPure, Category = "Player Info")
    float GetTotalPlayTime() const;

    /**
     * 현재 생존 시간을 반환합니다 (초 단위)
     * @return 현재 생존 시간
     */
    UFUNCTION(BlueprintPure, Category = "Player Info")
    float GetCurrentSurvivalTime() const;

    /**
     * 플레이어 정보를 문자열로 반환합니다 (디버그용)
     * @return 플레이어 정보 문자열
     */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    FString GetPlayerInfoString() const;

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerStatusChanged OnPlayerStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerLevelUp OnPlayerLevelUp;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerExperienceGained OnPlayerExperienceGained;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerStatisticsUpdated OnPlayerStatisticsUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerTeamChanged OnPlayerTeamChanged;

    UPROPERTY(BlueprintAssignable, Category = "Player Events")
    FOnPlayerRoleChanged OnPlayerRoleChanged;

protected:
    // === 복제되는 플레이어 상태 변수들 ===

    // 플레이어 상태
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    EHSPlayerStatus PlayerStatus;

    // 플레이어 클래스
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    EHSPlayerClass PlayerClass;

    // 플레이어 역할
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    EHSPlayerRole PlayerRole;

    // 팀 ID
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    int32 TeamID;

    // 플레이어 통계
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    FHSPlayerSessionStatistics PlayerStatistics;

    // 레벨 정보
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    FHSPlayerLevelInfo LevelInfo;

    // 인벤토리 상태
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player State")
    FHSPlayerInventoryState InventoryState;

    // === 타이밍 관련 변수들 ===

    // 플레이 시작 시간
    UPROPERTY(BlueprintReadOnly, Category = "Player Timing")
    float PlayStartTime;

    // 현재 생존 시작 시간
    UPROPERTY(BlueprintReadOnly, Category = "Player Timing")
    float CurrentLifeStartTime;

    // 마지막 액션 시간
    UPROPERTY(BlueprintReadOnly, Category = "Player Timing")
    float LastActionTime;

    // === 타이머 핸들들 ===

    // 생존 시간 업데이트 타이머
    FTimerHandle SurvivalTimeUpdateTimer;

    // 통계 업데이트 타이머
    FTimerHandle StatisticsUpdateTimer;

    // 네트워크 상태 체크 타이머
    FTimerHandle NetworkStatusTimer;

    // === 설정 가능한 매개변수들 ===

    // 경험치 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Configuration")
    float ExperienceMultiplier;

    // 기본 레벨별 필요 경험치
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Configuration")
    float BaseExperiencePerLevel;

    // 레벨별 경험치 증가율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Configuration")
    float ExperienceScalingFactor;

    // 최대 레벨
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Configuration")
    int32 MaxLevel;

    // 통계 업데이트 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    float StatisticsUpdateInterval;

    // 네트워크 상태 체크 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Configuration")
    float NetworkStatusCheckInterval;

private:
    // === 내부 헬퍼 함수들 ===

    // 다음 레벨까지 필요한 경험치 계산
    float CalculateExperienceForNextLevel(int32 Level) const;

    // 레벨업 처리
    void ProcessLevelUp(int32 NewLevel);

    // 생존 시간 업데이트
    UFUNCTION()
    void UpdateSurvivalTime();

    // 통계 자동 업데이트
    UFUNCTION()
    void AutoUpdateStatistics();

    // 네트워크 상태 체크
    UFUNCTION()
    void CheckNetworkStatus();

    // 타이머 설정
    void SetupTimers();

    // === 네트워크 복제 콜백 함수들 ===

    UFUNCTION()
    void OnRep_PlayerStatus();

    UFUNCTION()
    void OnRep_PlayerClass();

    UFUNCTION()
    void OnRep_PlayerRole();

    UFUNCTION()
    void OnRep_TeamID();

    UFUNCTION()
    void OnRep_PlayerStatistics();

    UFUNCTION()
    void OnRep_LevelInfo();

    UFUNCTION()
    void OnRep_InventoryState();

    // === 디버그 및 로깅 함수들 ===

    // 플레이어 상태 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogPlayerState() const;

    // 플레이어 통계 로그 출력
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void LogPlayerStatistics() const;

    // === 메모리 최적화 관련 ===

    // 사용하지 않는 데이터 정리
    void CleanupUnusedData();

    // 초기화 완료 플래그
    bool bInitialized;

    // 스레드 안전성을 위한 뮤텍스
    mutable FCriticalSection StatisticsMutex;
    mutable FCriticalSection LevelInfoMutex;
};
