// HuntingSpirit Game - Magic Projectile Header
// 마법 발사체 클래스 - 오브젝트 풀링 지원

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HuntingSpirit/Optimization/ObjectPool/HSObjectPool.h"
#include "HuntingSpirit/Combat/Damage/HSDamageType.h"
#include "HSMagicProjectile.generated.h"

// 전방 선언
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UParticleSystemComponent;
class USphereComponent;
class UPrimitiveComponent;
struct FHitResult;

UCLASS()
class HUNTINGSPIRIT_API AHSMagicProjectile : public AActor, public IHSPoolableObject
{
    GENERATED_BODY()
    
public:    
    // 생성자
    AHSMagicProjectile();

    // IHSPoolableObject 인터페이스 구현
    virtual void OnActivated_Implementation() override;
    virtual void OnDeactivated_Implementation() override;
    virtual void OnCreated_Implementation() override;
    
    // 발사체 초기화
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void InitializeProjectile(FVector Direction, float Speed, float Damage);

    // 소속 풀 설정
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void SetOwnerPool(AHSObjectPool* Pool) { OwnerPool = Pool; }

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;
    
    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;
    
    // 충돌 시 호출
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

private:
    // 충돌 컴포넌트 (루트)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    USphereComponent* CollisionComponent;

    // 발사체 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* ProjectileMesh;
    
    // 발사체 이동 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UProjectileMovementComponent* ProjectileMovement;
    
    // 파티클 시스템
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UParticleSystemComponent* TrailParticles;
    
    // 발사체 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    float ProjectileDamage = 25.0f;
    
    // 수명
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    float LifeSpan = 5.0f;
    
    // 현재 수명 타이머
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    float CurrentLifeTime = 0.0f;
    
    // 소속 풀에 대한 참조
    UPROPERTY()
    AHSObjectPool* OwnerPool = nullptr;
};
