// HuntingSpirit Game - Character Base Class Implementation

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

// 생성자
AHSCharacterBase::AHSCharacterBase()
{
    // Tick 활성화
    PrimaryActorTick.bCanEverTick = true;

    // 캡슐 컴포넌트 기본 크기 설정
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    // 전투 컴포넌트 생성
    CombatComponent = CreateDefaultSubobject<UHSCombatComponent>(TEXT("CombatComponent"));

    // 피격 반응 컴포넌트 생성
    HitReactionComponent = CreateDefaultSubobject<UHSHitReactionComponent>(TEXT("HitReactionComponent"));
    
    // 스탯 컴포넌트 생성
    StatsComponent = CreateDefaultSubobject<UHSStatsComponent>(TEXT("StatsComponent"));
    
    // 인벤토리 컴포넌트 생성
    InventoryComponent = CreateDefaultSubobject<UHSInventoryComponent>(TEXT("InventoryComponent"));
    
    // 채집 컴포넌트 생성
    GatheringComponent = CreateDefaultSubobject<UHSGatheringComponent>(TEXT("GatheringComponent"));

    // 이동 관련 설정
    WalkSpeed = 400.0f;
    RunSpeed = 600.0f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    
    // 컨트롤러 회전에 따른 캐릭터 회전 비활성화
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // 전투 관련 기본 설정
    BasicAttackDuration = 1.0f;
    
    // 스태미너 관련 설정
    StaminaMax = 100.0f;
    StaminaCurrent = StaminaMax;
    StaminaRegenRate = 10.0f;  // 초당 10 스태미너 회복
    SprintStaminaConsumptionRate = 15.0f;  // 초당 15 스태미너 소모
    StaminaRegenDelay = 1.0f;  // 스태미너 소모 후 1초 뒤 회복 시작
    
    // 달리기 기본값 설정
    bSprintEnabled = false;
    
    // 초기 상태는 Idle로 설정
    CurrentState = ECharacterState::Idle;
}

// 게임 시작 또는 스폰 시 호출
void AHSCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 기본 이동 속도 설정
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

// 매 프레임 호출
void AHSCharacterBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 스태미너 업데이트
    UpdateStamina(DeltaTime);

    // 이동 상태 업데이트 확인
    if (CurrentState != ECharacterState::Dead && CurrentState != ECharacterState::Attacking)
    {
        if (GetVelocity().Size() > 0.0f)
        {
            // 달리기가 활성화되어 있고 충분한 스태미너가 있으면 달리기 상태 유지
            if (bSprintEnabled && StaminaCurrent > 0.0f)
            {
                if (CurrentState != ECharacterState::Running)
                {
                    SetCharacterState(ECharacterState::Running);
                }
            }
            else
            {
                // 달리기가 비활성화되었거나 스태미너가 부족하면 걷기 상태로 전환
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

// 입력 바인딩 설정
void AHSCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// 캐릭터 상태 설정
void AHSCharacterBase::SetCharacterState(ECharacterState NewState)
{
    if (CurrentState != NewState)
    {
        CurrentState = NewState;
        
        // 블루프린트에서 구현할 수 있도록 이벤트 호출
        OnCharacterStateChanged(NewState);
        
        // 상태에 따른 추가 로직 처리
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
                // 공격 중에는 이동 속도 감소
                GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * 0.5f;
                break;
                
            case ECharacterState::Dead:
                // 사망 시 이동 및 입력 비활성화
                GetCharacterMovement()->DisableMovement();
                DisableInput(Cast<APlayerController>(GetController()));
                break;
        }
    }
}

// 걷기 속도 설정
void AHSCharacterBase::SetWalkSpeed(float NewSpeed)
{
    WalkSpeed = FMath::Max(0.0f, NewSpeed);
    
    if (CurrentState == ECharacterState::Walking)
    {
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    }
}

// 달리기 속도 설정
void AHSCharacterBase::SetRunSpeed(float NewSpeed)
{
    RunSpeed = FMath::Max(0.0f, NewSpeed);
    
    if (CurrentState == ECharacterState::Running)
    {
        GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
    }
}

// 기본 공격 수행
void AHSCharacterBase::PerformBasicAttack()
{
    // 이미 공격 중이거나 사망 상태면 공격 불가
    if (CurrentState == ECharacterState::Attacking || CurrentState == ECharacterState::Dead)
    {
        return;
    }
    
    // 공격 상태로 변경
    SetCharacterState(ECharacterState::Attacking);
    
    // 공격 애니메이션 재생 (애니메이션 몽타주가 설정되어 있을 경우)
    if (BasicAttackMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(BasicAttackMontage, 1.0f);
            
            // 몽타주 완료 시 델리게이트 설정
            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &AHSCharacterBase::OnAttackEnd);
            AnimInstance->Montage_SetEndDelegate(EndDelegate, BasicAttackMontage);
        }
    }
    else
    {
        // 공격 종료 타이머 설정 (애니메이션 몽타주가 없을 경우)
        GetWorldTimerManager().SetTimer(AttackCooldownTimerHandle, this, &AHSCharacterBase::OnAttackEndTimer, BasicAttackDuration, false);
    }
}

