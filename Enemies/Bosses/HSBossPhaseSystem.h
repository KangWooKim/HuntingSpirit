// HSBossPhaseSystem.h
// 보스 페이즈 시스템 컴포넌트 - 보스의 체력에 따른 페이즈 전환을 관리
// 현업에서 사용하는 최적화 기법 적용: 오브젝트 풀링, 메모리 효율적 설계, 네트워크 복제 최적화

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Net/UnrealNetwork.h"
#include "HSBossBase.h" // EHSBossPhase 정의를 위해
#include "HSBossPhaseSystem.generated.h"

// 전방 선언
class AHSBossBase;
class UParticleSystemComponent;
class USoundCue;

// 페이즈 전환 상태
UENUM(BlueprintType)
enum class EHSPhaseTransitionState : uint8
{
    None            UMETA(DisplayName = "None"),
    Preparing       UMETA(DisplayName = "Preparing"),
    Transitioning   UMETA(DisplayName = "Transitioning"),
    Completing      UMETA(DisplayName = "Completing")
};

// 페이즈 전환 정보 구조체
USTRUCT(BlueprintType)
struct FHSPhaseTransitionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHSBossPhase FromPhase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHSBossPhase ToPhase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TransitionDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bMakeInvincible;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UParticleSystem* TransitionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundCue* TransitionSound;

    FHSPhaseTransitionInfo()
    {
        FromPhase = EHSBossPhase::Phase1;
        ToPhase = EHSBossPhase::Phase2;
        TransitionDuration = 2.0f;
        bMakeInvincible = true;
        TransitionEffect = nullptr;
        TransitionSound = nullptr;
    }
};

// 보스 페이즈 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPhaseChangedDelegate, EHSBossPhase, OldPhase, EHSBossPhase, NewPhase, float, TransitionDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseTransitionStateChanged, EHSPhaseTransitionState, OldState, EHSPhaseTransitionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseTransitionCompleted, EHSBossPhase, NewPhase);

