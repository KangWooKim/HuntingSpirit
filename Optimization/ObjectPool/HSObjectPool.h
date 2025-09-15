// HuntingSpirit Game - Object Pool Header
// 오브젝트 풀링 시스템 - 자주 생성/삭제되는 객체의 성능 최적화

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSObjectPool.generated.h"

// 풀링 가능한 오브젝트가 구현해야 하는 인터페이스
UINTERFACE(MinimalAPI, Blueprintable)
class UHSPoolableObject : public UInterface
{
    GENERATED_BODY()
};

class HUNTINGSPIRIT_API IHSPoolableObject
{
    GENERATED_BODY()

public:
    // 오브젝트가 풀에서 활성화될 때 호출
    UFUNCTION(BlueprintNativeEvent, Category = "Object Pool")
    void OnActivated();
    virtual void OnActivated_Implementation() {}
    
    // 오브젝트가 풀로 반환될 때 호출
    UFUNCTION(BlueprintNativeEvent, Category = "Object Pool")
    void OnDeactivated();
    virtual void OnDeactivated_Implementation() {}
    
    // 오브젝트가 초기 스폰될 때 호출
    UFUNCTION(BlueprintNativeEvent, Category = "Object Pool")
    void OnCreated();
    virtual void OnCreated_Implementation() {}
};

// 오브젝트 풀 클래스
UCLASS()
class HUNTINGSPIRIT_API AHSObjectPool : public AActor
{
    GENERATED_BODY()
    
public:    
    // 생성자
    AHSObjectPool();

    // 풀에서 오브젝트 가져오기
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    AActor* GetPooledObject();
    
    // 오브젝트를 풀에 반환
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    void ReturnObjectToPool(AActor* ObjectToReturn);
    
    // 특정 위치에서 풀링된 오브젝트 스폰
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    AActor* SpawnPooledObject(FVector Location, FRotator Rotation);
    
    // 오브젝트 풀 초기화
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    void InitializePool(TSubclassOf<AActor> InPooledObjectClass, int32 InitialSize, UWorld* InWorld);
    
    // 풀에서 사용하는 클래스 반환
    UFUNCTION(BlueprintPure, Category = "Object Pool")
    TSubclassOf<AActor> GetPoolClass() const { return PooledObjectClass; }

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;
    
    // 풀에 사용할 액터 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object Pool")
    TSubclassOf<AActor> PooledObjectClass;
    
    // 초기 풀 크기
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object Pool")
    int32 PoolSize;
    
    // 풀이 비었을 때 새 오브젝트 생성 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object Pool")
    bool bGrowWhenFull;
    
    // 최대 풀 크기 (bGrowWhenFull이 true일 때 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object Pool", meta = (EditCondition = "bGrowWhenFull"))
    int32 MaxPoolSize;

private:
    // 비활성화된 오브젝트 목록 (풀)
    UPROPERTY()
    TArray<AActor*> InactivePool;
    
    // 활성화된 오브젝트 목록 (현재 사용 중)
    UPROPERTY()
    TArray<AActor*> ActivePool;
    
    // 새 오브젝트 생성
    AActor* CreateNewPooledObject();
};
