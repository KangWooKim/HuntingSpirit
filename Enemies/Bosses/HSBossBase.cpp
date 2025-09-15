// HuntingSpirit Game - Boss Base Implementation
// 모든 보스 몬스터의 기본 클래스 구현

#include "HSBossBase.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Combat/HSHitReactionComponent.h"
#include "HuntingSpirit/AI/HSAIControllerBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimInstance.h"
#include "HuntingSpirit/Optimization/ObjectPool/HSObjectPool.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HuntingSpirit/Enemies/Regular/HSBasicMeleeEnemy.h"
#include "HuntingSpirit/Enemies/Regular/HSBasicRangedEnemy.h"
#include "DrawDebugHelpers.h"

// Sets default values
AHSBossBase::AHSBossBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // 보스 기본 설정
    EnemyType = EHSEnemyType::Boss;
    EnemyRank = EHSEnemyRank::Boss;
    EnemyName = TEXT("Boss");
    
    // 보스는 기본적으로 더 큰 크기를 가짐
    GetCapsuleComponent()->SetCapsuleSize(120.0f, 200.0f);
    
    // 보스 체력바 UI 컴포넌트 생성
    BossHealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("BossHealthBar"));
    BossHealthBarComponent->SetupAttachment(RootComponent);
    BossHealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));
    BossHealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
    BossHealthBarComponent->SetDrawSize(FVector2D(400.0f, 50.0f));
    
    // 보스 이름표 UI 컴포넌트 생성
    BossNameplateComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("BossNameplate"));
    BossNameplateComponent->SetupAttachment(RootComponent);
    BossNameplateComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 350.0f));
    BossNameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
    BossNameplateComponent->SetDrawSize(FVector2D(300.0f, 40.0f));
    
    // 페이즈 전환 효과 컴포넌트
    PhaseTransitionEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("PhaseTransitionEffect"));
    PhaseTransitionEffect->SetupAttachment(RootComponent);
    PhaseTransitionEffect->bAutoActivate = false;
    
    // 분노 효과 컴포넌트
    EnrageEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("EnrageEffect"));
    EnrageEffect->SetupAttachment(RootComponent);
    EnrageEffect->bAutoActivate = false;
    
    // 확장 히트박스 (대형 보스용)
    ExtendedHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("ExtendedHitbox"));
    ExtendedHitbox->SetupAttachment(RootComponent);
    ExtendedHitbox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    ExtendedHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ExtendedHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    ExtendedHitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    
    // 기본 스탯 설정
    BaseMaxHealth = 10000.0f;
    SetMaxHealth(BaseMaxHealth);
    SetHealth(BaseMaxHealth);
    
    // AI 설정
    DetectionRange = 2000.0f;
    LoseTargetRange = 3000.0f;
    AttackRange = 300.0f;
    AggroRange = 2000.0f;
    bCanLoseAggro = false;
    
    // 기본 페이즈 임계값 설정
    InitializePhaseThresholds();
}

// 매 프레임 호출
void AHSBossBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 보스 UI 업데이트
    UpdateBossUI();
    
    // 협동 플레이어 업데이트
    UpdateEngagedPlayers();
    
    // 페이즈 전환 체크
    CheckPhaseTransition();
    
    // 분노 모드 자동 활성화 체크
    if (!bIsEnraged && GetHealthPercent() <= EnrageHealthThreshold)
    {
        EnterEnrageMode();
    }
}

// 게임 시작 시 호출
void AHSBossBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 보스 초기화
    InitializeBoss();
    
    // 컴포넌트 설정
    SetupBossComponents();
    
    // 공격 패턴 초기화
    InitializeAttackPatterns();
    
    // 보스 이벤트 브로드캐스트
    OnBossHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}

// 보스 초기화
void AHSBossBase::InitializeBoss()
{
    // 보스 전용 스탯 적용
    ScaleStatsForRank();
    
    // 페이즈별 데미지 배율 설정
    PhaseDamageMultipliers.Add(EHSBossPhase::Phase1, 1.0f);
    PhaseDamageMultipliers.Add(EHSBossPhase::Phase2, 1.2f);
    PhaseDamageMultipliers.Add(EHSBossPhase::Phase3, 1.5f);
    PhaseDamageMultipliers.Add(EHSBossPhase::Enraged, 2.0f);
    PhaseDamageMultipliers.Add(EHSBossPhase::Final, 2.5f);
    
    // 특수 능력 초기화
    SpecialAbilities.Add(TEXT("PhaseShield"), false);
    SpecialAbilities.Add(TEXT("DamageReflection"), false);
    SpecialAbilities.Add(TEXT("Summon"), false);
    SpecialAbilities.Add(TEXT("AreaDenial"), false);
}