// 공격 종료 타이머용 함수 (파라미터 없음)
void AHSCharacterBase::OnAttackEndTimer()
{
    // OnAttackEnd 함수 호출 - 기본 파라미터 사용
    OnAttackEnd(nullptr, false);
}

// 공격 종료 후 호출되는 함수 (애니메이션 몽타주 델리게이트에서 호출 가능)
void AHSCharacterBase::OnAttackEnd(UAnimMontage* Montage, bool bInterrupted)
{
    // 현재 상태가 여전히 공격 중이라면 Idle 상태로 변경
    if (CurrentState == ECharacterState::Attacking)
    {
        SetCharacterState(ECharacterState::Idle);
    }
    
    // 타이머 핸들 초기화
    GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);
}

// 스태미너 사용
bool AHSCharacterBase::UseStamina(float Amount)
{
    // 충분한 스태미너가 있는지 확인
    if (StaminaCurrent >= Amount)
    {
        // 스태미너 차감
        StaminaCurrent = FMath::Max(0.0f, StaminaCurrent - Amount);
        
        // 스태미너 변경 이벤트 호출
        OnStaminaChanged(StaminaCurrent, StaminaMax);
        
        // 스태미너를 사용했으므로 회복 지연 타이머 설정
        GetWorldTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
        GetWorldTimerManager().SetTimer(StaminaRegenDelayTimerHandle, this, &AHSCharacterBase::StartStaminaRegeneration, StaminaRegenDelay, false);
        
        return true;
    }
    
    return false;
}

// 스태미너 회복
void AHSCharacterBase::RestoreStamina(float Amount)
{
    // 스태미너 회복
    StaminaCurrent = FMath::Min(StaminaMax, StaminaCurrent + Amount);
    
    // 스태미너 변경 이벤트 호출
    OnStaminaChanged(StaminaCurrent, StaminaMax);
}

// 달리기 토글
void AHSCharacterBase::ToggleSprint()
{
    // 공격 중이거나 사망 상태면 달리기 토글 불가
    if (CurrentState == ECharacterState::Attacking || CurrentState == ECharacterState::Dead)
    {
        return;
    }
    
    // 달리기 활성화 상태 토글
    bSprintEnabled = !bSprintEnabled;
    
    // 상태 업데이트
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

// 달리기 시작 함수
void AHSCharacterBase::StartSprinting()
{
    // 캐릭터가 공격 또는 사망 상태면 달리기 무시
    if (CurrentState == ECharacterState::Attacking || CurrentState == ECharacterState::Dead)
        return;

    // 달리기 상태로 변경
    SetCharacterState(ECharacterState::Running);
}

// 달리기 종료 함수
void AHSCharacterBase::StopSprinting()
{
    // 캐릭터가 달리기 상태이고 이동 중이면 걷기 상태로 변경
    if (CurrentState == ECharacterState::Running && GetVelocity().Size() > 0.0f)
    {
        SetCharacterState(ECharacterState::Walking);
    }
}

// 스태미너 회복 시작
void AHSCharacterBase::StartStaminaRegeneration()
{
    // 타이머 초기화
    GetWorldTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
}

// 스태미너 업데이트
void AHSCharacterBase::UpdateStamina(float DeltaTime)
{
    // 달리기 중이고 이동 중이면 스태미너 소모
    if (CurrentState == ECharacterState::Running && GetVelocity().Size() > 0.0f)
    {
        float StaminaToUse = SprintStaminaConsumptionRate * DeltaTime;
        
        // 스태미너가 부족하면 달리기 비활성화
        if (!UseStamina(StaminaToUse))
        {
            bSprintEnabled = false;
            SetCharacterState(ECharacterState::Walking);
        }
    }
    // 달리기 중이 아니거나 회복 지연 타이머가 끝났으면 스태미너 회복
    else if (!GetWorldTimerManager().IsTimerActive(StaminaRegenDelayTimerHandle) && StaminaCurrent < StaminaMax)
    {
        RestoreStamina(StaminaRegenRate * DeltaTime);
    }
}

// 체력 설정
void AHSCharacterBase::SetHealth(float NewHealth)
{
    // 체력을 범위 내로 제한
    Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
    
    // 체력 변경 이벤트 호출
    OnHealthChanged.Broadcast(Health, MaxHealth);
    
    // 체력이 0이면 사망 상태로 변경
    if (Health <= 0.0f && CurrentState != ECharacterState::Dead)
    {
        SetCharacterState(ECharacterState::Dead);
    }
}
