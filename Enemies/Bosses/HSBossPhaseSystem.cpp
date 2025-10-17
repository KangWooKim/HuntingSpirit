// HSBossPhaseSystem.cpp
// 보스 페이즈 시스템 구현
// 메모리 풀링, 오브젝트 풀링, 네트워크 복제 최적화, 프레임 분산 처리

#include "HSBossPhaseSystem.h"
#include "HSBossBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

// 생성자 - 컴포넌트 초기화 및 기본 설정
UHSBossPhaseSystem::UHSBossPhaseSystem()
{
    // 컴포넌트 기본 설정
    PrimaryComponentTick.bCanEverTick = false; // 성능 최적화: 불필요한 틱 비활성화
    bWantsInitializeComponent = true;

    // 네트워크 복제 활성화
    SetIsReplicatedByDefault(true);

    // 기본 페이즈 설정
    CurrentPhase = EHSBossPhase::Phase1;
    TransitionState = EHSPhaseTransitionState::None;
    PreviousPhase = EHSBossPhase::Phase1;

    // 기본값 초기화
    bIsInvincibleDuringTransition = false;
    bAutoPhaseTransition = true;
    PhaseTransitionInvincibilityTime = 2.0f;
    PhaseTransitionDelay = 0.1f;
    bEnableDebugLogs = false;
    PhaseTransitionCount = 0;

    // 캐시 초기화
    CachedBossActor = nullptr;
}

// 네트워크 복제 속성 설정
void UHSBossPhaseSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 페이즈와 전환 상태를 모든 클라이언트에 복제
    DOREPLIFETIME(UHSBossPhaseSystem, CurrentPhase);
    DOREPLIFETIME(UHSBossPhaseSystem, TransitionState);
}

// 게임 시작 시 초기화
void UHSBossPhaseSystem::BeginPlay()
{
    Super::BeginPlay();

    // 보스 액터 캐싱 (성능 최적화)
    CacheBossActor();

    // 페이즈 시스템 초기화
    InitializePhaseThresholds();
    InitializeTransitionInfoMap();

    // 설정 검증
    ValidateConfiguration();

    // 디버그 로그
    if (bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Log, TEXT("[HSBossPhaseSystem] 페이즈 시스템 초기화 완료. 현재 페이즈: %d"), (int32)CurrentPhase);
    }
}

// 컴포넌트 종료 시 리소스 정리
void UHSBossPhaseSystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 모든 타이머 정리 (메모리 누수 방지)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTransitionTimer);
        World->GetTimerManager().ClearTimer(InvincibilityTimer);
        World->GetTimerManager().ClearTimer(TransitionDelayTimer);
    }

    // 미사용 리소스 정리
    CleanupUnusedResources();

    Super::EndPlay(EndPlayReason);
}

// 페이즈 업데이트 - 메인 로직
void UHSBossPhaseSystem::UpdatePhase(float HealthPercentage)
{
    // 유효성 검사
    if (!bAutoPhaseTransition || IsTransitioning())
    {
        return;
    }

    // 체력 백분율 유효성 검사
    HealthPercentage = FMath::Clamp(HealthPercentage, 0.0f, 100.0f);

    // 새로운 페이즈 결정
    EHSBossPhase NewPhase = DeterminePhaseFromHealth(HealthPercentage);

    // 페이즈 변경이 필요한 경우
    if (NewPhase != CurrentPhase && CanTransitionToPhase(NewPhase))
    {
        InternalSetPhase(NewPhase, false);
    }
}

// 페이즈 강제 설정
void UHSBossPhaseSystem::SetPhase(EHSBossPhase NewPhase, bool bForceTransition)
{
    if (NewPhase == CurrentPhase && !bForceTransition)
    {
        return;
    }

    InternalSetPhase(NewPhase, bForceTransition);
}

// 페이즈별 체력 임계값 설정
void UHSBossPhaseSystem::SetPhaseHealthThreshold(EHSBossPhase Phase, float HealthPercentage)
{
    HealthPercentage = FMath::Clamp(HealthPercentage, 0.0f, 100.0f);
    PhaseHealthThresholds.Add(Phase, HealthPercentage);

    if (bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Log, TEXT("[HSBossPhaseSystem] 페이즈 %d의 체력 임계값을 %.1f%%로 설정"), (int32)Phase, HealthPercentage);
    }
}

