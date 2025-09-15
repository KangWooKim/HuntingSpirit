/**
 * @file HSThiefCharacter.cpp
 * @brief 시프(도적) 캐릭터 클래스 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#include "HSThiefCharacter.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"

// 생성자
AHSThiefCharacter::AHSThiefCharacter()
{
    // 시프 클래스로 설정
    PlayerClass = EHSPlayerClass::Thief;
    
    // 시프 특화 설정 - 빠르고 민첩한 특성
    GetCharacterMovement()->MaxWalkSpeed = 450.0f;         // 빠른 걷기 속도
    GetCharacterMovement()->MaxAcceleration = 2000.0f;     // 높은 가속도 (민첩함)
    GetCharacterMovement()->RotationRate = FRotator(0, 720.0f, 0); // 빠른 회전 속도
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f; // 빠른 감속
    
    // 캡슐 컴포넌트 조정 (더 작은 히트박스)
    GetCapsuleComponent()->SetCapsuleSize(40.0f, 96.0f);
    
    // 시프 특화 스탯 설정
    SetupThiefStats();
    
    // 시프 스킬 초기화
    InitializeThiefSkills();
}

// 게임 시작 또는 스폰 시 호출
void AHSThiefCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 시프 특화 초기화 로직
}

// 특수 공격 (오버라이드) - 빠른 연속 공격
void AHSThiefCharacter::PerformBasicAttack()
{
    // 시프는 더 빠른 공격 속도
    BasicAttackDuration = 0.7f; // 전사보다 빠른 공격
    
    Super::PerformBasicAttack();
}

// 달리기 시작 함수 (시프 특화)
void AHSThiefCharacter::StartSprinting()
{
    Super::StartSprinting();
    
    // 시프는 더 빠른 질주 속도
    if (CurrentState == ECharacterState::Running)
    {
        GetCharacterMovement()->MaxWalkSpeed = RunSpeed * SprintSpeedMultiplier;
    }
}

// 달리기 종료 함수 (시프 특화)
void AHSThiefCharacter::StopSprinting()
{
    Super::StopSprinting();
    
    // 원래 속도로 복구
    if (CurrentState == ECharacterState::Walking)
    {
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    }
}

// === 시프 스킬 시스템 (QWER) ===

// Q 스킬 - 은신
void AHSThiefCharacter::UseSkillQ()
{
    if (CanUseSkill(EThiefSkillType::Stealth))
    {
        ExecuteStealth();
        StartSkillCooldown(EThiefSkillType::Stealth, StealthData.Cooldown);
    }
}

// W 스킬 - 질주
void AHSThiefCharacter::UseSkillW()
{
    if (CanUseSkill(EThiefSkillType::QuickDash))
    {
        ExecuteQuickDash();
        StartSkillCooldown(EThiefSkillType::QuickDash, QuickDashData.Cooldown);
    }
}

// E 스킬 - 회피
void AHSThiefCharacter::UseSkillE()
{
    if (CanUseSkill(EThiefSkillType::DodgeRoll))
    {
        ExecuteDodgeRoll();
        StartSkillCooldown(EThiefSkillType::DodgeRoll, DodgeRollData.Cooldown);
    }
}

// R 스킬 - 연속공격 (궁극기)
void AHSThiefCharacter::UseSkillR()
{
    if (CanUseSkill(EThiefSkillType::MultiStrike))
    {
        ExecuteMultiStrike();
        StartSkillCooldown(EThiefSkillType::MultiStrike, MultiStrikeData.Cooldown);
    }
}

// 스킬 사용 가능 여부 확인
bool AHSThiefCharacter::CanUseSkill(EThiefSkillType SkillType) const
{
    // 죽음 상태 또는 공격 중이면 스킬 사용 불가
    if (CurrentState == ECharacterState::Dead || CurrentState == ECharacterState::Attacking)
    {
        return false;
    }
    
    // 쿨다운 체크
    if (SkillCooldownTimers.Contains(SkillType))
    {
        const FTimerHandle& TimerHandle = SkillCooldownTimers[SkillType];
        if (GetWorldTimerManager().IsTimerActive(TimerHandle))
        {
            return false;
        }
    }
    
    // 스킬별 추가 조건 체크
    FThiefSkillData SkillData = GetSkillData(SkillType);
    
    // 스태미너 체크
    if (StaminaCurrent < SkillData.StaminaCost)
    {
        return false;
    }
    
    // 특정 스킬별 조건
    switch (SkillType)
    {
        case EThiefSkillType::Stealth:
            return !bIsStealthed; // 이미 은신 중이면 불가
            
        case EThiefSkillType::QuickDash:
            return !bIsQuickDashing; // 이미 질주 중이면 불가
            
        case EThiefSkillType::MultiStrike:
            return !bIsMultiStriking; // 이미 연속공격 중이면 불가
            
        default:
            return true;
    }
}

// 스킬 쿨다운 남은 시간 반환
float AHSThiefCharacter::GetSkillCooldownRemaining(EThiefSkillType SkillType) const
{
    if (SkillCooldownTimers.Contains(SkillType))
    {
        const FTimerHandle& TimerHandle = SkillCooldownTimers[SkillType];
        if (GetWorldTimerManager().IsTimerActive(TimerHandle))
        {
            return GetWorldTimerManager().GetTimerRemaining(TimerHandle);
        }
    }
    
    return 0.0f;
}

// 스킬 데이터 가져오기
FThiefSkillData AHSThiefCharacter::GetSkillData(EThiefSkillType SkillType) const
{
    switch (SkillType)
    {
        case EThiefSkillType::Stealth:
            return StealthData;
        case EThiefSkillType::QuickDash:
            return QuickDashData;
        case EThiefSkillType::DodgeRoll:
            return DodgeRollData;
        case EThiefSkillType::MultiStrike:
            return MultiStrikeData;
        default:
            return FThiefSkillData();
    }
}

// === 스킬 구현 내부 함수들 ===

// Q 스킬 실행 - 은신
void AHSThiefCharacter::ExecuteStealth()
{
    // 스태미너 소모
    UseStamina(StealthData.StaminaCost);
    
    // 은신 상태 활성화
    bIsStealthed = true;
    
    // 애니메이션 재생
    if (StealthData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(StealthData.SkillMontage, 1.0f);
        }
    }
    
    // 투명도 적용 (머티리얼 수정)
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        // 동적 머티리얼 인스턴스 생성하여 투명도 조절
        for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
        {
            UMaterialInterface* Material = MeshComp->GetMaterial(i);
            if (Material)
            {
                UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
                if (DynamicMaterial)
                {
                    DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), StealthOpacity);
                    MeshComp->SetMaterial(i, DynamicMaterial);
                }
            }
        }
    }
    
    // 은신 중에는 이동 속도 증가
    GetCharacterMovement()->MaxWalkSpeed *= 1.3f;
    
    // 은신 지속 시간 설정
    GetWorldTimerManager().SetTimer(StealthTimerHandle, this, &AHSThiefCharacter::EndStealth, StealthData.Duration, false);
}

// W 스킬 실행 - 질주
void AHSThiefCharacter::ExecuteQuickDash()
{
    // 스태미너 소모
    UseStamina(QuickDashData.StaminaCost);
    
    // 질주 상태 활성화
    bIsQuickDashing = true;
    
    // 애니메이션 재생
    if (QuickDashData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(QuickDashData.SkillMontage, 1.0f);
        }
    }
    
    // 앞으로 빠르게 이동하는 임펄스 적용
    FVector ForwardDirection = GetActorForwardVector();
    FVector DashImpulse = ForwardDirection * 1500.0f; // 질주 강도
    GetCharacterMovement()->AddImpulse(DashImpulse, true);
    
    // 질주 중에는 잠시 무적 상태 (TODO: CombatComponent 연동)
    
    // 질주 지속 시간 설정
    GetWorldTimerManager().SetTimer(QuickDashTimerHandle, this, &AHSThiefCharacter::EndQuickDash, QuickDashData.Duration, false);
}

// E 스킬 실행 - 회피
void AHSThiefCharacter::ExecuteDodgeRoll()
{
    // 스태미너 소모
    UseStamina(DodgeRollData.StaminaCost);
    
    // 애니메이션 재생
    if (DodgeRollData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(DodgeRollData.SkillMontage, 1.0f);
        }
    }
    
    // 회피 방향으로 이동 (현재 입력 방향 또는 뒤쪽)
    FVector DodgeDirection = GetActorForwardVector() * -1.0f; // 뒤로 회피
    FVector DodgeImpulse = DodgeDirection * 800.0f;
    GetCharacterMovement()->AddImpulse(DodgeImpulse, true);
    
    // 회피 중에는 잠시 무적 상태 (TODO: CombatComponent 연동)
    
    // 회피는 즉시 완료되므로 별도 타이머 불필요
}

// R 스킬 실행 - 연속공격 (궁극기)
void AHSThiefCharacter::ExecuteMultiStrike()
{
    // 스태미너 소모
    UseStamina(MultiStrikeData.StaminaCost);
    
    // 연속공격 상태 활성화
    bIsMultiStriking = true;
    MultiStrikeCombo = 0;
    
    // 첫 번째 공격 실행
    ExecuteNextMultiStrike();
}

// 연속공격 다음 타격 실행
void AHSThiefCharacter::ExecuteNextMultiStrike()
{
    MultiStrikeCombo++;
    
    // 공격 상태로 변경
    SetCharacterState(ECharacterState::Attacking);
    
    // 애니메이션 재생
    if (MultiStrikeData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(MultiStrikeData.SkillMontage, 1.5f); // 빠른 공격
        }
    }
    
    // TODO: 데미지 적용 로직
    
    // 연속공격이 5번 미만이면 다음 공격 예약
    if (MultiStrikeCombo < 5 && bIsMultiStriking)
    {
        GetWorldTimerManager().SetTimer(MultiStrikeComboTimerHandle, this, &AHSThiefCharacter::ExecuteNextMultiStrike, 0.3f, false);
    }
    else
    {
        // 연속공격 종료
        EndMultiStrike();
    }
}

// === 상태 종료 함수들 ===

// 은신 종료
void AHSThiefCharacter::EndStealth()
{
    bIsStealthed = false;
    
    // 투명도 복구
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
        {
            UMaterialInterface* Material = MeshComp->GetMaterial(i);
            if (Material)
            {
                UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
                if (DynamicMaterial)
                {
                    DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
                    MeshComp->SetMaterial(i, DynamicMaterial);
                }
            }
        }
    }
    
    // 이동 속도 복구
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

// 질주 종료
void AHSThiefCharacter::EndQuickDash()
{
    bIsQuickDashing = false;
}

// 연속공격 종료
void AHSThiefCharacter::EndMultiStrike()
{
    bIsMultiStriking = false;
    MultiStrikeCombo = 0;
    
    // 대기 상태로 복구
    if (CurrentState == ECharacterState::Attacking)
    {
        SetCharacterState(ECharacterState::Idle);
    }
    
    // 타이머 클리어
    GetWorldTimerManager().ClearTimer(MultiStrikeComboTimerHandle);
}

// === 유틸리티 함수들 ===

// 시프 특화 스탯 설정
void AHSThiefCharacter::SetupThiefStats()
{
    // 예시: 체력, 회피력, 공격속도 등 스탯 설정
    /*
    if (StatsComponent)
    {
        StatsComponent->SetMaxHealth(100.0f);    // 보통 체력
        StatsComponent->SetDefense(15.0f);       // 낮은 방어력
        StatsComponent->SetAttackPower(35.0f);   // 중간 공격력
        StatsComponent->SetAttackSpeed(1.3f);    // 빠른 공격 속도
        StatsComponent->SetCriticalChance(0.25f); // 높은 치명타 확률
    }
    */
}