// 보스 컴포넌트 설정
void AHSBossBase::SetupBossComponents()
{
    // UI 위젯 클래스 설정은 블루프린트에서 처리
    
    // 이동 속도 조정 (보스는 일반적으로 느림)
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = 300.0f;
    }
}

// 페이즈 임계값 초기화
void AHSBossBase::InitializePhaseThresholds()
{
    PhaseHealthThresholds.Add(EHSBossPhase::Phase1, 1.0f);
    PhaseHealthThresholds.Add(EHSBossPhase::Phase2, 0.75f);
    PhaseHealthThresholds.Add(EHSBossPhase::Phase3, 0.5f);
    PhaseHealthThresholds.Add(EHSBossPhase::Enraged, 0.25f);
    PhaseHealthThresholds.Add(EHSBossPhase::Final, 0.1f);
}

// 공격 패턴 초기화
void AHSBossBase::InitializeAttackPatterns()
{
    // 기본 패턴들은 블루프린트에서 설정
    // 패턴 가중치 초기화
    for (const FHSBossAttackPattern& Pattern : AttackPatterns)
    {
        PatternWeights.Add(Pattern.PatternName, 1.0f);
    }
}

// 페이즈 설정
void AHSBossBase::SetBossPhase(EHSBossPhase NewPhase)
{
    if (CurrentPhase != NewPhase)
    {
        EHSBossPhase OldPhase = CurrentPhase;
        CurrentPhase = NewPhase;
        
        // 페이즈 전환 처리
        OnPhaseTransition(OldPhase, NewPhase);
        
        // 델리게이트 브로드캐스트
        OnBossPhaseChanged.Broadcast(OldPhase, NewPhase);
    }
}

// 페이즈 전환 체크
void AHSBossBase::CheckPhaseTransition()
{
    float HealthPercent = GetHealthPercent();
    
    // 현재 체력에 따른 적절한 페이즈 찾기
    EHSBossPhase TargetPhase = CurrentPhase;
    
    if (HealthPercent <= PhaseHealthThresholds[EHSBossPhase::Final])
    {
        TargetPhase = EHSBossPhase::Final;
    }
    else if (HealthPercent <= PhaseHealthThresholds[EHSBossPhase::Enraged])
    {
        TargetPhase = EHSBossPhase::Enraged;
    }
    else if (HealthPercent <= PhaseHealthThresholds[EHSBossPhase::Phase3])
    {
        TargetPhase = EHSBossPhase::Phase3;
    }
    else if (HealthPercent <= PhaseHealthThresholds[EHSBossPhase::Phase2])
    {
        TargetPhase = EHSBossPhase::Phase2;
    }
    
    // 페이즈 변경
    if (TargetPhase != CurrentPhase)
    {
        SetBossPhase(TargetPhase);
    }
}

// 페이즈 전환 처리
void AHSBossBase::OnPhaseTransition(EHSBossPhase OldPhase, EHSBossPhase NewPhase)
{
    // 페이즈 전환 효과 재생
    PlayPhaseTransitionEffects();
    
    // 페이즈별 특수 처리
    switch (NewPhase)
    {
        case EHSBossPhase::Phase2:
            // 페이즈 2 진입 시 특수 능력 활성화
            ActivateSpecialAbility(TEXT("PhaseShield"));
            break;
            
        case EHSBossPhase::Phase3:
            // 페이즈 3 진입 시 소환 능력 활성화
            ActivateSpecialAbility(TEXT("Summon"));
            break;
            
        case EHSBossPhase::Enraged:
            // 분노 모드는 별도로 처리
            if (!bIsEnraged)
            {
                EnterEnrageMode();
            }
            break;
            
        case EHSBossPhase::Final:
            // 최종 단계 - 모든 능력 활성화
            ActivateSpecialAbility(TEXT("DamageReflection"));
            ActivateSpecialAbility(TEXT("AreaDenial"));
            break;
    }
    
    // 패턴 가중치 업데이트
    UpdatePatternWeights();
}

// 페이즈 전환 효과 재생
void AHSBossBase::PlayPhaseTransitionEffects()
{
    if (PhaseTransitionEffect)
    {
        PhaseTransitionEffect->Activate(true);
    }
    
    // 화면 흔들림 효과
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->ClientStartCameraShake(nullptr); // 카메라 쉐이크 클래스는 블루프린트에서 설정
    }
}