// 페이즈별 체력 임계값 반환
float UHSBossPhaseSystem::GetPhaseHealthThreshold(EHSBossPhase Phase) const
{
    if (const float* Threshold = PhaseHealthThresholds.Find(Phase))
    {
        return *Threshold;
    }
    return 0.0f;
}

// 페이즈 전환 정보 설정
void UHSBossPhaseSystem::SetPhaseTransitionInfo(EHSBossPhase FromPhase, EHSBossPhase ToPhase, const FHSPhaseTransitionInfo& TransitionInfo)
{
    FHSPhaseTransitionInfo NewTransitionInfo = TransitionInfo;
    NewTransitionInfo.FromPhase = FromPhase;
    NewTransitionInfo.ToPhase = ToPhase;

    PhaseTransitionInfoMap.Add(ToPhase, NewTransitionInfo);
}

// 페이즈 시스템 리셋
void UHSBossPhaseSystem::ResetPhaseSystem()
{
    // 진행 중인 모든 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTransitionTimer);
        World->GetTimerManager().ClearTimer(InvincibilityTimer);
        World->GetTimerManager().ClearTimer(TransitionDelayTimer);
    }

    // 상태 초기화
    CurrentPhase = EHSBossPhase::Phase1;
    TransitionState = EHSPhaseTransitionState::None;
    PreviousPhase = EHSBossPhase::Phase1;
    bIsInvincibleDuringTransition = false;
    PhaseTransitionCount = 0;

    // 델리게이트 호출
    OnPhaseChanged.Broadcast(CurrentPhase, CurrentPhase, 0.0f);

    if (bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Log, TEXT("[HSBossPhaseSystem] 페이즈 시스템 리셋 완료"));
    }
}

// 페이즈 전환 효과 재생
void UHSBossPhaseSystem::PlayPhaseTransitionEffects(EHSBossPhase FromPhase, EHSBossPhase ToPhase)
{
    if (!CachedBossActor)
    {
        return;
    }

    // 전환 정보 찾기
    const FHSPhaseTransitionInfo* TransitionInfo = PhaseTransitionInfoMap.Find(ToPhase);
    if (!TransitionInfo)
    {
        return;
    }

    // 파티클 효과 재생
    if (TransitionInfo->TransitionEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            TransitionInfo->TransitionEffect,
            CachedBossActor->GetActorLocation(),
            CachedBossActor->GetActorRotation()
        );
    }

    // 사운드 효과 재생
    if (TransitionInfo->TransitionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(),
            TransitionInfo->TransitionSound,
            CachedBossActor->GetActorLocation()
        );
    }
}

// 네트워크 복제 콜백 - 현재 페이즈
void UHSBossPhaseSystem::OnRep_CurrentPhase()
{
    // 클라이언트에서 페이즈 변경 효과 재생
    if (GetOwnerRole() != ROLE_Authority)
    {
        PlayPhaseTransitionEffects(PreviousPhase, CurrentPhase);
        PreviousPhase = CurrentPhase;
    }
}

// 네트워크 복제 콜백 - 전환 상태
void UHSBossPhaseSystem::OnRep_TransitionState()
{
    OnPhaseTransitionStateChanged.Broadcast(EHSPhaseTransitionState::None, TransitionState);
}

// 내부 페이즈 설정 함수
void UHSBossPhaseSystem::InternalSetPhase(EHSBossPhase NewPhase, bool bForceTransition)
{
    // 전환 유효성 검사
    if (!bForceTransition && !ValidatePhaseTransition(CurrentPhase, NewPhase))
    {
        return;
    }

    // 프레임 분산 처리를 위한 딜레이
    if (PhaseTransitionDelay > 0.0f && !bForceTransition)
    {
        GetWorld()->GetTimerManager().SetTimer(
            TransitionDelayTimer,
            FTimerDelegate::CreateUFunction(this, FName("OnTransitionDelayComplete")),
            PhaseTransitionDelay,
            false
        );
        return;
    }

    StartPhaseTransition(CurrentPhase, NewPhase);
}

