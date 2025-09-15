/**
 * @file HSMageCharacter.cpp
 * @brief 마법사 캐릭터 클래스 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#include "HSMageCharacter.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HuntingSpirit/Combat/Projectiles/HSMagicProjectile.h"

// 생성자
AHSMageCharacter::AHSMageCharacter()
{
    // 마법사 클래스로 설정
    PlayerClass = EHSPlayerClass::Mage;
    
    // 마법사 특화 설정 - 느리지만 강력한 원거리
    GetCharacterMovement()->MaxWalkSpeed = 350.0f;         // 느린 걷기 속도
    GetCharacterMovement()->MaxAcceleration = 1200.0f;     // 보통 가속도
    GetCharacterMovement()->RotationRate = FRotator(0, 540.0f, 0); // 보통 회전 속도
    
    // 캡슐 컴포넌트 조정 (표준 히트박스)
    GetCapsuleComponent()->SetCapsuleSize(42.0f, 96.0f);
    
    // 마법사 특화 스탯 설정
    SetupMageStats();
    
    // 마법사 스킬 초기화
    InitializeMageSkills();
    
    // 마나 초기화
    ManaCurrent = ManaMax;
}

// 게임 시작 또는 스폰 시 호출
void AHSMageCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 마법사 특화 초기화 로직
    CurrentMagicType = EMagicType::Fire;
}

// 매 프레임 호출
void AHSMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 마나 재생
    RegenerateMana(DeltaTime);
}

// 특수 공격 (오버라이드) - 마법 발사체 공격
void AHSMageCharacter::PerformBasicAttack()
{
    // 마나 소모 체크
    float BasicAttackManaCost = 10.0f;
    if (!ConsumeMana(BasicAttackManaCost))
    {
        // 마나 부족 시 기본 공격 대신 근접 공격
        Super::PerformBasicAttack();
        return;
    }
    
    // 마법 발사체 생성 로직
    Super::PerformBasicAttack();
    
    // 기본 마법 발사체 생성
    if (FireballData.ProjectileClass) // 기본적으로 화염구 사용
    {
        FVector ForwardDirection = GetActorForwardVector();
        SpawnMagicProjectile(FireballData.ProjectileClass, ForwardDirection);
    }
}

// === 마법사 스킬 시스템 (QWER) ===

// Q 스킬 - 화염구
void AHSMageCharacter::UseSkillQ()
{
    if (CanUseSkill(EMageSkillType::Fireball))
    {
        StartCasting(EMageSkillType::Fireball, FireballData.CastTime);
        StartSkillCooldown(EMageSkillType::Fireball, FireballData.Cooldown);
    }
}

// W 스킬 - 얼음창
void AHSMageCharacter::UseSkillW()
{
    if (CanUseSkill(EMageSkillType::IceShard))
    {
        StartCasting(EMageSkillType::IceShard, IceShardData.CastTime);
        StartSkillCooldown(EMageSkillType::IceShard, IceShardData.Cooldown);
    }
}

// E 스킬 - 번개
void AHSMageCharacter::UseSkillE()
{
    if (CanUseSkill(EMageSkillType::LightningBolt))
    {
        StartCasting(EMageSkillType::LightningBolt, LightningBoltData.CastTime);
        StartSkillCooldown(EMageSkillType::LightningBolt, LightningBoltData.Cooldown);
    }
}

// R 스킬 - 메테오 (궁극기)
void AHSMageCharacter::UseSkillR()
{
    if (CanUseSkill(EMageSkillType::Meteor))
    {
        StartCasting(EMageSkillType::Meteor, MeteorData.CastTime);
        StartSkillCooldown(EMageSkillType::Meteor, MeteorData.Cooldown);
    }
}

// 스킬 사용 가능 여부 확인
bool AHSMageCharacter::CanUseSkill(EMageSkillType SkillType) const
{
    // 죽음 상태 또는 공격 중이거나 이미 캐스팅 중이면 스킬 사용 불가
    if (CurrentState == ECharacterState::Dead || CurrentState == ECharacterState::Attacking || bIsCasting)
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
    
    // 스킬별 마나 소모 체크
    FMageSkillData SkillData = GetSkillData(SkillType);
    if (ManaCurrent < SkillData.ManaCost)
    {
        return false;
    }
    
    return true;
}

// 스킬 쿨다운 남은 시간 반환
float AHSMageCharacter::GetSkillCooldownRemaining(EMageSkillType SkillType) const
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
FMageSkillData AHSMageCharacter::GetSkillData(EMageSkillType SkillType) const
{
    switch (SkillType)
    {
        case EMageSkillType::Fireball:
            return FireballData;
        case EMageSkillType::IceShard:
            return IceShardData;
        case EMageSkillType::LightningBolt:
            return LightningBoltData;
        case EMageSkillType::Meteor:
            return MeteorData;
        default:
            return FMageSkillData();
    }
}

// === 마나 관리 ===

// 마나 소모
bool AHSMageCharacter::ConsumeMana(float ManaAmount)
{
    if (ManaCurrent >= ManaAmount)
    {
        ManaCurrent = FMath::Max(0.0f, ManaCurrent - ManaAmount);
        return true;
    }
    return false;
}

// 마나 회복
void AHSMageCharacter::RestoreMana(float ManaAmount)
{
    ManaCurrent = FMath::Min(ManaMax, ManaCurrent + ManaAmount);
}

// 마나 재생
void AHSMageCharacter::RegenerateMana(float DeltaTime)
{
    if (ManaCurrent < ManaMax)
    {
        RestoreMana(ManaRegenRate * DeltaTime);
    }
}

// === 캐스팅 시스템 ===

// 캐스팅 시작
void AHSMageCharacter::StartCasting(EMageSkillType SkillType, float CastTime)
{
    // 이미 캐스팅 중이면 무시
    if (bIsCasting)
    {
        return;
    }
    
    // 마나 소모
    FMageSkillData SkillData = GetSkillData(SkillType);
    if (!ConsumeMana(SkillData.ManaCost))
    {
        return;
    }
    
    // 캐스팅 상태 설정
    bIsCasting = true;
    CurrentCastingSkill = SkillType;
    
    // 캐스팅 중에는 이동 속도 감소
    GetCharacterMovement()->MaxWalkSpeed *= 0.5f;
    
    // 캐스팅 애니메이션 재생
    if (SkillData.SkillMontage)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(SkillData.SkillMontage, 1.0f);
        }
    }
    
    // 캐스팅 완료 타이머 설정
    GetWorldTimerManager().SetTimer(CastingTimerHandle, this, &AHSMageCharacter::FinishCasting, CastTime, false);
}

// 캐스팅 완료
void AHSMageCharacter::FinishCasting()
{
    if (!bIsCasting)
    {
        return;
    }
    
    // 스킬 실행
    switch (CurrentCastingSkill)
    {
        case EMageSkillType::Fireball:
            ExecuteFireball();
            break;
        case EMageSkillType::IceShard:
            ExecuteIceShard();
            break;
        case EMageSkillType::LightningBolt:
            ExecuteLightningBolt();
            break;
        case EMageSkillType::Meteor:
            ExecuteMeteor();
            break;
        default:
            break;
    }
    
    // 캐스팅 상태 해제
    bIsCasting = false;
    CurrentCastingSkill = EMageSkillType::None;
    
    // 이동 속도 복구
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

// 캐스팅 중단
void AHSMageCharacter::InterruptCasting()
{
    if (!bIsCasting)
    {
        return;
    }
    
    // 타이머 클리어
    GetWorldTimerManager().ClearTimer(CastingTimerHandle);
    
    // 캐스팅 상태 해제
    bIsCasting = false;
    CurrentCastingSkill = EMageSkillType::None;
    
    // 이동 속도 복구
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    
    // 애니메이션 중단
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance)
    {
        AnimInstance->Montage_Stop(0.2f);
    }
}

// === 스킬 구현 내부 함수들 ===

// Q 스킬 실행 - 화염구
void AHSMageCharacter::ExecuteFireball()
{
    if (FireballData.ProjectileClass)
    {
        FVector ForwardDirection = GetActorForwardVector();
        SpawnMagicProjectile(FireballData.ProjectileClass, ForwardDirection);
    }
}

// W 스킬 실행 - 얼음창
void AHSMageCharacter::ExecuteIceShard()
{
    if (IceShardData.ProjectileClass)
    {
        // 3개의 얼음창을 부채꼴 모양으로 발사
        FVector ForwardDirection = GetActorForwardVector();
        FVector RightDirection = GetActorRightVector();
        
        // 중앙
        SpawnMagicProjectile(IceShardData.ProjectileClass, ForwardDirection);
        
        // 좌측 15도
        FVector LeftDirection = ForwardDirection.RotateAngleAxis(-15.0f, FVector::UpVector);
        SpawnMagicProjectile(IceShardData.ProjectileClass, LeftDirection);
        
        // 우측 15도
        FVector RightDirectionAngled = ForwardDirection.RotateAngleAxis(15.0f, FVector::UpVector);
        SpawnMagicProjectile(IceShardData.ProjectileClass, RightDirectionAngled);
    }
}

// E 스킬 실행 - 번개
void AHSMageCharacter::ExecuteLightningBolt()
{
    // 번개는 즉시 타격하는 타겟팅 스킬
    // TODO: 마우스 커서 위치 또는 가장 가까운 적에게 즉시 데미지
    
    // 임시로 전방 직선상의 적에게 즉시 데미지
    if (LightningBoltData.ProjectileClass)
    {
        FVector ForwardDirection = GetActorForwardVector();
        SpawnMagicProjectile(LightningBoltData.ProjectileClass, ForwardDirection);
    }
    
    // TODO: 라인 트레이스로 즉시 피해 적용 + 시각 효과
}

// R 스킬 실행 - 메테오 (궁극기)
void AHSMageCharacter::ExecuteMeteor()
{
    // 메테오는 지연 시간 후 광역 공격
    // TODO: 마우스 커서 위치에 메테오 낙하 마커 표시
    
    // 임시로 전방 위치에 메테오 소환
    FVector MeteorLocation = GetActorLocation() + GetActorForwardVector() * 800.0f;
    
    // 3초 후 메테오 임팩트
    GetWorldTimerManager().SetTimer(MeteorImpactTimerHandle, [this, MeteorLocation]()
    {
        // 메테오 임팩트 효과
        if (MeteorData.ProjectileClass)
        {
            FVector SpawnLocation = MeteorLocation + FVector(0, 0, 1000.0f); // 하늘에서 생성
            FVector DownDirection = FVector(0, 0, -1);
            
            // 메테오 발사체 생성 (위에서 아래로)
            if (UWorld* World = GetWorld())
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.Instigator = this;
                
                if (AActor* Meteor = World->SpawnActor<AActor>(MeteorData.ProjectileClass, SpawnLocation, DownDirection.Rotation(), SpawnParams))
                {
                    // 메테오 특수 설정
                }
            }
        }
        
        // TODO: 광역 데미지 적용
        
    }, 3.0f, false);
}

// === 유틸리티 함수들 ===

// 발사체 생성
void AHSMageCharacter::SpawnMagicProjectile(TSubclassOf<AActor> ProjectileClass, const FVector& Direction)
{
    if (!ProjectileClass)
    {
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    // 스폰 위치 (캐릭터 앞쪽)
    FVector SpawnLocation = GetActorLocation() + Direction * 100.0f + FVector(0, 0, 50.0f);
    FRotator SpawnRotation = Direction.Rotation();
    
    // 발사체 생성
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    
    AActor* Projectile = World->SpawnActor<AActor>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
    
    // 발사체에 추가 설정이 필요한 경우 여기서 처리
}

// 마법 타입 전환
void AHSMageCharacter::CycleMagicType()
{
    switch (CurrentMagicType)
    {
        case EMagicType::Fire:
            CurrentMagicType = EMagicType::Ice;
            break;
        case EMagicType::Ice:
            CurrentMagicType = EMagicType::Lightning;
            break;
        case EMagicType::Lightning:
            CurrentMagicType = EMagicType::Arcane;
            break;
        case EMagicType::Arcane:
            CurrentMagicType = EMagicType::Fire;
            break;
        default:
            CurrentMagicType = EMagicType::Fire;
            break;
    }
}

// 마법사 특화 스탯 설정
void AHSMageCharacter::SetupMageStats()
{
    // 예시: 체력, 마나, 마법 공격력 등 스탯 설정
    /*
    if (StatsComponent)
    {
        StatsComponent->SetMaxHealth(80.0f);     // 낮은 체력
        StatsComponent->SetMaxMana(150.0f);      // 높은 마나
        StatsComponent->SetDefense(10.0f);       // 낮은 방어력
        StatsComponent->SetMagicalPower(60.0f);  // 높은 마법 공격력
        StatsComponent->SetAttackSpeed(0.9f);    // 느린 공격 속도
    }
    */
}