// 공격 패턴 실행
void AHSBossBase::ExecuteAttackPattern(const FHSBossAttackPattern& Pattern)
{
    if (bIsExecutingPattern || !CanExecutePattern(Pattern))
    {
        return;
    }
    
    // 패턴 실행 시작
    bIsExecutingPattern = true;
    CurrentPattern = Pattern;
    
    // 패턴 시작 이벤트
    OnBossPatternStart.Broadcast(Pattern);
    
    // 애니메이션 재생
    if (Pattern.AnimationMontage)
    {
        PlayAnimMontage(Pattern.AnimationMontage);
    }
    
    // 사운드 재생
    if (Pattern.SoundEffect)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Pattern.SoundEffect, GetActorLocation());
    }
    
    // 패턴 타입별 실행
    switch (Pattern.PatternType)
    {
        case EHSBossPatternType::Melee:
            ExecuteMeleePattern(Pattern);
            break;
            
        case EHSBossPatternType::Ranged:
            ExecuteRangedPattern(Pattern);
            break;
            
        case EHSBossPatternType::Area:
            ExecuteAreaPattern(Pattern);
            break;
            
        case EHSBossPatternType::Special:
            ExecuteSpecialPattern(Pattern);
            break;
            
        case EHSBossPatternType::Ultimate:
            ExecuteUltimatePattern(Pattern);
            break;
    }
    
    // 패턴 완료 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        PatternExecutionTimer,
        this,
        &AHSBossBase::OnPatternActivationComplete,
        Pattern.ActivationTime,
        false
    );
}

// 다음 패턴 선택
FHSBossAttackPattern AHSBossBase::SelectNextPattern()
{
    TArray<FHSBossAttackPattern> AvailablePatterns;
    float TotalWeight = 0.0f;
    
    // 사용 가능한 패턴 필터링
    for (const FHSBossAttackPattern& Pattern : AttackPatterns)
    {
        if (CanExecutePattern(Pattern))
        {
            AvailablePatterns.Add(Pattern);
            TotalWeight += PatternWeights[Pattern.PatternName];
        }
    }
    
    // 가중치 기반 랜덤 선택
    if (AvailablePatterns.Num() > 0)
    {
        float RandomValue = FMath::FRandRange(0.0f, TotalWeight);
        float CurrentWeight = 0.0f;
        
        for (const FHSBossAttackPattern& Pattern : AvailablePatterns)
        {
            CurrentWeight += PatternWeights[Pattern.PatternName];
            if (RandomValue <= CurrentWeight)
            {
                return Pattern;
            }
        }
        
        // 기본값 반환
        return AvailablePatterns[0];
    }
    
    // 사용 가능한 패턴이 없으면 기본 패턴 반환
    return FHSBossAttackPattern();
}

// 패턴 실행 가능 여부 확인
bool AHSBossBase::CanExecutePattern(const FHSBossAttackPattern& Pattern) const
{
    // 페이즈 요구사항 확인
    int32 CurrentPhaseNum = (int32)CurrentPhase + 1;
    if (CurrentPhaseNum < Pattern.MinimumPhase)
    {
        return false;
    }
    
    // 협동 요구사항 확인
    if (Pattern.bRequiresMultiplePlayers && GetActivePlayerCount() < MinPlayersForCoopMechanic)
    {
        return false;
    }
    
    // 쿨다운 확인 (쿨다운 타이머가 활성화되어 있으면 실행 불가)
    if (PatternCooldownTimer.IsValid())
    {
        return false;
    }
    
    // 타겟 거리 확인
    if (CurrentTarget)
    {
        float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
        if (Distance > Pattern.Range)
        {
            return false;
        }
    }
    
    return true;
}

// 근접 패턴 실행
void AHSBossBase::ExecuteMeleePattern(const FHSBossAttackPattern& Pattern)
{
    // 근접 공격 로직
    if (CurrentTarget)
    {
        // 타겟 방향으로 회전
        FVector Direction = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        SetActorRotation(Direction.Rotation());
        
        // 데미지 적용 (실제 데미지는 애니메이션 노티파이에서 처리)
        float FinalDamage = Pattern.Damage * PhaseDamageMultipliers[CurrentPhase];
        if (bIsEnraged)
        {
            FinalDamage *= EnrageDamageMultiplier;
        }
        
        // 임시 데미지 정보 저장
        AttackDamageInfo.BaseDamage = FinalDamage;
        AttackDamageInfo.DamageType = EHSDamageType::Physical;
    }
}

