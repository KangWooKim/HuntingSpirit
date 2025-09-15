// HuntingSpirit Game - Weapon Base Implementation

#include "HSWeaponBase.h"
#include "HuntingSpirit/Characters/Base/HSCharacterBase.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"

// 생성자
AHSWeaponBase::AHSWeaponBase()
{
    // Tick 비활성화 (필요시에만 활성화)
    PrimaryActorTick.bCanEverTick = false;

    // 루트 컴포넌트 설정
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    // 무기 메시 컴포넌트 생성
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(RootComponent);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

    // 상호작용 구체 생성
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(150.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 기본값 설정
    WeaponType = EHSWeaponType::Sword;
    WeaponRarity = EHSWeaponRarity::Common;
    WeaponState = EHSWeaponState::Dropped;
    WeaponName = TEXT("Basic Weapon");
    WeaponDescription = TEXT("A basic weapon.");
    MaxDurability = 100.0f;
    CurrentDurability = MaxDurability;
    bHasDurability = true;
    DurabilityLossPerAttack = 1.0f;
    OwningCharacter = nullptr;

    // 초기화
    InitializeWeapon();
}

// 게임 시작 시 호출
void AHSWeaponBase::BeginPlay()
{
    Super::BeginPlay();

    // 상호작용 이벤트 바인딩
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AHSWeaponBase::OnInteractionSphereBeginOverlap);
    InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AHSWeaponBase::OnInteractionSphereEndOverlap);

    // 공격 쿨다운 타이머 배열 초기화
    AttackCooldownTimers.SetNum(AttackPatterns.Num());
}

// 매 프레임 호출
void AHSWeaponBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 풀링 인터페이스 구현 - 활성화
void AHSWeaponBase::OnActivated_Implementation()
{
    // 무기를 드롭된 상태로 설정
    SetWeaponState(EHSWeaponState::Dropped);
    
    // 상호작용 활성화
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    
    // 메시 표시
    WeaponMesh->SetVisibility(true);
}

// 풀링 인터페이스 구현 - 비활성화
void AHSWeaponBase::OnDeactivated_Implementation()
{
    // 무기가 장착된 상태라면 해제
    if (WeaponState == EHSWeaponState::Equipped)
    {
        UnequipWeapon();
    }
    
    // 상호작용 비활성화
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // 메시 숨김
    WeaponMesh->SetVisibility(false);
    
    // 모든 쿨다운 타이머 정리
    for (FTimerHandle& Timer : AttackCooldownTimers)
    {
        GetWorld()->GetTimerManager().ClearTimer(Timer);
    }
}

// 풀링 인터페이스 구현 - 생성
void AHSWeaponBase::OnCreated_Implementation()
{
    // 무기 초기화
    InitializeWeapon();
}

// 무기 장착 함수
bool AHSWeaponBase::EquipWeapon(AHSCharacterBase* Character)
{
    if (!Character || WeaponState == EHSWeaponState::Equipped || WeaponState == EHSWeaponState::Broken)
    {
        return false;
    }

    // 기존 소유자가 있다면 해제
    if (OwningCharacter && OwningCharacter != Character)
    {
        UnequipWeapon();
    }

    // 새 소유자 설정
    OwningCharacter = Character;
    SetWeaponState(EHSWeaponState::Equipped);

    // 캐릭터에 무기 부착 (소켓 찾아서 부착)
    USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
    if (CharacterMesh)
    {
        FName WeaponSocketName = GetWeaponSocketName();
        if (CharacterMesh->DoesSocketExist(WeaponSocketName))
        {
            AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponSocketName);
        }
        else
        {
            // 기본 소켓이 없다면 손에 부착
            AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("hand_rSocket"));
        }
    }

    // 상호작용 구체 비활성화
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 이벤트 발생
    OnWeaponEquipped.Broadcast(this, Character);

    return true;
}

// 무기 해제 함수
bool AHSWeaponBase::UnequipWeapon()
{
    if (WeaponState != EHSWeaponState::Equipped || !OwningCharacter)
    {
        return false;
    }

    AHSCharacterBase* PreviousOwner = OwningCharacter;

    // 소유자 해제
    OwningCharacter = nullptr;
    SetWeaponState(EHSWeaponState::Dropped);

    // 액터 분리
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // 바닥에 배치 (약간 아래로)
    FVector DropLocation = PreviousOwner->GetActorLocation() + FVector(0.0f, 0.0f, -50.0f);
    SetActorLocation(DropLocation);

    // 상호작용 구체 재활성화
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    // 이벤트 발생
    OnWeaponUnequipped.Broadcast(this, PreviousOwner);

    return true;
}

