// HuntingSpirit Game - Object Pool Implementation

#include "HSObjectPool.h"

// 생성자
AHSObjectPool::AHSObjectPool()
{
    // Tick 활성화하지 않음 (필요한 경우 활성화)
    PrimaryActorTick.bCanEverTick = false;
    
    // 기본값 설정
    PoolSize = 10;
    bGrowWhenFull = true;
    MaxPoolSize = 100;
}

// 게임 시작 시 호출
void AHSObjectPool::BeginPlay()
{
    Super::BeginPlay();
    
    // 풀 초기화 - 기본 클래스가 설정되어 있는 경우만
    if (PooledObjectClass)
    {
        InitializePool(PooledObjectClass, PoolSize, GetWorld());
    }
}

// 풀에서 오브젝트 가져오기
AActor* AHSObjectPool::GetPooledObject()
{
    // 풀에 비활성화된 오브젝트가 있는지 확인
    if (InactivePool.Num() > 0)
    {
        // 마지막 오브젝트 가져오기 (스택처럼 사용)
        AActor* PooledObject = InactivePool.Last();
        InactivePool.RemoveAt(InactivePool.Num() - 1);
        
        // 활성화 상태로 이동
        ActivePool.Add(PooledObject);
        
        // 오브젝트가 인터페이스를 구현하는지 확인하고 활성화 함수 호출
        if (PooledObject->GetClass()->ImplementsInterface(UHSPoolableObject::StaticClass()))
        {
            IHSPoolableObject::Execute_OnActivated(PooledObject);
        }
        
        return PooledObject;
    }
    
    // 풀이 비어있고 확장 가능한 경우
    if (bGrowWhenFull && (MaxPoolSize <= 0 || ActivePool.Num() + InactivePool.Num() < MaxPoolSize))
    {
        // 새 오브젝트 생성 후 바로 활성화 상태로 이동
        AActor* NewObject = CreateNewPooledObject();
        ActivePool.Add(NewObject);
        
        // 오브젝트가 인터페이스를 구현하는지 확인하고 활성화 함수 호출
        if (NewObject->GetClass()->ImplementsInterface(UHSPoolableObject::StaticClass()))
        {
            IHSPoolableObject::Execute_OnActivated(NewObject);
        }
        
        return NewObject;
    }
    
    // 풀이 비어있고 확장 불가능한 경우 nullptr 반환
    return nullptr;
}

// 오브젝트를 풀에 반환
void AHSObjectPool::ReturnObjectToPool(AActor* ObjectToReturn)
{
    if (!ObjectToReturn)
    {
        return;
    }
    
    // 활성화 목록에서 제거
    ActivePool.Remove(ObjectToReturn);
    
    // 비활성화 목록에 추가
    InactivePool.Add(ObjectToReturn);
    
    // 오브젝트 비활성화
    ObjectToReturn->SetActorHiddenInGame(true);
    ObjectToReturn->SetActorEnableCollision(false);
    ObjectToReturn->SetActorTickEnabled(false);
    
    // 인터페이스 비활성화 함수 호출
    if (ObjectToReturn->GetClass()->ImplementsInterface(UHSPoolableObject::StaticClass()))
    {
        IHSPoolableObject::Execute_OnDeactivated(ObjectToReturn);
    }
}

// 특정 위치에서 풀링된 오브젝트 스폰
AActor* AHSObjectPool::SpawnPooledObject(FVector Location, FRotator Rotation)
{
    // 풀에서 오브젝트 가져오기
    AActor* PooledObject = GetPooledObject();
    
    if (PooledObject)
    {
        // 위치와 회전 설정
        PooledObject->SetActorLocationAndRotation(Location, Rotation);
        
        // 오브젝트 활성화
        PooledObject->SetActorHiddenInGame(false);
        PooledObject->SetActorEnableCollision(true);
        PooledObject->SetActorTickEnabled(true);
    }
    
    return PooledObject;
}

// 오브젝트 풀 초기화
void AHSObjectPool::InitializePool(TSubclassOf<AActor> InPooledObjectClass, int32 InitialSize, UWorld* InWorld)
{
    // 클래스 설정
    PooledObjectClass = InPooledObjectClass;
    
    // 클래스가 지정되지 않은 경우 초기화하지 않음
    if (!PooledObjectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot initialize object pool: PooledObjectClass is not set."));
        return;
    }
    
    // 모든 풀링된 오브젝트 제거
    for (AActor* Actor : InactivePool)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }
    
    for (AActor* Actor : ActivePool)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }
    
    InactivePool.Empty();
    ActivePool.Empty();
    
    // 새 오브젝트로 풀 채우기
    for (int32 i = 0; i < InitialSize; ++i)
    {
        AActor* NewObject = CreateNewPooledObject();
        if (NewObject)
        {
            InactivePool.Add(NewObject);
        }
    }
}

// 새 오브젝트 생성
AActor* AHSObjectPool::CreateNewPooledObject()
{
    if (!PooledObjectClass || !GetWorld())
    {
        return nullptr;
    }
    
    // 스폰 매개변수 설정
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = this;
    
    // 새 액터 스폰
    AActor* NewObject = GetWorld()->SpawnActor<AActor>(PooledObjectClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    
    if (NewObject)
    {
        // 기본 상태를 비활성화로 설정
        NewObject->SetActorHiddenInGame(true);
        NewObject->SetActorEnableCollision(false);
        NewObject->SetActorTickEnabled(false);
        
        // 초기화 함수 호출
        if (NewObject->GetClass()->ImplementsInterface(UHSPoolableObject::StaticClass()))
        {
            IHSPoolableObject::Execute_OnCreated(NewObject);
        }
    }
    
    return NewObject;
}