// 원거리 패턴 실행
void AHSBossBase::ExecuteRangedPattern(const FHSBossAttackPattern& Pattern)
{
    // 원거리 공격 로직 - 발사체 생성 등
    if (CurrentTarget)
    {
        // 발사체 스폰 위치 계산
        FVector ProjectileSpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f + FVector(0, 0, 100);
        FRotator SpawnRotation = (CurrentTarget->GetActorLocation() - ProjectileSpawnLocation).Rotation();
        
        // 발사체 스폰 (오브젝트 풀에서 가져오기)
        if (UWorld* World = GetWorld())
        {
            // 오브젝트 풀에서 발사체 가져오기
            if (AHSObjectPool* ObjectPool = Cast<AHSObjectPool>(UGameplayStatics::GetActorOfClass(World, AHSObjectPool::StaticClass())))
            {
                // 보스 전용 발사체 클래스 사용 (블루프린트에서 설정)
                if (AActor* Projectile = ObjectPool->GetPooledObject())
                {
                    Projectile->SetActorLocation(ProjectileSpawnLocation);
                    Projectile->SetActorRotation(SpawnRotation);
                    
                    // 발사체 초기화 및 데미지 설정
                    if (UPrimitiveComponent* ProjectileComp = Cast<UPrimitiveComponent>(Projectile->GetRootComponent()))
                    {
                        // 발사체 속도 설정
                        FVector LaunchVelocity = SpawnRotation.Vector() * 2000.0f;
                        ProjectileComp->SetPhysicsLinearVelocity(LaunchVelocity);
                    }
                    
                    // 데미지 정보 저장 (발사체가 충돌 시 사용)
                    float FinalDamage = Pattern.Damage * PhaseDamageMultipliers[CurrentPhase];
                    if (bIsEnraged)
                    {
                        FinalDamage *= EnrageDamageMultiplier;
                    }
                    
                    // 발사체에 데미지 정보 전달 (태그 사용)
                    Projectile->Tags.Add(FName(*FString::Printf(TEXT("Damage:%f"), FinalDamage)));
                }
            }
        }
    }
}

// 광역 패턴 실행
void AHSBossBase::ExecuteAreaPattern(const FHSBossAttackPattern& Pattern)
{
    // 광역 공격 로직
    TArray<FHitResult> HitResults;
    FVector Center = GetActorLocation();
    
    // 구체 충돌 검사
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(Pattern.Range);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    GetWorld()->SweepMultiByChannel(
        HitResults,
        Center,
        Center,
        FQuat::Identity,
        ECC_Pawn,
        SphereShape,
        QueryParams
    );
    
    // 범위 내 모든 플레이어에게 데미지
    for (const FHitResult& Hit : HitResults)
    {
        if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(Hit.GetActor()))
        {
            float FinalDamage = Pattern.Damage * PhaseDamageMultipliers[CurrentPhase];
            if (bIsEnraged)
            {
                FinalDamage *= EnrageDamageMultiplier;
            }
            
            // 데미지 적용
            FPointDamageEvent DamageEvent(FinalDamage, Hit, GetActorLocation(), nullptr);
            
            Player->TakeDamage(FinalDamage, DamageEvent, GetController(), this);
        }
    }
    
    // 시각 효과
    if (Pattern.VFXTemplate)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            this,
            Pattern.VFXTemplate,
            Center,
            FRotator::ZeroRotator,
            FVector(Pattern.Range / 100.0f)
        );
    }
}

// 특수 패턴 실행
void AHSBossBase::ExecuteSpecialPattern(const FHSBossAttackPattern& Pattern)
{
    // 특수 패턴은 보스별로 다르게 구현
    // 기본적으로는 환경 위험 요소 생성
    TriggerEnvironmentalHazard();
}

// 궁극기 패턴 실행
void AHSBossBase::ExecuteUltimatePattern(const FHSBossAttackPattern& Pattern)
{
    // 궁극기는 매우 강력한 공격
    // 모든 플레이어에게 경고
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, 
            FString::Printf(TEXT("%s is casting ultimate ability!"), *BossTitle));
    }
    
    // 궁극기 효과는 보스별로 구현
}

// 패턴 활성화 완료
void AHSBossBase::OnPatternActivationComplete()
{
    // 패턴 종료 처리
    bIsExecutingPattern = false;
    
    OnBossPatternEnd.Broadcast(CurrentPattern);
    
    // 쿨다운 타이머 시작
    float CooldownTime = CurrentPattern.Cooldown * AbilityCooldownMultiplier;
    GetWorld()->GetTimerManager().SetTimer(
        PatternCooldownTimer,
        this,
        &AHSBossBase::OnPatternCooldownComplete,
        CooldownTime,
        false
    );
}

// 패턴 쿨다운 완료
void AHSBossBase::OnPatternCooldownComplete()
{
    // 쿨다운 완료 - 다음 패턴 실행 가능
    GetWorld()->GetTimerManager().ClearTimer(PatternCooldownTimer);
}

// 여러 플레이어 감지
void AHSBossBase::OnMultiplePlayersDetected(const TArray<AActor*>& Players)
{
    // 협동 메커니즘 활성화
    EngagedPlayers = Players;
    
    // 협동 필수 패턴 활성화
    TriggerCoopMechanic();
}