// 공격 수행 함수
bool AHSWeaponBase::PerformAttack(int32 AttackPatternIndex)
{
    // 공격 가능 여부 확인
    if (!CanPerformAttack(AttackPatternIndex))
    {
        return false;
    }

    const FHSWeaponAttackPattern& AttackPattern = AttackPatterns[AttackPatternIndex];

    // 스태미너 소모
    if (OwningCharacter && AttackPattern.StaminaCost > 0.0f)
    {
        if (!OwningCharacter->UseStamina(AttackPattern.StaminaCost))
        {
            return false; // 스태미너 부족
        }
    }

    // 공격 애니메이션 재생
    if (AttackPattern.AttackMontage && OwningCharacter)
    {
        UAnimInstance* AnimInstance = OwningCharacter->GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->Montage_Play(AttackPattern.AttackMontage);
        }
    }

    // 공격 효과음 재생
    if (AttackPattern.AttackSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), AttackPattern.AttackSound, GetActorLocation());
    }

    // 공격 이펙트 재생
    if (AttackPattern.AttackEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), AttackPattern.AttackEffect, GetActorLocation());
    }

    // 범위 공격 수행
    TArray<AActor*> HitTargets = PerformRangeAttack(AttackPattern);

    // 타겟들에게 데미지 적용
    ApplyDamageToTargets(HitTargets, AttackPattern);

    // 내구도 감소
    if (bHasDurability)
    {
        ReduceDurability(DurabilityLossPerAttack);
    }

    // 공격 쿨다운 시작
    StartAttackCooldown(AttackPatternIndex);

    // 이벤트 발생
    OnWeaponAttack.Broadcast(this, AttackPattern, HitTargets);

    return true;
}

// 공격 패턴 가져오기
FHSWeaponAttackPattern AHSWeaponBase::GetAttackPattern(int32 Index) const
{
    if (AttackPatterns.IsValidIndex(Index))
    {
        return AttackPatterns[Index];
    }
    return FHSWeaponAttackPattern();
}

// 공격 패턴 추가
void AHSWeaponBase::AddAttackPattern(const FHSWeaponAttackPattern& Pattern)
{
    AttackPatterns.Add(Pattern);
    
    // 새로운 쿨다운 타이머 핸들 추가
    AttackCooldownTimers.Add(FTimerHandle());
}

// 내구도 설정
void AHSWeaponBase::SetDurability(float NewDurability)
{
    float OldDurability = CurrentDurability;
    CurrentDurability = FMath::Clamp(NewDurability, 0.0f, MaxDurability);

    if (FMath::Abs(OldDurability - CurrentDurability) > KINDA_SMALL_NUMBER)
    {
        OnWeaponDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);

        // 내구도가 0이 되면 무기 파괴
        if (CurrentDurability <= 0.0f && WeaponState != EHSWeaponState::Broken)
        {
            SetWeaponState(EHSWeaponState::Broken);
            OnWeaponBroken.Broadcast(this);
        }
    }
}

// 무기 수리
void AHSWeaponBase::RepairWeapon(float RepairAmount)
{
    if (RepairAmount < 0.0f)
    {
        // 완전 수리
        SetDurability(MaxDurability);
    }
    else
    {
        // 지정된 양만큼 수리
        SetDurability(CurrentDurability + RepairAmount);
    }

    // 파괴된 상태에서 수리하면 드롭 상태로 복원
    if (WeaponState == EHSWeaponState::Broken && CurrentDurability > 0.0f)
    {
        SetWeaponState(EHSWeaponState::Dropped);
    }
}

// 무기 초기화
void AHSWeaponBase::InitializeWeapon()
{
    // 기본 공격 패턴이 없다면 추가
    if (AttackPatterns.Num() == 0)
    {
        FHSWeaponAttackPattern BasicAttack;
        BasicAttack.AttackName = TEXT("Basic Attack");
        BasicAttack.DamageInfo.BaseDamage = 25.0f;
        BasicAttack.DamageInfo.DamageType = EHSDamageType::Physical;
        BasicAttack.AttackRange = 150.0f;
        BasicAttack.AttackAngle = 90.0f;
        BasicAttack.Cooldown = 1.0f;
        BasicAttack.StaminaCost = 10.0f;
        
        AttackPatterns.Add(BasicAttack);
    }

    // 쿨다운 타이머 배열 크기 조정
    AttackCooldownTimers.SetNum(AttackPatterns.Num());

    // 내구도 초기화
    CurrentDurability = MaxDurability;
}