// 스킬 초기화
void AHSThiefCharacter::InitializeThiefSkills()
{
    // Q 스킬 - 은신 설정
    StealthData.Cooldown = 15.0f;
    StealthData.Duration = 5.0f;
    StealthData.StaminaCost = 30.0f;
    StealthData.Damage = 0.0f;
    StealthData.Range = 0.0f;
    
    // W 스킬 - 질주 설정
    QuickDashData.Cooldown = 8.0f;
    QuickDashData.Duration = 0.5f;
    QuickDashData.StaminaCost = 25.0f;
    QuickDashData.Damage = 30.0f;
    QuickDashData.Range = 500.0f;
    
    // E 스킬 - 회피 설정
    DodgeRollData.Cooldown = 6.0f;
    DodgeRollData.Duration = 0.6f;
    DodgeRollData.StaminaCost = 20.0f;
    DodgeRollData.Damage = 0.0f;
    DodgeRollData.Range = 300.0f;
    
    // R 스킬 - 연속공격 설정 (궁극기)
    MultiStrikeData.Cooldown = 45.0f;
    MultiStrikeData.Duration = 2.0f;
    MultiStrikeData.StaminaCost = 60.0f;
    MultiStrikeData.Damage = 40.0f; // 공격당 데미지
    MultiStrikeData.Range = 150.0f;
}

// 스킬 쿨다운 시작
void AHSThiefCharacter::StartSkillCooldown(EThiefSkillType SkillType, float CooldownTime)
{
    // 기존 타이머가 있다면 클리어
    if (SkillCooldownTimers.Contains(SkillType))
    {
        GetWorldTimerManager().ClearTimer(SkillCooldownTimers[SkillType]);
    }
    
    // 새로운 쿨다운 타이머 설정
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, CooldownTime, false);
    SkillCooldownTimers.Add(SkillType, TimerHandle);
}