// 협동 메커니즘 실행
void AHSBossBase::TriggerCoopMechanic()
{
    // 플레이어 수에 따른 특수 메커니즘
    int32 PlayerCount = GetActivePlayerCount();
    
    if (PlayerCount >= MinPlayersForCoopMechanic)
    {
        // 협동 보상: 받는 데미지 감소
        DamageResistance = FMath::Min(DamageResistance + CoopDamageReduction, 0.8f);
        
        // 특수 협동 패턴 활성화
        for (FHSBossAttackPattern& Pattern : AttackPatterns)
        {
            if (Pattern.bRequiresMultiplePlayers)
            {
                // 협동 패턴 가중치 증가
                PatternWeights[Pattern.PatternName] *= 2.0f;
            }
        }
    }
}

// 활성 플레이어 수 확인
int32 AHSBossBase::GetActivePlayerCount() const
{
    int32 Count = 0;
    for (AActor* Player : EngagedPlayers)
    {
        if (IsValid(Player))
        {
            if (AHSEnemyBase* Enemy = Cast<AHSEnemyBase>(Player))
            {
                if (!Enemy->IsDead())
                {
                    Count++;
                }
            }
            else if (AHSPlayerCharacter* PlayerChar = Cast<AHSPlayerCharacter>(Player))
            {
                // 플레이어 캐릭터는 일단 살아있다고 가정
                Count++;
            }
        }
    }
    return Count;
}

// 교전 중인 플레이어 업데이트
void AHSBossBase::UpdateEngagedPlayers()
{
    // 유효하지 않은 플레이어 제거
    EngagedPlayers.RemoveAll([this](AActor* Player) {
        if (!IsValid(Player))
            return true;
            
        // 거리 체크
        float Distance = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
        if (Distance > AggroRange)
            return true;
            
        // 사망 체크 (플레이어는 일단 살아있다고 가정)
        if (AHSEnemyBase* Enemy = Cast<AHSEnemyBase>(Player))
        {
            if (Enemy->IsDead())
                return true;
        }
        
        return false;
    });
    
    // 범위 내 새로운 플레이어 추가
    TArray<AActor*> NearbyPlayers = FindNearbyPlayers();
    for (AActor* Player : NearbyPlayers)
    {
        if (!EngagedPlayers.Contains(Player))
        {
            EngagedPlayers.Add(Player);
        }
    }
}

// 근처 플레이어 찾기
TArray<AActor*> AHSBossBase::FindNearbyPlayers() const
{
    TArray<AActor*> Players;
    TArray<FHitResult> HitResults;
    
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(AggroRange);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    GetWorld()->SweepMultiByChannel(
        HitResults,
        GetActorLocation(),
        GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        SphereShape,
        QueryParams
    );
    
    for (const FHitResult& Hit : HitResults)
    {
        if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(Hit.GetActor()))
        {
            Players.Add(Player);
        }
    }
    
    return Players;
}

// 특수 능력 활성화
void AHSBossBase::ActivateSpecialAbility(FName AbilityName)
{
    if (SpecialAbilities.Contains(AbilityName))
    {
        SpecialAbilities[AbilityName] = true;
        
        // 능력별 처리
        if (AbilityName == TEXT("PhaseShield"))
        {
            // 페이즈 실드 - 일시적 무적
            DamageResistance = 1.0f;
            
            // 5초 후 해제
            FTimerHandle ShieldTimer;
            GetWorld()->GetTimerManager().SetTimer(ShieldTimer, [this]() {
                DamageResistance = 0.3f;
                SpecialAbilities[TEXT("PhaseShield")] = false;
            }, 5.0f, false);
        }
        else if (AbilityName == TEXT("DamageReflection"))
        {
            // 데미지 반사는 TakeDamage에서 처리
        }
        else if (AbilityName == TEXT("Summon"))
        {
            // 부하 소환
            SpawnMinions();
        }
        else if (AbilityName == TEXT("AreaDenial"))
        {
            // 지역 봉쇄
            TriggerEnvironmentalHazard();
        }
    }
}

// 특수 능력 비활성화
void AHSBossBase::DeactivateSpecialAbility(FName AbilityName)
{
    if (SpecialAbilities.Contains(AbilityName))
    {
        SpecialAbilities[AbilityName] = false;
    }
}