// 무기 상태 설정
void AHSWeaponBase::SetWeaponState(EHSWeaponState NewState)
{
    if (WeaponState != NewState)
    {
        WeaponState = NewState;

        // 상태에 따른 처리
        switch (NewState)
        {
            case EHSWeaponState::Dropped:
                // 드롭 상태: 상호작용 가능
                InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                break;

            case EHSWeaponState::Equipped:
                // 장착 상태: 상호작용 불가능
                InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                break;

            case EHSWeaponState::Stored:
                // 보관 상태: 모든 충돌 비활성화
                InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                WeaponMesh->SetVisibility(false);
                break;

            case EHSWeaponState::Broken:
                // 파괴 상태: 기능 비활성화
                if (OwningCharacter)
                {
                    UnequipWeapon();
                }
                break;
        }
    }
}

// 내구도 감소
void AHSWeaponBase::ReduceDurability(float Amount)
{
    if (bHasDurability && Amount > 0.0f)
    {
        SetDurability(CurrentDurability - Amount);
    }
}

// 범위 공격 수행
TArray<AActor*> AHSWeaponBase::PerformRangeAttack(const FHSWeaponAttackPattern& Pattern)
{
    TArray<AActor*> HitTargets;

    if (!OwningCharacter)
    {
        return HitTargets;
    }

    // 공격 시작 위치 (무기 위치 또는 캐릭터 위치)
    FVector AttackOrigin = GetActorLocation();
    if (WeaponState != EHSWeaponState::Equipped)
    {
        AttackOrigin = OwningCharacter->GetActorLocation();
    }

    // 공격 방향 (캐릭터 전방)
    FVector AttackDirection = OwningCharacter->GetActorForwardVector();

    // 구체 스윕으로 타겟 탐지
    TArray<FHitResult> HitResults;
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(OwningCharacter);

    // 구체 반지름 계산 (공격 범위의 절반)
    float SphereRadius = Pattern.AttackRange * 0.3f;

    // 공격 범위 스윕
    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults,
        AttackOrigin,
        AttackOrigin + (AttackDirection * Pattern.AttackRange),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(SphereRadius),
        QueryParams
    );

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor && HitActor != OwningCharacter)
            {
                // 공격 각도 내에 있는지 확인
                FVector ToTarget = (HitActor->GetActorLocation() - AttackOrigin).GetSafeNormal();
                float DotProduct = FVector::DotProduct(AttackDirection, ToTarget);
                float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

                if (AngleToTarget <= Pattern.AttackAngle * 0.5f)
                {
                    // 적인지 확인 (같은 팀이 아닌 경우만 공격)
                    if (IsValidTarget(HitActor))
                    {
                        HitTargets.AddUnique(HitActor);
                    }
                }
            }
        }
    }

    return HitTargets;
}

// 타겟들에게 데미지 적용
void AHSWeaponBase::ApplyDamageToTargets(const TArray<AActor*>& Targets, const FHSWeaponAttackPattern& Pattern)
{
    for (AActor* Target : Targets)
    {
        if (!Target)
        {
            continue;
        }

        // 타겟의 전투 컴포넌트 찾기
        UHSCombatComponent* TargetCombat = Target->FindComponentByClass<UHSCombatComponent>();
        if (TargetCombat)
        {
            // 데미지 적용
            TargetCombat->ApplyDamage(Pattern.DamageInfo, OwningCharacter);
        }
    }
}

// 공격 가능 여부 확인
bool AHSWeaponBase::CanPerformAttack(int32 PatternIndex) const
{
    // 기본 검사
    if (!OwningCharacter || WeaponState != EHSWeaponState::Equipped || IsBroken())
    {
        return false;
    }

    // 패턴 인덱스 유효성 검사
    if (!AttackPatterns.IsValidIndex(PatternIndex))
    {
        return false;
    }

    // 쿨다운 검사
    if (AttackCooldownTimers.IsValidIndex(PatternIndex))
    {
        const FTimerHandle& Timer = AttackCooldownTimers[PatternIndex];
        if (GetWorld()->GetTimerManager().IsTimerActive(Timer))
        {
            return false;
        }
    }

    // 캐릭터 상태 검사
    if (OwningCharacter->GetCharacterState() == ECharacterState::Dead ||
        OwningCharacter->GetCharacterState() == ECharacterState::Attacking)
    {
        return false;
    }

    return true;
}