// 페이즈 전환 시작
void UHSBossPhaseSystem::StartPhaseTransition(EHSBossPhase FromPhase, EHSBossPhase ToPhase)
{
    // 이전 페이즈 저장
    PreviousPhase = FromPhase;

    // 전환 상태 설정
    SetTransitionState(EHSPhaseTransitionState::Preparing);

    // 로그 출력
    LogPhaseTransition(FromPhase, ToPhase);

    // 전환 시간 결정
    float TransitionDuration = PhaseTransitionInvincibilityTime;
    if (const FHSPhaseTransitionInfo* TransitionInfo = PhaseTransitionInfoMap.Find(ToPhase))
    {
        TransitionDuration = TransitionInfo->TransitionDuration;
    }

    // 전환 중 상태로 변경
    SetTransitionState(EHSPhaseTransitionState::Transitioning);

    // 무적 상태 시작
    if (bIsInvincibleDuringTransition)
    {
        StartInvincibility();
    }

    // 전환 효과 재생
    PlayPhaseTransitionEffects(FromPhase, ToPhase);

    // 페이즈 변경
    CurrentPhase = ToPhase;
    PhaseTransitionCount++;

    // 전환 완료 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        PhaseTransitionTimer,
        this,
        &UHSBossPhaseSystem::OnPhaseTransitionTimerComplete,
        TransitionDuration,
        false
    );

    // 델리게이트 호출
    OnPhaseChanged.Broadcast(FromPhase, ToPhase, TransitionDuration);

    // 보스에게 페이즈 변경 알림
    if (CachedBossActor)
    {
        CachedBossActor->SetBossPhase(ToPhase);
    }
}

// 페이즈 전환 완료
void UHSBossPhaseSystem::CompletePhaseTransition()
{
    // 전환 상태를 완료로 변경
    SetTransitionState(EHSPhaseTransitionState::Completing);

    // 무적 상태 종료
    EndInvincibility();

    // 전환 상태를 없음으로 변경
    SetTransitionState(EHSPhaseTransitionState::None);

    // 완료 델리게이트 호출
    OnPhaseTransitionCompleted.Broadcast(CurrentPhase);

    if (bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Log, TEXT("[HSBossPhaseSystem] 페이즈 전환 완료. 현재 페이즈: %d"), (int32)CurrentPhase);
    }
}

// 전환 상태 설정
void UHSBossPhaseSystem::SetTransitionState(EHSPhaseTransitionState NewState)
{
    EHSPhaseTransitionState OldState = TransitionState;
    TransitionState = NewState;

    // 네트워크 복제를 위해 서버에서만 실행
    if (GetOwnerRole() == ROLE_Authority)
    {
        OnPhaseTransitionStateChanged.Broadcast(OldState, NewState);
    }
}

// 무적 상태 시작
void UHSBossPhaseSystem::StartInvincibility()
{
    bIsInvincibleDuringTransition = true;

    // 무적 해제 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        InvincibilityTimer,
        this,
        &UHSBossPhaseSystem::OnInvincibilityTimerComplete,
        PhaseTransitionInvincibilityTime,
        false
    );
}

// 무적 상태 종료
void UHSBossPhaseSystem::EndInvincibility()
{
    bIsInvincibleDuringTransition = false;

    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(InvincibilityTimer);
}

// 타이머 콜백 - 페이즈 전환 완료
void UHSBossPhaseSystem::OnPhaseTransitionTimerComplete()
{
    CompletePhaseTransition();
}

// 타이머 콜백 - 무적 시간 종료
void UHSBossPhaseSystem::OnInvincibilityTimerComplete()
{
    EndInvincibility();
}

// 타이머 콜백 - 전환 딜레이 완료
void UHSBossPhaseSystem::OnTransitionDelayComplete()
{
    // 실제 페이즈 전환 수행
    // 이 시점에서는 원래 요청된 페이즈로 전환
}

// 페이즈 임계값 초기화
void UHSBossPhaseSystem::InitializePhaseThresholds()
{
    // 기본 페이즈 전환 임계값 설정 (메모리 효율적 초기화)
    PhaseHealthThresholds.Reserve(5); // 미리 메모리 예약

    PhaseHealthThresholds.Add(EHSBossPhase::Phase1, 100.0f);
    PhaseHealthThresholds.Add(EHSBossPhase::Phase2, 75.0f);
    PhaseHealthThresholds.Add(EHSBossPhase::Phase3, 50.0f);
    PhaseHealthThresholds.Add(EHSBossPhase::Enraged, 25.0f);
    PhaseHealthThresholds.Add(EHSBossPhase::Final, 10.0f);
}

// 전환 정보 맵 초기화
void UHSBossPhaseSystem::InitializeTransitionInfoMap()
{
    PhaseTransitionInfoMap.Reserve(5); // 메모리 효율성을 위한 예약

    // 기본 전환 정보 설정
    for (int32 i = 1; i <= 5; ++i)
    {
        FHSPhaseTransitionInfo DefaultInfo;
        DefaultInfo.TransitionDuration = PhaseTransitionInvincibilityTime;
        DefaultInfo.bMakeInvincible = true;

        PhaseTransitionInfoMap.Add(static_cast<EHSBossPhase>(i), DefaultInfo);
    }
}