// 부하 소환
void AHSBossBase::SpawnMinions()
{
    // 소환 위치 계산
    int32 MinionCount = FMath::RandRange(3, 5);
    float AngleStep = 360.0f / MinionCount;
    
    // 소환할 부하 클래스 배열 (블루프린트에서 설정 가능)
    TArray<TSubclassOf<AHSEnemyBase>> MinionClasses;
    MinionClasses.Add(AHSBasicMeleeEnemy::StaticClass());
    MinionClasses.Add(AHSBasicRangedEnemy::StaticClass());
    
    for (int32 i = 0; i < MinionCount; i++)
    {
        float Angle = AngleStep * i + FMath::RandRange(-15.0f, 15.0f); // 약간의 랜덤성 추가
        FVector MinionSpawnPos = GetActorLocation() + FRotator(0, Angle, 0).Vector() * 500.0f;
        MinionSpawnPos.Z = GetActorLocation().Z;
        
        // 스폰 가능 여부 확인 (충돌 체크)
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);
        
        bool bCanSpawn = !GetWorld()->LineTraceSingleByChannel(
            HitResult,
            MinionSpawnPos + FVector(0, 0, 100),
            MinionSpawnPos - FVector(0, 0, 100),
            ECC_WorldStatic,
            QueryParams
        );
        
        if (bCanSpawn && MinionClasses.Num() > 0)
        {
            // 랜덤 부하 클래스 선택
            int32 ClassIndex = FMath::RandRange(0, MinionClasses.Num() - 1);
            TSubclassOf<AHSEnemyBase> MinionClass = MinionClasses[ClassIndex];
            
            // 오브젝트 풀에서 부하 가져오기
            if (AHSObjectPool* ObjectPool = Cast<AHSObjectPool>(UGameplayStatics::GetActorOfClass(GetWorld(), AHSObjectPool::StaticClass())))
            {
                if (AActor* Minion = ObjectPool->GetPooledObject())
                {
                    Minion->SetActorLocation(MinionSpawnPos);
                    Minion->SetActorRotation(FRotator(0, FMath::RandRange(0, 360), 0));
                    
                    // 부하 초기화
                    if (AHSEnemyBase* Enemy = Cast<AHSEnemyBase>(Minion))
                    {
                        // 보스의 부하로 설정
                        Enemy->SetEnemyRank(EHSEnemyRank::Minion);
                        Enemy->SetEnemyName(FString::Printf(TEXT("%s's Minion"), *BossTitle));
                        
                        // 체력 및 데미지 감소 (부하는 약함)
                        Enemy->SetMaxHealth(Enemy->GetMaxHealth() * 0.5f);
                        Enemy->SetHealth(Enemy->GetMaxHealth());
                        Enemy->SetBaseDamage(Enemy->GetBaseDamage() * 0.5f);
                        
                        // 플레이어 타겟 설정
                        if (CurrentTarget)
                        {
                            Enemy->SetCurrentTarget(CurrentTarget);
                        }
                    }
                    
                    // 소환 효과
                    UGameplayStatics::SpawnEmitterAtLocation(
                        this,
                        nullptr, // 소환 효과는 블루프린트에서 설정
                        MinionSpawnPos,
                        FRotator::ZeroRotator
                    );
                }
            }
        }
    }
    
    // 소환 사운드 재생
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, 
            FString::Printf(TEXT("%s summons %d minions!"), *BossTitle, MinionCount));
    }
}

// 환경 위험 요소 생성
void AHSBossBase::TriggerEnvironmentalHazard()
{
    TArray<FVector> HazardLocations = GetHazardSpawnLocations();
    
    for (const FVector& Location : HazardLocations)
    {
        SpawnEnvironmentalHazard(Location);
    }
}

// 환경 위험 요소 스폰
void AHSBossBase::SpawnEnvironmentalHazard(const FVector& Location)
{
    if (EnvironmentalHazardClasses.Num() > 0)
    {
        // 랜덤 위험 요소 선택
        int32 RandomIndex = FMath::RandRange(0, EnvironmentalHazardClasses.Num() - 1);
        TSubclassOf<AActor> HazardClass = EnvironmentalHazardClasses[RandomIndex];
        
        if (HazardClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = GetInstigator();
            
            AActor* Hazard = GetWorld()->SpawnActor<AActor>(
                HazardClass,
                Location,
                FRotator::ZeroRotator,
                SpawnParams
            );
            
            if (Hazard)
            {
                CachedEnvironmentalHazards.Add(Hazard);
            }
        }
    }
}

// 위험 요소 스폰 위치 계산
TArray<FVector> AHSBossBase::GetHazardSpawnLocations() const
{
    TArray<FVector> Locations;
    
    // 보스 주변 원형으로 배치
    int32 HazardCount = FMath::RandRange(4, 8);
    float AngleStep = 360.0f / HazardCount;
    
    for (int32 i = 0; i < HazardCount; i++)
    {
        float Angle = AngleStep * i + FMath::FRandRange(-20.0f, 20.0f);
        float Distance = FMath::FRandRange(HazardSpawnRadius * 0.5f, HazardSpawnRadius);
        
        FVector HazardLocation = GetActorLocation() + FRotator(0, Angle, 0).Vector() * Distance;
        HazardLocation.Z = GetActorLocation().Z;
        
        Locations.Add(HazardLocation);
    }
    
    return Locations;
}