// 공격 쿨다운 시작
void AHSWeaponBase::StartAttackCooldown(int32 PatternIndex)
{
    if (!AttackPatterns.IsValidIndex(PatternIndex) || !AttackCooldownTimers.IsValidIndex(PatternIndex))
    {
        return;
    }

    const FHSWeaponAttackPattern& Pattern = AttackPatterns[PatternIndex];
    FTimerHandle& TimerHandle = AttackCooldownTimers[PatternIndex];

    if (Pattern.Cooldown > 0.0f)
    {
        FTimerDelegate TimerDelegate;
        TimerDelegate.BindUFunction(this, FName("OnAttackCooldownExpired"), PatternIndex);
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, Pattern.Cooldown, false);
    }
}

// 공격 쿨다운 만료
void AHSWeaponBase::OnAttackCooldownExpired(int32 PatternIndex)
{
    // 쿨다운 완료 (타이머는 자동으로 정리됨)
}

// 유효한 타겟인지 확인
bool AHSWeaponBase::IsValidTarget(AActor* Target) const
{
    if (!Target || !OwningCharacter)
    {
        return false;
    }

    // 같은 액터인지 확인
    if (Target == OwningCharacter)
    {
        return false;
    }

    // 전투 컴포넌트가 있는지 확인
    UHSCombatComponent* TargetCombat = Target->FindComponentByClass<UHSCombatComponent>();
    if (!TargetCombat || TargetCombat->IsDead())
    {
        return false;
    }

    // 팀 시스템이 구현되면 여기서 팀 확인 로직 추가
    // 현재는 플레이어가 아닌 캐릭터를 타겟으로 간주
    AHSCharacterBase* TargetCharacter = Cast<AHSCharacterBase>(Target);
    AHSCharacterBase* OwnerCharacterBase = Cast<AHSCharacterBase>(OwningCharacter);
    
    if (TargetCharacter && OwnerCharacterBase)
    {
        // 현재는 간단히 플레이어와 적을 구분 (나중에 팀 시스템으로 교체)
        bool bOwnerIsPlayer = OwnerCharacterBase->IsPlayerControlled();
        bool bTargetIsPlayer = TargetCharacter->IsPlayerControlled();
        
        // 플레이어는 적만 공격, 적은 플레이어만 공격
        return bOwnerIsPlayer != bTargetIsPlayer;
    }

    return true;
}

// 무기 소켓 이름 가져오기
FName AHSWeaponBase::GetWeaponSocketName() const
{
    switch (WeaponType)
    {
        case EHSWeaponType::Sword:
        case EHSWeaponType::GreatSword:
            return TEXT("weapon_sword_socket");
        case EHSWeaponType::Dagger:
        case EHSWeaponType::DualBlades:
            return TEXT("weapon_dagger_socket");
        case EHSWeaponType::Staff:
        case EHSWeaponType::Wand:
            return TEXT("weapon_staff_socket");
        case EHSWeaponType::Bow:
        case EHSWeaponType::Crossbow:
            return TEXT("weapon_bow_socket");
        default:
            return TEXT("hand_rSocket");
    }
}

// 상호작용 구체 오버랩 시작
void AHSWeaponBase::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 플레이어 캐릭터가 근처에 왔을 때의 처리
    AHSCharacterBase* Character = Cast<AHSCharacterBase>(OtherActor);
    if (Character && Character->IsPlayerControlled() && WeaponState == EHSWeaponState::Dropped)
    {
        // UI에 상호작용 프롬프트 표시 등의 로직 추가 가능
        // 예: "E키를 눌러 무기 획득"
    }
}

// 상호작용 구체 오버랩 종료
void AHSWeaponBase::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 플레이어 캐릭터가 멀어졌을 때의 처리
    AHSCharacterBase* Character = Cast<AHSCharacterBase>(OtherActor);
    if (Character && Character->IsPlayerControlled())
    {
        // UI에서 상호작용 프롬프트 숨김 등의 로직 추가 가능
    }
}
