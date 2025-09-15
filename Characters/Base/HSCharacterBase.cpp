/**
 * @file HSCharacterBase.cpp
 * @brief 모든 캐릭터(플레이어 및 적)의 기본 클래스 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#include "HSCharacterBase.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Combat/HSHitReactionComponent.h"
#include "HuntingSpirit/Characters/Stats/HSStatsComponent.h"
#include "HuntingSpirit/Gathering/Inventory/HSInventoryComponent.h"
#include "HuntingSpirit/Gathering/Harvesting/HSGatheringComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

/**
 * @brief 기본 생성자
 * @details 캐릭터의 모든 컴포넌트와 기본 설정을 초기화
 */
AHSCharacterBase::AHSCharacterBase()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    CombatComponent = CreateDefaultSubobject<UHSCombatComponent>(TEXT("CombatComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHSHitReactionComponent>(TEXT("HitReactionComponent"));
    StatsComponent = CreateDefaultSubobject<UHSStatsComponent>(TEXT("StatsComponent"));
    InventoryComponent = CreateDefaultSubobject<UHSInventoryComponent>(TEXT("InventoryComponent"));
    GatheringComponent = CreateDefaultSubobject<UHSGatheringComponent>(TEXT("GatheringComponent"));

    WalkSpeed = 400.0f;
    RunSpeed = 600.0f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    BasicAttackDuration = 1.0f;
    
    StaminaMax = 100.0f;
    StaminaCurrent = StaminaMax;
    StaminaRegenRate = 10.0f;
    SprintStaminaConsumptionRate = 15.0f;
    StaminaRegenDelay = 1.0f;
    
    bSprintEnabled = false;
    CurrentState = ECharacterState::Idle;
}

/**
 * @brief 게임 시작 시 초기화 작업을 수행
 */
void AHSCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

/**
 * @brief 매 프레임마다 캐릭터 상태를 업데이트
 * @param DeltaTime 이전 프레임과의 시간 차이
 */
void AHSCharacterBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateStamina(DeltaTime);

    if (CurrentState != ECharacterState::Dead && CurrentState != ECharacterState::Attacking)
    {
        if (GetVelocity().Size() > 0.0f)
        {
            if (bSprintEnabled && StaminaCurrent > 0.0f)
            {
                if (CurrentState != ECharacterState::Running)
                {
                    SetCharacterState(ECharacterState::Running);
                }
            }
            else
            {
                if (CurrentState != ECharacterState::Walking)
                {
                    SetCharacterState(ECharacterState::Walking);
                }
            }
        }
        else if (CurrentState != ECharacterState::Idle)
        {
            SetCharacterState(ECharacterState::Idle);
        }
    }
}

/**
 * @brief 플레이어 입력 바인딩을 설정
 * @param PlayerInputComponent 플레이어 입력 컴포넌트
 */
void AHSCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

/**
 * @brief 캐릭터 상태를 변경하고 관련 설정을 업데이트
 * @param NewState 새로운 캐릭터 상태
 */
void AHSCharacterBase::SetCharacterState(ECharacterState NewState)
{
    if (CurrentState != NewState)
    {
        CurrentState = NewState;
        
        OnCharacterStateChanged(NewState);
        
        switch (NewState)
        {
            case ECharacterState::Idle:
                break;
                
            case ECharacterState::Walking:
                GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
                break;
                
            case ECharacterState::Running:
                GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
                break;
                
            case ECharacterState::Attacking:
                GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * 0.5f;
                break;
                
            case ECharacterState::Dead:
                GetCharacterMovement()->DisableMovement();
                DisableInput(Cast<APlayerController>(GetController()));
                break;
        }
    }
}

/**
 * @brief 걷기 속도를 설정하고 현재 상태에 반영
 * @param NewSpeed 새로운 걷기 속도
 */
void AHSCharacterBase::SetWalkSpeed(float NewSpeed)
{
    WalkSpeed = FMath::Max(0.0f, NewSpeed);
    
    if (CurrentState == ECharacterState::Walking)
    {
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    }
}

/**
 * @brief 달리기 속도를 설정하고 현재 상태에 반영
 * @param NewSpeed 새로운 달리기 속도
 */
void AHSCharacterBase::SetRunSpeed(float NewSpeed)
{
    RunSpeed = FMath::Max(0.0f, NewSpeed);
    
    if (CurrentState == ECharacterState::Running)
    {
        GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
    }
}

/**
 * @brief 기본 공격을 실행하고 애니메이션을 재생
 */
