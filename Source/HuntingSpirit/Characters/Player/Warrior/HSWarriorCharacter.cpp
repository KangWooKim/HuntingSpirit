// HuntingSpirit Game - Warrior Character Implementation

#include "HSWarriorCharacter.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 생성자
AHSWarriorCharacter::AHSWarriorCharacter()
{
    // 전사 클래스로 설정
    PlayerClass = EHSPlayerClass::Warrior;
    
    // 전사 특화 설정
    // 더 무겁고 강력한 느낌을 위해 이동 속도와 회전 속도 조정
    GetCharacterMovement()->MaxWalkSpeed = 375.0f;         // 약간 느린 걷기 속도
    GetCharacterMovement()->MaxAcceleration = 1500.0f;     // 높은 가속도 (무거운 느낌)
    GetCharacterMovement()->RotationRate = FRotator(0, 480.0f, 0); // 약간 느린 회전 속도
    
    // 캡슐 컴포넌트 조정 (더 큰 히트박스)
    GetCapsuleComponent()->SetCapsuleSize(44.0f, 96.0f);
    
    // 전사 특화 스탯 설정 (기본 캐릭터보다 높은 체력과 방어력)
    SetupWarriorStats();
    
    // 전사 스킬 초기화
    InitializeWarriorSkills();
}

// 게임 시작 또는 스폰 시 호출
void AHSWarriorCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 전사 특화 초기화 로직
}

// 특수 공격 (오버라이드)
void AHSWarriorCharacter::PerformBasicAttack()
{
    // 전사는 기본 공격 대신 무겁고 강력한 대검 공격 사용
    Super::PerformBasicAttack();
    
    // 추가 효과 (예: 카메라 흔들림, 파티클 효과 등)
    // 구현 가능
}

// === 전사 스킬 시스템 (QWER) ===

// Q 스킬 - 방어
void AHSWarriorCharacter::UseSkillQ()
{
    if (CanUseSkill(EWarriorSkillType::ShieldBlock))
    {
        ExecuteShieldBlock();
        StartSkillCooldown(EWarriorSkillType::ShieldBlock, ShieldBlockData.Cooldown);
    }
}

// W 스킬 - 돌진
void AHSWarriorCharacter::UseSkillW()
{
    if (CanUseSkill(EWarriorSkillType::Charge))
    {
        ExecuteCharge();
        StartSkillCooldown(EWarriorSkillType::Charge, ChargeData.Cooldown);
    }
}

// E 스킬 - 회전베기
void AHSWarriorCharacter::UseSkillE()
{
    if (CanUseSkill(EWarriorSkillType::Whirlwind))
    {
        ExecuteWhirlwind();
        StartSkillCooldown(EWarriorSkillType::Whirlwind, WhirlwindData.Cooldown);
    }
}

// R 스킬 - 광폭화 (궁극기)
void AHSWarriorCharacter::UseSkillR()
{
    if (CanUseSkill(EWarriorSkillType::BerserkerRage))
    {
        ExecuteBerserkerRage();
        StartSkillCooldown(EWarriorSkillType::BerserkerRage, BerserkerRageData.Cooldown);
    }
}

// 스킬 사용 가능 여부 확인
bool AHSWarriorCharacter::CanUseSkill(EWarriorSkillType SkillType) const
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
    FWarriorSkillData SkillData = GetSkillData(SkillType);
    
    // 스태미너 체크
    if (StaminaCurrent < SkillData.StaminaCost)
    {
        return false;
    }
    
    // 특정 스킬별 조건
    switch (SkillType)
    {
        case EWarriorSkillType::ShieldBlock:
            return !bIsBlocking; // 이미 방어 중이면 불가
            
        case EWarriorSkillType::Charge:
            return !bIsCharging; // 이미 돌진 중이면 불가
            
        case EWarriorSkillType::BerserkerRage:
            return !bIsBerserkerMode; // 이미 광폭화 중이면 불가
            
        default:
            return true;
    }
}

// 스킬 쿨다운 남은 시간 반환
float AHSWarriorCharacter::GetSkillCooldownRemaining(EWarriorSkillType SkillType) const
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
FWarriorSkillData AHSWarriorCharacter::GetSkillData(EWarriorSkillType SkillType) const
{
    switch (SkillType)
    {
        case EWarriorSkillType::ShieldBlock:
            return ShieldBlockData;
        case EWarriorSkillType::Charge:
            return ChargeData;
        case EWarriorSkillType::Whirlwind:
            return WhirlwindData;
        case EWarriorSkillType::BerserkerRage:
            return BerserkerRageData;
        default:
            return FWarriorSkillData();
    }
}

// === 스킬 구현 내부 함수들 ===

// Q 스킬 실행 - 방어
void AHSWarriorCharacter::ExecuteShieldBlock()
{
    // 스태미너 소모
    UseStamina(ShieldBlockData.StaminaCost);
    
    // 방어 상태 활성화
    bIsBlocking = true;
    
    // 애니메이션 재생
    if (ShieldBlockData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(ShieldBlockData.SkillMontage, 1.0f);
        }
    }
    
    // 방어 지속 시간 설정
    GetWorldTimerManager().SetTimer(BlockingTimerHandle, this, &AHSWarriorCharacter::EndBlocking, ShieldBlockData.Duration, false);
    
    // 방어 중에는 이동 속도 감소
    GetCharacterMovement()->MaxWalkSpeed *= 0.3f;
}