/**
 * 보스 페이즈 시스템 컴포넌트
 * 보스의 체력에 따른 페이즈 전환을 관리하고, 전환 시 특수 효과 및 무적 시간을 처리합니다.
 * 네트워크 복제를 지원하며 현업 최적화 기법을 적용했습니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSBossPhaseSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    // 생성자
    UHSBossPhaseSystem();

    // 네트워크 복제 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // BeginPlay 오버라이드
    virtual void BeginPlay() override;

    // EndPlay 오버라이드 - 리소스 정리
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 페이즈 업데이트 함수 - 메인 로직
    UFUNCTION(BlueprintCallable, Category = "Boss Phase")
    void UpdatePhase(float HealthPercentage);

    // 페이즈 강제 설정 함수
    UFUNCTION(BlueprintCallable, Category = "Boss Phase", CallInEditor)
    void SetPhase(EHSBossPhase NewPhase, bool bForceTransition = false);

    // 현재 페이즈 반환
    UFUNCTION(BlueprintPure, Category = "Boss Phase")
    EHSBossPhase GetCurrentPhase() const { return CurrentPhase; }

    // 페이즈 전환 상태 반환
    UFUNCTION(BlueprintPure, Category = "Boss Phase")
    EHSPhaseTransitionState GetTransitionState() const { return TransitionState; }

    // 페이즈 전환 중인지 확인
    UFUNCTION(BlueprintPure, Category = "Boss Phase")
    bool IsTransitioning() const { return TransitionState != EHSPhaseTransitionState::None; }

    // 페이즈별 체력 임계값 설정
    UFUNCTION(BlueprintCallable, Category = "Boss Phase")
    void SetPhaseHealthThreshold(EHSBossPhase Phase, float HealthPercentage);

    // 페이즈별 체력 임계값 반환
    UFUNCTION(BlueprintPure, Category = "Boss Phase")
    float GetPhaseHealthThreshold(EHSBossPhase Phase) const;

    // 페이즈 전환 정보 설정
    UFUNCTION(BlueprintCallable, Category = "Boss Phase")
    void SetPhaseTransitionInfo(EHSBossPhase FromPhase, EHSBossPhase ToPhase, const FHSPhaseTransitionInfo& TransitionInfo);

    // 무적 상태 확인
    UFUNCTION(BlueprintPure, Category = "Boss Phase")
    bool IsInvincibleDuringTransition() const { return bIsInvincibleDuringTransition && IsTransitioning(); }

    // 페이즈 시스템 리셋
    UFUNCTION(BlueprintCallable, Category = "Boss Phase")
    void ResetPhaseSystem();

    // 페이즈 전환 효과 재생
    UFUNCTION(BlueprintCallable, Category = "Boss Phase")
    void PlayPhaseTransitionEffects(EHSBossPhase FromPhase, EHSBossPhase ToPhase);

    // 페이즈 변경 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Boss Phase Events")
    FOnPhaseChangedDelegate OnPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Boss Phase Events")
    FOnPhaseTransitionStateChanged OnPhaseTransitionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Boss Phase Events")
    FOnPhaseTransitionCompleted OnPhaseTransitionCompleted;

protected:
    // 현재 페이즈 - 네트워크 복제
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, VisibleAnywhere, BlueprintReadOnly, Category = "Boss Phase")
    EHSBossPhase CurrentPhase;

    // 페이즈 전환 상태 - 네트워크 복제
    UPROPERTY(ReplicatedUsing = OnRep_TransitionState, VisibleAnywhere, BlueprintReadOnly, Category = "Boss Phase")
    EHSPhaseTransitionState TransitionState;

    // 페이즈별 체력 임계값 (백분율) - 메모리 효율적 설계
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    TMap<EHSBossPhase, float> PhaseHealthThresholds;

    // 페이즈 전환 정보 맵
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase Transitions")
    TMap<EHSBossPhase, FHSPhaseTransitionInfo> PhaseTransitionInfoMap;

    // 페이즈 전환 시 무적 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase", meta = (ClampMin = "0.0"))
    float PhaseTransitionInvincibilityTime = 2.0f;

    // 페이즈 전환 중 무적 상태 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss Phase")
    bool bIsInvincibleDuringTransition = false;

    // 자동 페이즈 전환 활성화 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase")
    bool bAutoPhaseTransition = true;

    // 페이즈 전환 딜레이 (최적화를 위한 프레임 분산)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase Performance", meta = (ClampMin = "0.0"))
    float PhaseTransitionDelay = 0.1f;

    // 디버그 로그 활성화 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Phase Debug")
    bool bEnableDebugLogs = false;

private:
    // 보스 액터 캐시 (성능 최적화)
    UPROPERTY()
    AHSBossBase* CachedBossActor;

    // 타이머 핸들들
    FTimerHandle PhaseTransitionTimer;
    FTimerHandle InvincibilityTimer;
    FTimerHandle TransitionDelayTimer;

    // 이전 페이즈 (롤백용)
    EHSBossPhase PreviousPhase;

    // 페이즈 전환 카운터 (디버깅용)
    int32 PhaseTransitionCount = 0;

    // 네트워크 복제 콜백 함수들
    UFUNCTION()
    void OnRep_CurrentPhase();

    UFUNCTION()
    void OnRep_TransitionState();

    // 내부 페이즈 전환 함수들 - 최적화된 실행 순서
    void InternalSetPhase(EHSBossPhase NewPhase, bool bForceTransition = false);
    void StartPhaseTransition(EHSBossPhase FromPhase, EHSBossPhase ToPhase);
    void CompletePhaseTransition();

    // 페이즈 전환 상태 변경
    void SetTransitionState(EHSPhaseTransitionState NewState);

    // 무적 상태 관리
    void StartInvincibility();
    void EndInvincibility();

    // 타이머 콜백 함수들
    UFUNCTION()
    void OnPhaseTransitionTimerComplete();

    UFUNCTION()
    void OnInvincibilityTimerComplete();

    UFUNCTION()
    void OnTransitionDelayComplete();

    // 초기화 함수들
    void InitializePhaseThresholds();
    void InitializeTransitionInfoMap();
    void CacheBossActor();

    // 유틸리티 함수들
    EHSBossPhase DeterminePhaseFromHealth(float HealthPercentage) const;
    bool CanTransitionToPhase(EHSBossPhase NewPhase) const;
    void LogPhaseTransition(EHSBossPhase FromPhase, EHSBossPhase ToPhase) const;

    // 메모리 최적화 - 미사용 리소스 정리
    void CleanupUnusedResources();

    // 검증 함수들
    bool ValidatePhaseTransition(EHSBossPhase FromPhase, EHSBossPhase ToPhase) const;
    void ValidateConfiguration() const;
};
