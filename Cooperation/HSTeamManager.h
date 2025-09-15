// 사냥의 영혼(HuntingSpirit) 게임의 팀 관리자 클래스
// 플레이어들을 팀으로 관리하고 협동 플레이를 지원하는 시스템

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "HSTeamManager.generated.h"

// 전방 선언
class AHSPlayerController;
class AHSPlayerCharacter;

/**
 * 팀 정보를 저장하는 구조체
 * 네트워크 복제를 지원하여 모든 클라이언트에서 팀 상태를 동기화
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSTeamInfo
{
    GENERATED_BODY()

    // 팀 고유 ID
    UPROPERTY(BlueprintReadOnly, Category = "Team")
    int32 TeamID;

    // 팀 리더의 PlayerState (Blueprint 접근 불필요 - 내부 로직용)
    UPROPERTY()
    TWeakObjectPtr<APlayerState> TeamLeader;

    // 팀원들의 PlayerState 배열 (Blueprint 접근 불필요 - 내부 로직용)
    UPROPERTY()
    TArray<TWeakObjectPtr<APlayerState>> TeamMembers;

    // 팀 생성 시간
    UPROPERTY(BlueprintReadOnly, Category = "Team")
    float CreationTime;

    // 팀이 활성화 상태인지 여부
    UPROPERTY(BlueprintReadOnly, Category = "Team")
    bool bIsActive;

    // 최대 팀원 수
    UPROPERTY(BlueprintReadOnly, Category = "Team")
    int32 MaxTeamSize;

    // 팀 공유 체력 (선택적 사용)
    UPROPERTY(BlueprintReadOnly, Category = "Team")
    float SharedHealth;

    // 팀 레벨 (협동 보너스 적용)
    UPROPERTY(BlueprintReadOnly, Category = "Team")
    int32 TeamLevel;

    // 생성자 - 기본값 설정
    FHSTeamInfo()
    {
        TeamID = -1;
        TeamLeader = nullptr;
        TeamMembers.Empty();
        CreationTime = 0.0f;
        bIsActive = false;
        MaxTeamSize = 4; // 기본 최대 4명
        SharedHealth = 100.0f;
        TeamLevel = 1;
    }

    // 팀원 수 반환
    int32 GetTeamMemberCount() const
    {
        return TeamMembers.Num();
    }

    // 팀이 가득 찼는지 확인
    bool IsTeamFull() const
    {
        return GetTeamMemberCount() >= MaxTeamSize;
    }

    // 특정 플레이어가 팀에 속해있는지 확인
    bool IsPlayerInTeam(APlayerState* PlayerState) const
    {
        if (!PlayerState)
        {
            return false;
        }

        // 리더인지 확인
        if (TeamLeader.IsValid() && TeamLeader.Get() == PlayerState)
        {
            return true;
        }

        // 팀원인지 확인
        return TeamMembers.ContainsByPredicate([PlayerState](const TWeakObjectPtr<APlayerState>& Member)
        {
            return Member.IsValid() && Member.Get() == PlayerState;
        });
    }

    // 유효하지 않은 팀원들 정리 (가비지 컬렉션 대응)
    void CleanupInvalidMembers()
    {
        // 유효하지 않은 팀원 제거
        TeamMembers.RemoveAll([](const TWeakObjectPtr<APlayerState>& Member)
        {
            return !Member.IsValid();
        });

        // 리더가 유효하지 않으면 null로 설정
        if (!TeamLeader.IsValid())
        {
            TeamLeader = nullptr;
        }
    }
};

/**
 * 팀 이벤트 델리게이트 선언
 * 팀 상태 변경 시 다른 시스템들이 반응할 수 있도록 함
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamCreated, int32, TeamID, const FHSTeamInfo&, TeamInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamDisbanded, int32, TeamID, const FHSTeamInfo&, TeamInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerJoinedTeam, int32, TeamID, APlayerState*, PlayerState, const FHSTeamInfo&, TeamInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerLeftTeam, int32, TeamID, APlayerState*, PlayerState, const FHSTeamInfo&, TeamInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTeamLeaderChanged, int32, TeamID, APlayerState*, NewLeader, APlayerState*, OldLeader);

/**
 * 팀 관리자 클래스 - GameInstanceSubsystem으로 구현
 * 
 * 주요 기능:
 * - 팀 생성, 해체, 관리
 * - 플레이어 팀 가입/탈퇴 처리
 * - 팀 상태 네트워크 동기화
 * - 팀 기반 협동 메커니즘 지원
 * - 메모리 최적화 및 오브젝트 풀링 적용
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSTeamManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 생성자
    UHSTeamManager();

    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 네트워크 복제 지원
    virtual bool IsSupportedForNetworking() const override { return true; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // 팀 데이터 저장소 (네트워크 복제 지원을 위해 배열 사용)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Team Management")
    TArray<FHSTeamInfo> TeamDatabase;

    // 플레이어별 팀 매핑 (빠른 검색을 위한 캐시 - Blueprint 접근 불필요)
    UPROPERTY()
    TMap<TWeakObjectPtr<APlayerState>, int32> PlayerToTeamMap;

    // 다음 팀 ID (고유성 보장)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Team Management")
    int32 NextTeamID;

    // 팀 생성 시간 기록 (성능 분석용)
    UPROPERTY(BlueprintReadOnly, Category = "Team Management")
    TMap<int32, float> TeamCreationTimes;

    // 메모리 풀링을 위한 비활성 팀 정보 풀
    UPROPERTY()
    TArray<FHSTeamInfo> InactiveTeamPool;

    // 최적화: 정리 작업 타이머
    UPROPERTY()
    FTimerHandle CleanupTimerHandle;

    // 정리 작업 주기 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
    float CleanupInterval;

    // 최대 팀 개수 제한 (메모리 관리)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxTeamsAllowed;

    // 기본 팀 최대 인원
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 DefaultMaxTeamSize;

public:
    // 팀 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Team Events")
    FOnTeamCreated OnTeamCreated;

    UPROPERTY(BlueprintAssignable, Category = "Team Events")
    FOnTeamDisbanded OnTeamDisbanded;

    UPROPERTY(BlueprintAssignable, Category = "Team Events")
    FOnPlayerJoinedTeam OnPlayerJoinedTeam;

    UPROPERTY(BlueprintAssignable, Category = "Team Events")
    FOnPlayerLeftTeam OnPlayerLeftTeam;

    UPROPERTY(BlueprintAssignable, Category = "Team Events")
    FOnTeamLeaderChanged OnTeamLeaderChanged;

    // === 팀 생성 및 관리 기능 ===

    /**
     * 새로운 팀을 생성합니다
     * @param TeamLeader 팀 리더가 될 플레이어
     * @param MaxTeamSize 최대 팀원 수 (기본값 사용 시 -1)
     * @return 생성된 팀의 ID, 실패 시 -1
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    int32 CreateTeam(APlayerState* TeamLeader, int32 MaxTeamSize = -1);

    /**
     * 팀을 해체합니다
     * @param TeamID 해체할 팀의 ID
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool DisbandTeam(int32 TeamID);

    /**
     * 플레이어를 팀에 추가합니다
     * @param TeamID 대상 팀 ID
     * @param PlayerState 추가할 플레이어
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool AddPlayerToTeam(int32 TeamID, APlayerState* PlayerState);

    /**
     * 플레이어를 팀에서 제거합니다
     * @param PlayerState 제거할 플레이어
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool RemovePlayerFromTeam(APlayerState* PlayerState);

    /**
     * 팀 리더를 변경합니다
     * @param TeamID 대상 팀 ID
     * @param NewLeader 새로운 리더
     * @return 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool ChangeTeamLeader(int32 TeamID, APlayerState* NewLeader);

    // === 팀 정보 조회 기능 ===

    /**
     * 플레이어가 속한 팀 ID를 반환합니다
     * @param PlayerState 조회할 플레이어
     * @return 팀 ID, 팀에 속하지 않으면 -1
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    int32 GetPlayerTeamID(APlayerState* PlayerState) const;

    /**
     * 팀 정보를 반환합니다
     * @param TeamID 조회할 팀 ID
     * @return 팀 정보, 존재하지 않으면 빈 구조체
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    FHSTeamInfo GetTeamInfo(int32 TeamID) const;

    /**
     * 플레이어가 속한 팀의 정보를 반환합니다
     * @param PlayerState 조회할 플레이어
     * @return 팀 정보, 팀에 속하지 않으면 빈 구조체
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    FHSTeamInfo GetPlayerTeamInfo(APlayerState* PlayerState) const;

    /**
     * 모든 활성 팀의 ID 목록을 반환합니다
     * @return 활성 팀 ID 배열
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    TArray<int32> GetAllActiveTeamIDs() const;

    /**
     * 플레이어가 팀에 속해있는지 확인합니다
     * @param PlayerState 확인할 플레이어
     * @return 팀 소속 여부
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsPlayerInTeam(APlayerState* PlayerState) const;

    /**
     * 두 플레이어가 같은 팀인지 확인합니다
     * @param PlayerA 첫 번째 플레이어
     * @param PlayerB 두 번째 플레이어
     * @return 같은 팀 여부
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool ArePlayersInSameTeam(APlayerState* PlayerA, APlayerState* PlayerB) const;

    /**
     * 플레이어가 팀 리더인지 확인합니다
     * @param PlayerState 확인할 플레이어
     * @return 팀 리더 여부
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsPlayerTeamLeader(APlayerState* PlayerState) const;

    /**
     * 팀의 리더를 반환합니다 (Blueprint 안전 접근)
     * @param TeamID 조회할 팀 ID
     * @return 팀 리더, 없으면 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    APlayerState* GetTeamLeader(int32 TeamID) const;

    /**
     * 팀원들의 목록을 반환합니다 (Blueprint 안전 접근)
     * @param TeamID 조회할 팀 ID
     * @return 팀원 배열
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    TArray<APlayerState*> GetTeamMembers(int32 TeamID) const;

    /**
     * 플레이어가 속한 팀의 리더를 반환합니다
     * @param PlayerState 조회할 플레이어
     * @return 팀 리더, 없으면 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    APlayerState* GetPlayerTeamLeader(APlayerState* PlayerState) const;

    /**
     * 플레이어가 속한 팀의 팀원들을 반환합니다
     * @param PlayerState 조회할 플레이어
     * @return 팀원 배열 (자신 포함)
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    TArray<APlayerState*> GetPlayerTeamMembers(APlayerState* PlayerState) const;

    // === 팀 협동 기능 ===

    /**
     * 팀 전체에게 메시지를 브로드캐스트합니다
     * @param TeamID 대상 팀 ID
     * @param Message 전송할 메시지
     * @param bIncludeLeader 리더도 포함할지 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Team Communication")
    void BroadcastMessageToTeam(int32 TeamID, const FString& Message, bool bIncludeLeader = true);

    /**
     * 팀의 평균 레벨을 계산합니다
     * @param TeamID 대상 팀 ID
     * @return 평균 레벨
     */
    UFUNCTION(BlueprintPure, Category = "Team Statistics")
    float GetTeamAverageLevel(int32 TeamID) const;

    /**
     * 팀의 총 체력을 계산합니다
     * @param TeamID 대상 팀 ID
     * @return 총 체력
     */
    UFUNCTION(BlueprintPure, Category = "Team Statistics")
    float GetTeamTotalHealth(int32 TeamID) const;

    /**
     * 팀원들의 위치 중심점을 계산합니다
     * @param TeamID 대상 팀 ID
     * @return 중심점 좌표
     */
    UFUNCTION(BlueprintPure, Category = "Team Statistics")
    FVector GetTeamCenterLocation(int32 TeamID) const;

    // === 최적화 및 유지보수 기능 ===

    /**
     * 유효하지 않은 데이터를 정리합니다 (가비지 컬렉션 대응)
     */
    UFUNCTION(BlueprintCallable, Category = "Team Maintenance", CallInEditor)
    void CleanupInvalidData();

    /**
     * 모든 팀을 강제로 해체합니다 (디버그용)
     */
    UFUNCTION(BlueprintCallable, Category = "Team Maintenance", CallInEditor)
    void DisbandAllTeams();

    /**
     * 팀 관리자 상태를 로그로 출력합니다 (디버그용)
     */
    UFUNCTION(BlueprintCallable, Category = "Team Debug", CallInEditor)
    void LogTeamManagerStatus() const;

    /**
     * 특정 팀의 상세 정보를 로그로 출력합니다 (디버그용)
     * @param TeamID 조회할 팀 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Team Debug", CallInEditor)
    void LogTeamDetails(int32 TeamID) const;

protected:
    // === 내부 헬퍼 함수들 ===

    /**
     * 새로운 팀 ID를 생성합니다 (스레드 안전)
     * @return 고유한 팀 ID
     */
    int32 GenerateNewTeamID();

    /**
     * 팀 정보 유효성을 검사합니다
     * @param TeamInfo 검사할 팀 정보
     * @return 유효성 여부
     */
    bool ValidateTeamInfo(const FHSTeamInfo& TeamInfo) const;

    /**
     * 플레이어-팀 매핑 캐시를 업데이트합니다
     * @param PlayerState 업데이트할 플레이어
     * @param NewTeamID 새로운 팀 ID (-1이면 제거)
     */
    void UpdatePlayerTeamMapping(APlayerState* PlayerState, int32 NewTeamID = -1);

    /**
     * 정기적인 정리 작업을 수행합니다
     */
    UFUNCTION()
    void PerformScheduledCleanup();

    /**
     * 네트워크 복제용 함수들
     */
    UFUNCTION()
    void OnRep_TeamDatabase();

    /**
     * TeamID로 팀 정보를 찾는 헬퍼 함수
     * @param TeamID 찾을 팀 ID
     * @return 찾은 팀 정보의 포인터, 없으면 nullptr
     */
    FHSTeamInfo* FindTeamByID(int32 TeamID);
    const FHSTeamInfo* FindTeamByID(int32 TeamID) const;

    /**
     * TeamID로 팀 배열 인덱스를 찾는 헬퍼 함수
     * @param TeamID 찾을 팀 ID
     * @return 배열 인덱스, 없으면 INDEX_NONE
     */
    int32 FindTeamIndexByID(int32 TeamID) const;

private:
    // 내부 상태 변수들
    bool bIsInitialized;
    mutable FCriticalSection TeamDatabaseMutex; // 스레드 안전성을 위한 뮤텍스
};
