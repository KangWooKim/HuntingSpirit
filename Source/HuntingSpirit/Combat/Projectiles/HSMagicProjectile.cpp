// HuntingSpirit Game - Magic Projectile Implementation
// 마법 발사체 클래스 구현

#include "HSMagicProjectile.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HuntingSpirit/Combat/HSCombatComponent.h"
#include "HuntingSpirit/Optimization/ObjectPool/HSObjectPool.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 생성자
AHSMagicProjectile::AHSMagicProjectile()
{
    // 매 프레임 틱 활성화
    PrimaryActorTick.bCanEverTick = true;

    // 루트 컴포넌트로 구체 충돌체 생성
    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    RootComponent = CollisionComponent;
    CollisionComponent->SetSphereRadius(5.0f);
    
    // 충돌 설정
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
    
    // 충돌 이벤트 바인딩
    CollisionComponent->OnComponentHit.AddDynamic(this, &AHSMagicProjectile::OnHit);

    // 발사체 메시 컴포넌트 생성
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 발사체 이동 컴포넌트 생성
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1000.0f;
    ProjectileMovement->MaxSpeed = 1000.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.0f; // 마법 발사체는 중력 영향 없음

    // 트레일 파티클 컴포넌트 생성
    TrailParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("TrailParticles"));
    TrailParticles->SetupAttachment(RootComponent);

    // 기본값 설정
    ProjectileDamage = 25.0f;
    LifeSpan = 5.0f;
    CurrentLifeTime = 0.0f;
    OwnerPool = nullptr;

    // 기본 수명 설정 (백업용)
    SetLifeSpan(LifeSpan);
}

// 게임 시작 시 호출
void AHSMagicProjectile::BeginPlay()
{
    Super::BeginPlay();
    
    // 현재 수명 초기화
    CurrentLifeTime = 0.0f;
}

// 매 프레임 호출
void AHSMagicProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 수명 관리
    CurrentLifeTime += DeltaTime;
    if (CurrentLifeTime >= LifeSpan)
    {
        // 풀에 반환하거나 제거
        if (OwnerPool)
        {
            OwnerPool->ReturnObjectToPool(this);
        }
        else
        {
            Destroy();
        }
    }
}

// 오브젝트 풀에서 활성화될 때 호출
void AHSMagicProjectile::OnActivated_Implementation()
{
    // 발사체 활성화
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    
    // 이동 컴포넌트 활성화
    if (ProjectileMovement)
    {
        ProjectileMovement->SetActive(true);
    }
    
    // 파티클 활성화
    if (TrailParticles)
    {
        TrailParticles->SetActive(true);
    }
    
    // 수명 초기화
    CurrentLifeTime = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("HSMagicProjectile: Activated"));
}

// 오브젝트 풀에서 비활성화될 때 호출
void AHSMagicProjectile::OnDeactivated_Implementation()
{
    // 발사체 비활성화
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    
    // 이동 컴포넌트 정지
    if (ProjectileMovement)
    {
        ProjectileMovement->SetActive(false);
        ProjectileMovement->Velocity = FVector::ZeroVector;
    }
    
    // 파티클 비활성화
    if (TrailParticles)
    {
        TrailParticles->SetActive(false);
    }
    
    // 위치 초기화
    SetActorLocation(FVector::ZeroVector);
    SetActorRotation(FRotator::ZeroRotator);
    
    UE_LOG(LogTemp, Log, TEXT("HSMagicProjectile: Deactivated"));
}

// 오브젝트 풀에서 생성될 때 호출
void AHSMagicProjectile::OnCreated_Implementation()
{
    // 생성 시 초기화 작업
    CurrentLifeTime = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("HSMagicProjectile: Created in pool"));
}

// 발사체 초기화
void AHSMagicProjectile::InitializeProjectile(FVector Direction, float Speed, float Damage)
{
    // 매개변수 검증
    if (Direction.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMagicProjectile: Invalid direction vector provided"));
        Direction = FVector::ForwardVector; // 기본값 사용
    }

    if (Speed <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("HSMagicProjectile: Invalid speed provided: %f"), Speed);
        Speed = 1000.0f; // 기본값 사용
    }

    // 발사 방향 정규화
    FVector NormalizedDirection = Direction.GetSafeNormal();
    
    // 발사체 회전 설정 (발사 방향으로 향하도록)
    FRotator ProjectileRotation = NormalizedDirection.Rotation();
    SetActorRotation(ProjectileRotation);
    
    // 발사체 이동 설정
    if (ProjectileMovement)
    {
        ProjectileMovement->InitialSpeed = Speed;
        ProjectileMovement->MaxSpeed = Speed;
        ProjectileMovement->Velocity = NormalizedDirection * Speed;
    }
    
    // 데미지 설정
    ProjectileDamage = FMath::Max(0.0f, Damage);
    
    // 수명 초기화
    CurrentLifeTime = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("HSMagicProjectile: Initialized - Direction: %s, Speed: %f, Damage: %f"), 
           *NormalizedDirection.ToString(), Speed, ProjectileDamage);
}

// 충돌 시 호출
void AHSMagicProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    // 자기 자신이나 소유자와 충돌하지 않도록 체크
    if (!OtherActor || OtherActor == this || OtherActor == GetOwner())
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("HSMagicProjectile: Hit actor %s"), OtherActor ? *OtherActor->GetName() : TEXT("Unknown"));

    // 적에게 데미지 적용
    UHSCombatComponent* TargetCombat = OtherActor->FindComponentByClass<UHSCombatComponent>();
    if (TargetCombat)
    {
        // 데미지 정보 구성
        FHSDamageInfo DamageInfo;
        DamageInfo.BaseDamage = ProjectileDamage;
        DamageInfo.DamageType = EHSDamageType::Magical;
        DamageInfo.CalculationMode = EHSDamageCalculationMode::Fixed;
        DamageInfo.CriticalChance = 0.1f; // 10% 크리티컬 확률
        DamageInfo.CriticalMultiplier = 1.5f;

        // 데미지 적용
        FHSDamageResult DamageResult = TargetCombat->ApplyDamage(DamageInfo, GetOwner());
        
        UE_LOG(LogTemp, Log, TEXT("HSMagicProjectile: Applied %f damage to %s"), 
               DamageResult.FinalDamage, *OtherActor->GetName());
    }

    // 충돌 이펙트 생성 (향후 파티클 시스템 추가 가능)
    // TODO: 충돌 시 폭발 이펙트 추가
    
    // 발사체 제거 또는 풀에 반환
    if (OwnerPool)
    {
        OwnerPool->ReturnObjectToPool(this);
    }
    else
    {
        Destroy();
    }
}