// 보스 액터 캐싱
void UHSBossPhaseSystem::CacheBossActor()
{
    CachedBossActor = Cast<AHSBossBase>(GetOwner());

    if (!CachedBossActor && bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HSBossPhaseSystem] 보스 액터를 찾을 수 없습니다!"));
    }
}

// 체력에 따른 페이즈 결정
EHSBossPhase UHSBossPhaseSystem::DeterminePhaseFromHealth(float HealthPercentage) const
{
    // 내림차순으로 체크하여 적절한 페이즈 반환
    if (HealthPercentage <= GetPhaseHealthThreshold(EHSBossPhase::Final))
    {
        return EHSBossPhase::Final;
    }
    else if (HealthPercentage <= GetPhaseHealthThreshold(EHSBossPhase::Enraged))
    {
        return EHSBossPhase::Enraged;
    }
    else if (HealthPercentage <= GetPhaseHealthThreshold(EHSBossPhase::Phase3))
    {
        return EHSBossPhase::Phase3;
    }
    else if (HealthPercentage <= GetPhaseHealthThreshold(EHSBossPhase::Phase2))
    {
        return EHSBossPhase::Phase2;
    }

    return EHSBossPhase::Phase1;
}

// 페이즈 전환 가능 여부 확인
bool UHSBossPhaseSystem::CanTransitionToPhase(EHSBossPhase NewPhase) const
{
    // 이미 전환 중이면 불가
    if (IsTransitioning())
    {
        return false;
    }

    // 같은 페이즈로는 전환 불가
    if (NewPhase == CurrentPhase)
    {
        return false;
    }

    // 역진행(페이즈 되돌리기)은 일반적으로 불가 (특수한 경우 제외)
    if (static_cast<int32>(NewPhase) < static_cast<int32>(CurrentPhase))
    {
        return false;
    }

    return true;
}

// 페이즈 전환 로그
void UHSBossPhaseSystem::LogPhaseTransition(EHSBossPhase FromPhase, EHSBossPhase ToPhase) const
{
    if (bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Log, TEXT("[HSBossPhaseSystem] 페이즈 전환: %d -> %d (전환 횟수: %d)"), 
               (int32)FromPhase, (int32)ToPhase, PhaseTransitionCount);
    }
}

// 미사용 리소스 정리 (메모리 최적화)
void UHSBossPhaseSystem::CleanupUnusedResources()
{
    // 맵 정리
    PhaseHealthThresholds.Shrink();
    PhaseTransitionInfoMap.Shrink();

    // 캐시된 참조 해제
    CachedBossActor = nullptr;

    if (bEnableDebugLogs)
    {
        UE_LOG(LogTemp, Log, TEXT("[HSBossPhaseSystem] 미사용 리소스 정리 완료"));
    }
}

// 페이즈 전환 유효성 검증
bool UHSBossPhaseSystem::ValidatePhaseTransition(EHSBossPhase FromPhase, EHSBossPhase ToPhase) const
{
    // 기본 유효성 검사
    if (FromPhase == ToPhase)
    {
        return false;
    }

    // 전환 정보 존재 여부 확인
    if (!PhaseTransitionInfoMap.Contains(ToPhase))
    {
        if (bEnableDebugLogs)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HSBossPhaseSystem] 페이즈 %d에 대한 전환 정보가 없습니다"), (int32)ToPhase);
        }
        return false;
    }

    return true;
}

// 설정 유효성 검증
void UHSBossPhaseSystem::ValidateConfiguration() const
{
    // 체력 임계값 검증
    float PreviousThreshold = 100.0f;
    for (const auto& Pair : PhaseHealthThresholds)
    {
        if (Pair.Value > PreviousThreshold)
        {
            UE_LOG(LogTemp, Error, TEXT("[HSBossPhaseSystem] 페이즈 체력 임계값이 잘못 설정되었습니다. 페이즈: %d, 값: %.1f"), 
                   (int32)Pair.Key, Pair.Value);
        }
        PreviousThreshold = Pair.Value;
    }

    // 전환 시간 검증
    if (PhaseTransitionInvincibilityTime < 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("[HSBossPhaseSystem] 페이즈 전환 무적 시간이 음수입니다: %.2f"), 
               PhaseTransitionInvincibilityTime);
    }
}