// 환경 오브젝트 파괴
void AHSBossBase::DestroyEnvironmentalObject(AActor* Object)
{
    if (Object && CachedEnvironmentalHazards.Contains(Object))
    {
        CachedEnvironmentalHazards.Remove(Object);
        Object->Destroy();
    }
}

// 분노 모드 진입
void AHSBossBase::EnterEnrageMode(float Duration)
{
    if (bIsEnraged)
        return;
        
    bIsEnraged = true;
    
    // 분노 효과 적용
    ApplyEnrageEffects();
    
    // 분노 이벤트
    OnBossEnraged.Broadcast(Duration);
    
    // 지속 시간이 설정된 경우 타이머 설정
    if (Duration > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            EnrageTimer,
            this,
            &AHSBossBase::ExitEnrageMode,
            Duration,
            false
        );
    }
}

// 분노 모드 종료
void AHSBossBase::ExitEnrageMode()
{
    if (!bIsEnraged)
        return;
        
    bIsEnraged = false;
    
    // 분노 효과 제거
    RemoveEnrageEffects();
    
    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(EnrageTimer);
}

// 분노 효과 적용
void AHSBossBase::ApplyEnrageEffects()
{
    // 이동 속도 증가
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed *= EnrageSpeedMultiplier;
    }
    
    // 공격 속도 증가
    AbilityCooldownMultiplier = 0.5f;
    
    // 시각 효과
    if (EnrageEffect)
    {
        EnrageEffect->Activate(true);
    }
    
    // 크기 증가 효과
    SetActorScale3D(GetActorScale3D() * 1.2f);
}

// 분노 효과 제거
void AHSBossBase::RemoveEnrageEffects()
{
    // 이동 속도 복구
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed /= EnrageSpeedMultiplier;
    }
    
    // 공격 속도 복구
    AbilityCooldownMultiplier = 1.0f;
    
    // 시각 효과 제거
    if (EnrageEffect)
    {
        EnrageEffect->Deactivate();
    }
    
    // 크기 복구
    SetActorScale3D(GetActorScale3D() / 1.2f);
}

// 표준 데미지 받기 오버라이드
float AHSBossBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 커스텀 데미지 처리로 전달
    FHSDamageInfo CustomDamageInfo;
    CustomDamageInfo.BaseDamage = DamageAmount;
    CustomDamageInfo.DamageType = EHSDamageType::Physical;
    
    TakeDamageCustom(DamageAmount, CustomDamageInfo, DamageCauser);
    
    // 부모 클래스 호출
    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

// 커스텀 데미지 처리
void AHSBossBase::TakeDamageCustom(float DamageAmount, const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    // 페이즈 실드 체크
    if (SpecialAbilities[TEXT("PhaseShield")])
    {
        // 실드 활성화 시 데미지 무시
        return;
    }
    
    // 협동 데미지 감소 계산
    float CoopReduction = CalculateCoopDamageReduction();
    
    // 최종 데미지 계산
    float FinalDamage = DamageAmount * (1.0f - DamageResistance) * (1.0f - CoopReduction);
    
    // 크라우드 컨트롤 저항 - 상태 효과로 처리하도록 변경
    // 크라우드 컨트롤은 상태 효과(StatusEffect)로 처리되며, 데미지 타입과는 별개입니다
    // 필요한 경우 DamageInfo.StatusEffects를 확인하여 처리
    for (const FHSStatusEffect& StatusEffect : DamageInfo.StatusEffects)
    {
        if (StatusEffect.EffectType == EHSStatusEffectType::Stun || 
            StatusEffect.EffectType == EHSStatusEffectType::Slow)
        {
            // 크라우드 컨트롤 저항 적용 (상태 효과의 지속 시간 감소)
            // 이 부분은 실제 상태 효과 적용 시스템에서 처리되어야 함
        }
    }
    
    // 데미지 반사 체크
    if (SpecialAbilities[TEXT("DamageReflection")] && DamageInstigator)
    {
        if (AHSCharacterBase* Attacker = Cast<AHSCharacterBase>(DamageInstigator))
        {
            FPointDamageEvent ReflectDamageEvent(FinalDamage * 0.3f, FHitResult(), GetActorLocation(), nullptr);
            Attacker->TakeDamage(ReflectDamageEvent.Damage, ReflectDamageEvent, GetController(), this);
        }
    }
    
    // 체력 감소 처리
    SetHealth(GetHealth() - FinalDamage);
    
    // 위협 수준 업데이트
    if (DamageInstigator)
    {
        PlayerThreatLevels.FindOrAdd(DamageInstigator) += FinalDamage * ThreatMultiplier;
    }
    
    // 보스 체력 변경 이벤트
    OnBossHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}

