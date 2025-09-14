// HuntingSpirit Game - Character Base Class Header
// 모든 캐릭터(플레이어 및 적)의 기본 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSCharacterBase.generated.h"

class UHSCombatComponent;
class UHSHitReactionComponent;
class UHSStatsComponent;
class UHSInventoryComponent;
class UHSGatheringComponent;

UENUM(BlueprintType)
enum class ECharacterState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Walking UMETA(DisplayName = "Walking"),
    Running UMETA(DisplayName = "Running"),
    Attacking UMETA(DisplayName = "Attacking"),
    Gathering UMETA(DisplayName = "Gathering"),
    Dead UMETA(DisplayName = "Dead")
};

// 체력 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);

UCLASS()
class HUNTINGSPIRIT_API AHSCharacterBase : public ACharacter
{
    GENERATED_BODY()

public:
    // 캐릭터의 생성자
    AHSCharacterBase();

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Called to bind functionality to input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // 캐릭터의 현재 상태 반환
    UFUNCTION(BlueprintCallable, Category = "Character")
    ECharacterState GetCharacterState() const { return CurrentState; }

    // 캐릭터 상태 설정
    UFUNCTION(BlueprintCallable, Category = "Character")
    virtual void SetCharacterState(ECharacterState NewState);

    // 캐릭터 걷기 속도 설정
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    void SetWalkSpeed(float NewSpeed);

    // 캐릭터 달리기 속도 설정
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    void SetRunSpeed(float NewSpeed);

    // 캐릭터 기본 공격 함수
    UFUNCTION(BlueprintCallable, Category = "Character|Combat")
    virtual void PerformBasicAttack();
    
    // 공격 종료 후 호출되는 함수 - animation 몽타주 델리게이트 용
    UFUNCTION()
    virtual void OnAttackEnd(UAnimMontage* Montage = nullptr, bool bInterrupted = false);
    
    // 타이머용 공격 종료 함수 - 파라미터 없음
    UFUNCTION()
    virtual void OnAttackEndTimer();
    
    // 전투 및 피격 반응 컴포넌트 접근자
    UFUNCTION(BlueprintPure, Category = "Character|Combat")
    UHSCombatComponent* GetCombatComponent() const { return CombatComponent; }
    
    UFUNCTION(BlueprintPure, Category = "Character|Combat")
    UHSHitReactionComponent* GetHitReactionComponent() const { return HitReactionComponent; }
    
    UFUNCTION(BlueprintPure, Category = "Character|Stats")
    UHSStatsComponent* GetStatsComponent() const { return StatsComponent; }
    
    UFUNCTION(BlueprintPure, Category = "Character|Inventory")
    UHSInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
    
    UFUNCTION(BlueprintPure, Category = "Character|Gathering")
    UHSGatheringComponent* GetGatheringComponent() const { return GatheringComponent; }
    
    // 팀 ID 관련
    UFUNCTION(BlueprintPure, Category = "Character|Team")
    int32 GetTeamID() const { return TeamID; }
    
    UFUNCTION(BlueprintCallable, Category = "Character|Team")
    void SetTeamID(int32 NewTeamID) { TeamID = NewTeamID; }
    
    // 스태미너 관련 함수
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    float GetStaminaCurrent() const { return StaminaCurrent; }
    
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    float GetStaminaMax() const { return StaminaMax; }
    
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    float GetStaminaPercentage() const { return StaminaMax > 0.0f ? StaminaCurrent / StaminaMax : 0.0f; }
    
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    bool UseStamina(float Amount);
    
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    void RestoreStamina(float Amount);
    
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    bool HasEnoughStamina(float Amount) const { return StaminaCurrent >= Amount; }
    
    // 달리기 토글 기능
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void ToggleSprint();
    
    // 달리기 활성화 여부 확인
    UFUNCTION(BlueprintPure, Category = "Character|Movement")
    bool IsSprintEnabled() const { return bSprintEnabled; }
    
    // 달리기 시작/종료 함수 - virtual로 선언하여 파생 클래스에서 override 가능하도록 함
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void StartSprinting();
    
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void StopSprinting();
    
    // 고유 ID 반환 (네트워킹 및 식별용)
    UFUNCTION(BlueprintPure, Category = "Character")
    int32 GetUniqueID() const { return GetTypeHash(this); }
    
    // 체력 관련 함수들 - 플레이어와 적 모두에서 사용
    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual float GetHealth() const { return Health; }

    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual void SetHealth(float NewHealth);

    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual bool IsDead() const { return CurrentState == ECharacterState::Dead; }
    
    // 체력 변경 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Character Events")
    FOnHealthChanged OnHealthChanged;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // 전투 시스템 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSCombatComponent* CombatComponent;

    // 피격 반응 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSHitReactionComponent* HitReactionComponent;
    
    // 스탯 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSStatsComponent* StatsComponent;
    
    // 인벤토리 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSInventoryComponent* InventoryComponent;
    
    // 채집 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSGatheringComponent* GatheringComponent;
    
    // 팀 ID (0: 중립, 1: 플레이어 팀, 2+: 적 팀)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Team")
    int32 TeamID = 0;

    // 캐릭터 상태 변경 시 호출되는 이벤트 (블루프린트에서 구현할 수 있음)
    UFUNCTION(BlueprintImplementableEvent, Category = "Character")
    void OnCharacterStateChanged(ECharacterState NewState);
    
    // 스태미너 변경 시 호출되는 이벤트 (블루프린트에서 구현할 수 있음)
    UFUNCTION(BlueprintImplementableEvent, Category = "Character")
    void OnStaminaChanged(float NewStamina, float MaxStamina);

    // 캐릭터 현재 상태
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    ECharacterState CurrentState;

    // 걷기 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
    float WalkSpeed;

    // 달리기 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
    float RunSpeed;

    // 기본 공격 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Combat")
    class UAnimMontage* BasicAttackMontage;

    // 기본 공격 지속 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Combat")
    float BasicAttackDuration;
    
    // 스태미너 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaCurrent;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaMax;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaRegenRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float SprintStaminaConsumptionRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaRegenDelay;
    
    // 스태미너 회복 지연 타이머
    FTimerHandle StaminaRegenDelayTimerHandle;
    
    // 달리기 활성화 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Movement")
    bool bSprintEnabled;

    // 공격 쿨다운 타이머 핸들
    FTimerHandle AttackCooldownTimerHandle;
    
    // 스태미너 회복 시작
    UFUNCTION()
    virtual void StartStaminaRegeneration();
    
    // 스태미너 수치 업데이트
    UFUNCTION()
    virtual void UpdateStamina(float DeltaTime);
    
    // 체력 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
    float Health = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
    float MaxHealth = 100.0f;
};