// W 스킬 실행 - 돌진
void AHSWarriorCharacter::ExecuteCharge()
{
    // 스태미너 소모
    UseStamina(ChargeData.StaminaCost);
    
    // 돌진 상태 활성화
    bIsCharging = true;
    
    // 애니메이션 재생
    if (ChargeData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(ChargeData.SkillMontage, 1.0f);
        }
    }
    
    // 앞으로 돌진하는 임펄스 적용
    FVector ForwardDirection = GetActorForwardVector();
    FVector ChargeImpulse = ForwardDirection * 1000.0f; // 돌진 강도
    GetCharacterMovement()->AddImpulse(ChargeImpulse, true);
    
    // 돌진 지속 시간 설정
    GetWorldTimerManager().SetTimer(ChargingTimerHandle, this, &AHSWarriorCharacter::EndCharging, ChargeData.Duration, false);
}

// E 스킬 실행 - 회전베기
void AHSWarriorCharacter::ExecuteWhirlwind()
{
    // 스태미너 소모
    UseStamina(WhirlwindData.StaminaCost);
    
    // 공격 상태로 변경
    SetCharacterState(ECharacterState::Attacking);
    
    // 애니메이션 재생
    if (WhirlwindData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(WhirlwindData.SkillMontage, 1.0f);
            
            // 몽타주 완료 시 호출될 델리게이트 설정
            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &AHSCharacterBase::OnAttackEnd);
            AnimInstance->Montage_SetEndDelegate(EndDelegate, WhirlwindData.SkillMontage);
        }
    }
    else
    {
        // 애니메이션이 없으면 타이머로 공격 상태 종료
        GetWorldTimerManager().SetTimer(AttackCooldownTimerHandle, this, &AHSCharacterBase::OnAttackEndTimer, WhirlwindData.Duration, false);
    }
    
    // 여기에 광역 데미지 로직 추가 가능
    // TODO: 주변 적들에게 데미지 적용
}

// R 스킬 실행 - 광폭화 (궁극기)
void AHSWarriorCharacter::ExecuteBerserkerRage()
{
    // 스태미너 소모
    UseStamina(BerserkerRageData.StaminaCost);
    
    // 광폭화 상태 활성화
    bIsBerserkerMode = true;
    
    // 애니메이션 재생
    if (BerserkerRageData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(BerserkerRageData.SkillMontage, 1.0f);
        }
    }
    
    // 광폭화 효과 적용
    GetCharacterMovement()->MaxWalkSpeed *= 1.5f;  // 이동 속도 증가
    // TODO: 공격력 증가, 스태미너 회복 증가 등 효과 추가
    
    // 광폭화 지속 시간 설정
    GetWorldTimerManager().SetTimer(BerserkerTimerHandle, this, &AHSWarriorCharacter::EndBerserkerMode, BerserkerRageData.Duration, false);
}

// === 상태 종료 함수들 ===

// 방어 종료
void AHSWarriorCharacter::EndBlocking()
{
    bIsBlocking = false;
    
    // 이동 속도 복구
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

// 돌진 종료
void AHSWarriorCharacter::EndCharging()
{
    bIsCharging = false;
}

// 광폭화 종료
void AHSWarriorCharacter::EndBerserkerMode()
{
    bIsBerserkerMode = false;
    
    // 이동 속도 복구
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    // TODO: 기타 광폭화 효과 제거
}

// === 유틸리티 함수들 ===

// 전사 특화 스탯 설정
void AHSWarriorCharacter::SetupWarriorStats()
{
    // 예시: 체력, 방어력, 공격력 등 스탯 설정
    // 스탯 시스템이 구현되면 활용할 수 있는 코드
    /*
    if (StatsComponent)
    {
        StatsComponent->SetMaxHealth(150.0f);    // 높은 최대 체력
        StatsComponent->SetDefense(30.0f);       // 높은 방어력
        StatsComponent->SetAttackPower(40.0f);   // 중간 공격력
        StatsComponent->SetAttackSpeed(0.8f);    // 느린 공격 속도
    }
    */
}

// 스킬 초기화
void AHSWarriorCharacter::InitializeWarriorSkills()
{
    // Q 스킬 - 방어 설정
    ShieldBlockData.Cooldown = 8.0f;
    ShieldBlockData.Duration = 3.0f;
    ShieldBlockData.StaminaCost = 25.0f;
    ShieldBlockData.Damage = 0.0f;
    ShieldBlockData.Range = 0.0f;
    
    // W 스킬 - 돌진 설정
    ChargeData.Cooldown = 10.0f;
    ChargeData.Duration = 1.5f;
    ChargeData.StaminaCost = 30.0f;
    ChargeData.Damage = 50.0f;
    ChargeData.Range = 400.0f;
    
    // E 스킬 - 회전베기 설정
    WhirlwindData.Cooldown = 12.0f;
    WhirlwindData.Duration = 2.0f;
    WhirlwindData.StaminaCost = 40.0f;
    WhirlwindData.Damage = 80.0f;
    WhirlwindData.Range = 300.0f;
    
    // R 스킬 - 광폭화 설정 (궁극기)
    BerserkerRageData.Cooldown = 60.0f;
    BerserkerRageData.Duration = 10.0f;
    BerserkerRageData.StaminaCost = 50.0f;
    BerserkerRageData.Damage = 0.0f;
    BerserkerRageData.Range = 0.0f;
}

// 스킬 쿨다운 시작
void AHSWarriorCharacter::StartSkillCooldown(EWarriorSkillType SkillType, float CooldownTime)
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