void AHSCharacterBase::PerformBasicAttack()
{
    if (CurrentState == ECharacterState::Attacking || CurrentState == ECharacterState::Dead)
    {
        return;
    }
    
    SetCharacterState(ECharacterState::Attacking);
    
    if (BasicAttackMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(BasicAttackMontage, 1.0f);
            
            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &AHSCharacterBase::OnAttackEnd);
            AnimInstance->Montage_SetEndDelegate(EndDelegate, BasicAttackMontage);
        }
    }
    else
    {
        GetWorldTimerManager().SetTimer(AttackCooldownTimerHandle, this, &AHSCharacterBase::OnAttackEndTimer, BasicAttackDuration, false);
    }
}

/**
 * @brief 타이머용 공격 종료 함수 (파라미터 없는 버전)
 */
void AHSCharacterBase::OnAttackEndTimer()
{
    OnAttackEnd(nullptr, false);
}

/**
 * @brief 공격 종료 후 상태를 복원하는 콜백 함수
 * @param Montage 종료된 애니메이션 몽타주 (nullptr 허용)
 * @param bInterrupted 애니메이션이 중단되었는지 여부
 */
void AHSCharacterBase::OnAttackEnd(UAnimMontage* Montage, bool bInterrupted)
{
    if (CurrentState == ECharacterState::Attacking)
    {
        SetCharacterState(ECharacterState::Idle);
    }
    
    GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);
}

/**
 * @brief 스태미너를 소모하고 성공 여부를 반환
 * @param Amount 소모할 스태미너 양
 * @return 스태미너 소모 성공 여부
 */
bool AHSCharacterBase::UseStamina(float Amount)
{
    if (StaminaCurrent >= Amount)
    {
        StaminaCurrent = FMath::Max(0.0f, StaminaCurrent - Amount);
        OnStaminaChanged(StaminaCurrent, StaminaMax);
        
        GetWorldTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
        GetWorldTimerManager().SetTimer(StaminaRegenDelayTimerHandle, this, &AHSCharacterBase::StartStaminaRegeneration, StaminaRegenDelay, false);
        
        return true;
    }
    
    return false;
}

/**
 * @brief 스태미너를 회복하고 이벤트를 발생
 * @param Amount 회복할 스태미너 양
 */
void AHSCharacterBase::RestoreStamina(float Amount)
{
    StaminaCurrent = FMath::Min(StaminaMax, StaminaCurrent + Amount);
    OnStaminaChanged(StaminaCurrent, StaminaMax);
}

/**
 * @brief 달리기 상태를 토글하고 이동 상태를 업데이트
 */
void AHSCharacterBase::ToggleSprint()
{
    if (CurrentState == ECharacterState::Attacking || CurrentState == ECharacterState::Dead)
    {
        return;
    }
    
    bSprintEnabled = !bSprintEnabled;
    
    if (GetVelocity().Size() > 0.0f)
    {
        if (bSprintEnabled && StaminaCurrent > 0.0f)
        {
            SetCharacterState(ECharacterState::Running);
        }
        else
        {
            SetCharacterState(ECharacterState::Walking);
        }
    }
}

/**
 * @brief 달리기를 시작하고 상태를 변경
 */
void AHSCharacterBase::StartSprinting()
{
    if (CurrentState == ECharacterState::Attacking || CurrentState == ECharacterState::Dead)
        return;

    SetCharacterState(ECharacterState::Running);
}

/**
 * @brief 달리기를 종료하고 걷기 상태로 전환
 */
void AHSCharacterBase::StopSprinting()
{
    if (CurrentState == ECharacterState::Running && GetVelocity().Size() > 0.0f)
    {
        SetCharacterState(ECharacterState::Walking);
    }
}

/**
 * @brief 스태미너 재생 지연을 종료하고 회복을 시작
 */
void AHSCharacterBase::StartStaminaRegeneration()
{
    GetWorldTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
}

/**
 * @brief 매 프레임마다 스태미너 상태를 업데이트
 * @param DeltaTime 이전 프레임과의 시간 차이
 */
void AHSCharacterBase::UpdateStamina(float DeltaTime)
{
    if (CurrentState == ECharacterState::Running && GetVelocity().Size() > 0.0f)
    {
        float StaminaToUse = SprintStaminaConsumptionRate * DeltaTime;
        
        if (!UseStamina(StaminaToUse))
        {
            bSprintEnabled = false;
            SetCharacterState(ECharacterState::Walking);
        }
    }
    else if (!GetWorldTimerManager().IsTimerActive(StaminaRegenDelayTimerHandle) && StaminaCurrent < StaminaMax)
    {
        RestoreStamina(StaminaRegenRate * DeltaTime);
    }
}

/**
 * @brief 체력을 설정하고 사망 체크를 수행
 * @param NewHealth 새로운 체력 값
 */
void AHSCharacterBase::SetHealth(float NewHealth)
{
    Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(Health, MaxHealth);
    
    if (Health <= 0.0f && CurrentState != ECharacterState::Dead)
    {
        SetCharacterState(ECharacterState::Dead);
    }
}