// 스킬 초기화
void AHSMageCharacter::InitializeMageSkills()
{
    // Q 스킬 - 화염구 설정
    FireballData.Cooldown = 4.0f;
    FireballData.CastTime = 1.5f;
    FireballData.ManaCost = 25.0f;
    FireballData.Damage = 60.0f;
    FireballData.Range = 800.0f;
    FireballData.MagicType = EMagicType::Fire;
    
    // W 스킬 - 얼음창 설정
    IceShardData.Cooldown = 6.0f;
    IceShardData.CastTime = 1.2f;
    IceShardData.ManaCost = 35.0f;
    IceShardData.Damage = 45.0f; // 3개니까 총 135 데미지
    IceShardData.Range = 600.0f;
    IceShardData.MagicType = EMagicType::Ice;
    
    // E 스킬 - 번개 설정
    LightningBoltData.Cooldown = 8.0f;
    LightningBoltData.CastTime = 0.8f;
    LightningBoltData.ManaCost = 40.0f;
    LightningBoltData.Damage = 80.0f;
    LightningBoltData.Range = 1000.0f;
    LightningBoltData.MagicType = EMagicType::Lightning;
    
    // R 스킬 - 메테오 설정 (궁극기)
    MeteorData.Cooldown = 50.0f;
    MeteorData.CastTime = 2.5f;
    MeteorData.ManaCost = 80.0f;
    MeteorData.Damage = 200.0f; // 광역 대데미지
    MeteorData.Range = 1200.0f;
    MeteorData.MagicType = EMagicType::Arcane;
}

// 스킬 쿨다운 시작
void AHSMageCharacter::StartSkillCooldown(EMageSkillType SkillType, float CooldownTime)
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