// 체력 설정 오버라이드
void AHSBossBase::SetHealth(float NewHealth)
{
    Super::SetHealth(NewHealth);
    
    // 보스 체력 변경 이벤트
    OnBossHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}

// 협동 데미지 감소 계산
float AHSBossBase::CalculateCoopDamageReduction() const
{
    int32 PlayerCount = GetActivePlayerCount();
    
    if (PlayerCount >= MinPlayersForCoopMechanic)
    {
        // 플레이어가 많을수록 데미지 감소량 증가
        return FMath::Min(CoopDamageReduction * (PlayerCount - 1), 0.7f);
    }
    
    return 0.0f;
}

// 보스 UI 업데이트
void AHSBossBase::UpdateBossUI()
{
    // UI 위젯이 있는 경우 업데이트
    // 실제 구현은 위젯 블루프린트에서 처리
}

// 패턴 가중치 업데이트
void AHSBossBase::UpdatePatternWeights()
{
    // 페이즈에 따른 패턴 가중치 조정
    for (auto& PatternWeight : PatternWeights)
    {
        // 기본 가중치로 리셋
        PatternWeight.Value = 1.0f;
    }
    
    // 페이즈별 가중치 조정
    switch (CurrentPhase)
    {
        case EHSBossPhase::Phase2:
            // 원거리 패턴 선호
            for (const FHSBossAttackPattern& Pattern : AttackPatterns)
            {
                if (Pattern.PatternType == EHSBossPatternType::Ranged)
                {
                    PatternWeights[Pattern.PatternName] *= 1.5f;
                }
            }
            break;
            
        case EHSBossPhase::Phase3:
            // 광역 패턴 선호
            for (const FHSBossAttackPattern& Pattern : AttackPatterns)
            {
                if (Pattern.PatternType == EHSBossPatternType::Area)
                {
                    PatternWeights[Pattern.PatternName] *= 2.0f;
                }
            }
            break;
            
        case EHSBossPhase::Final:
            // 궁극기 패턴 선호
            for (const FHSBossAttackPattern& Pattern : AttackPatterns)
            {
                if (Pattern.PatternType == EHSBossPatternType::Ultimate)
                {
                    PatternWeights[Pattern.PatternName] *= 3.0f;
                }
            }
            break;
    }
}

// 사망 처리
void AHSBossBase::Die()
{
    // 보상 분배
    DistributeRewards();
    
    // 환경 위험 요소 제거
    for (AActor* Hazard : CachedEnvironmentalHazards)
    {
        if (IsValid(Hazard))
        {
            Hazard->Destroy();
        }
    }
    CachedEnvironmentalHazards.Empty();
    
    // 보스 사망 이벤트
    OnEnemyDeath.Broadcast(this);
    
    // 부모 클래스 호출
    Super::Die();
}

// 보상 분배
void AHSBossBase::DistributeRewards()
{
    // 경험치 부여
    GrantExperienceToPlayers();
    
    // 아이템 드롭
    SpawnLoot();
}

// 플레이어들에게 경험치 부여
void AHSBossBase::GrantExperienceToPlayers()
{
    for (AActor* Player : EngagedPlayers)
    {
        if (IsValid(Player))
        {
            // 실제 경험치 부여는 플레이어의 경험치 시스템에서 처리
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                    FString::Printf(TEXT("Player receives %f experience!"), ExperienceReward));
            }
        }
    }
}

// 전리품 생성
void AHSBossBase::SpawnLoot()
{
    // 보장 드롭 아이템 생성
    for (TSubclassOf<AHSItemBase> ItemClass : GuaranteedDrops)
    {
        if (ItemClass)
        {
            FVector LootSpawnLocation = GetActorLocation() + FVector(
                FMath::RandRange(-200.0f, 200.0f),
                FMath::RandRange(-200.0f, 200.0f),
                100.0f
            );
            
            FActorSpawnParameters SpawnParams;
            GetWorld()->SpawnActor<AActor>(ItemClass, LootSpawnLocation, FRotator::ZeroRotator, SpawnParams);
        }
    }
    
    // 화폐 보상 (UI 알림으로 처리)
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            FString::Printf(TEXT("Boss dropped %f gold!"), CurrencyReward));
    }
}

// 위협 수준 계산
float AHSBossBase::CalculateThreatLevel(AActor* Player) const
{
    if (PlayerThreatLevels.Contains(Player))
    {
        return PlayerThreatLevels[Player];
    }
    return 0.0f;
}

// 분노 타이머 만료
void AHSBossBase::OnEnrageExpired()
{
    ExitEnrageMode();
}
