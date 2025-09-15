/**
 * @file HSCharacterBase.h
 * @brief 모든 캐릭터(플레이어 및 적)의 기본 클래스 정의
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 * @project HuntingSpirit - 협동 로그라이크 RPG
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSCharacterBase.generated.h"

class UHSCombatComponent;
class UHSHitReactionComponent;
class UHSStatsComponent;
class UHSInventoryComponent;
class UHSGatheringComponent;

/**
 * @brief 캐릭터의 현재 상태를 나타내는 열거형
 */
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

/**
 * @brief 체력 변경 시 호출되는 델리게이트
 * @param NewHealth 새로운 체력 값
 * @param MaxHealth 최대 체력 값
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);

/**
 * @brief 모든 캐릭터의 기본 클래스
 * @details 플레이어와 적 캐릭터 모두에서 사용되는 공통 기능을 제공
 */
UCLASS()
class HUNTINGSPIRIT_API AHSCharacterBase : public ACharacter
{
    GENERATED_BODY()

public:
    /**
     * @brief 기본 생성자
     * @details 캐릭터의 기본 컴포넌트들을 초기화
     */
    AHSCharacterBase();

    /**
     * @brief 매 프레임마다 호출되는 틱 함수
     * @param DeltaTime 이전 프레임과의 시간 차이
     */
    virtual void Tick(float DeltaTime) override;

    /**
     * @brief 입력 컴포넌트에 기능을 바인딩
     * @param PlayerInputComponent 플레이어 입력 컴포넌트
     */
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /**
     * @brief 캐릭터의 현재 상태를 반환
     * @return 현재 캐릭터 상태
     */
    UFUNCTION(BlueprintCallable, Category = "Character")
    ECharacterState GetCharacterState() const { return CurrentState; }

    /**
     * @brief 캐릭터 상태를 설정
     * @param NewState 새로운 캐릭터 상태
     */
    UFUNCTION(BlueprintCallable, Category = "Character")
    virtual void SetCharacterState(ECharacterState NewState);

    /**
     * @brief 캐릭터의 걷기 속도를 설정
     * @param NewSpeed 새로운 걷기 속도
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    void SetWalkSpeed(float NewSpeed);

    /**
     * @brief 캐릭터의 달리기 속도를 설정
     * @param NewSpeed 새로운 달리기 속도
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    void SetRunSpeed(float NewSpeed);

    /**
     * @brief 기본 공격을 수행
     * @details 스태미너 소모 후 공격 애니메이션 재생
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Combat")
    virtual void PerformBasicAttack();
    
    /**
     * @brief 공격 종료 후 호출되는 콜백 함수 (애니메이션 몽타주용)
     * @param Montage 종료된 애니메이션 몽타주
     * @param bInterrupted 애니메이션이 중단되었는지 여부
     */
    UFUNCTION()
    virtual void OnAttackEnd(UAnimMontage* Montage = nullptr, bool bInterrupted = false);
    
    /**
     * @brief 타이머용 공격 종료 함수
     * @details 파라미터 없이 호출되는 버전
     */
    UFUNCTION()
    virtual void OnAttackEndTimer();
    
    /**
     * @brief 전투 컴포넌트를 반환
     * @return 전투 컴포넌트 포인터
     */
    UFUNCTION(BlueprintPure, Category = "Character|Combat")
    UHSCombatComponent* GetCombatComponent() const { return CombatComponent; }
    
    /**
     * @brief 피격 반응 컴포넌트를 반환
     * @return 피격 반응 컴포넌트 포인터
     */
    UFUNCTION(BlueprintPure, Category = "Character|Combat")
    UHSHitReactionComponent* GetHitReactionComponent() const { return HitReactionComponent; }
    
    /**
     * @brief 스탯 컴포넌트를 반환
     * @return 스탯 컴포넌트 포인터
     */
    UFUNCTION(BlueprintPure, Category = "Character|Stats")
    UHSStatsComponent* GetStatsComponent() const { return StatsComponent; }
    
    /**
     * @brief 인벤토리 컴포넌트를 반환
     * @return 인벤토리 컴포넌트 포인터
     */
    UFUNCTION(BlueprintPure, Category = "Character|Inventory")
    UHSInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
    
    /**
     * @brief 채집 컴포넌트를 반환
     * @return 채집 컴포넌트 포인터
     */
    UFUNCTION(BlueprintPure, Category = "Character|Gathering")
    UHSGatheringComponent* GetGatheringComponent() const { return GatheringComponent; }
    
    /**
     * @brief 팀 ID를 반환
     * @return 현재 팀 ID (0: 중립, 1: 플레이어 팀, 2+: 적 팀)
     */
    UFUNCTION(BlueprintPure, Category = "Character|Team")
    int32 GetTeamID() const { return TeamID; }
    
    /**
     * @brief 팀 ID를 설정
     * @param NewTeamID 새로운 팀 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Team")
    void SetTeamID(int32 NewTeamID) { TeamID = NewTeamID; }
    
    /**
     * @brief 현재 스태미너 값을 반환
     * @return 현재 스태미너 값
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    float GetStaminaCurrent() const { return StaminaCurrent; }
    
    /**
     * @brief 최대 스태미너 값을 반환
     * @return 최대 스태미너 값
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    float GetStaminaMax() const { return StaminaMax; }
    
    /**
     * @brief 스태미너 백분율을 반환
     * @return 스태미너 백분율 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    float GetStaminaPercentage() const { return StaminaMax > 0.0f ? StaminaCurrent / StaminaMax : 0.0f; }
    
    /**
     * @brief 스태미너를 소모
     * @param Amount 소모할 스태미너 양
     * @return 소모 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    bool UseStamina(float Amount);
    
    /**
     * @brief 스태미너를 회복
     * @param Amount 회복할 스태미너 양
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    void RestoreStamina(float Amount);
    
    /**
     * @brief 스태미너가 충분한지 확인
     * @param Amount 필요한 스태미너 양
     * @return 스태미너 충분 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Stamina")
    bool HasEnoughStamina(float Amount) const { return StaminaCurrent >= Amount; }
    
    /**
     * @brief 달리기 상태를 토글
     * @details 달리기와 걷기 상태를 전환
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void ToggleSprint();
    
    /**
     * @brief 달리기 활성화 여부를 확인
     * @return 달리기 활성화 상태
     */
    UFUNCTION(BlueprintPure, Category = "Character|Movement")
    bool IsSprintEnabled() const { return bSprintEnabled; }
    
    /**
     * @brief 달리기를 시작
     * @details 파생 클래스에서 오버라이드 가능
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void StartSprinting();
    
    /**
     * @brief 달리기를 종료
     * @details 파생 클래스에서 오버라이드 가능
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void StopSprinting();
    
    /**
     * @brief 고유 ID를 반환
     * @return 네트워킹 및 식별용 고유 ID
     */
    UFUNCTION(BlueprintPure, Category = "Character")
    int32 GetUniqueID() const { return GetTypeHash(this); }
    
    /**
     * @brief 현재 체력을 반환
     * @return 현재 체력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual float GetHealth() const { return Health; }

    /**
     * @brief 최대 체력을 반환
     * @return 최대 체력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual float GetMaxHealth() const { return MaxHealth; }

    /**
     * @brief 체력을 설정
     * @param NewHealth 새로운 체력 값
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual void SetHealth(float NewHealth);

    /**
     * @brief 체력 백분율을 반환
     * @return 체력 백분율 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

    /**
     * @brief 캐릭터가 죽었는지 확인
     * @return 사망 상태 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Character|Health")
    virtual bool IsDead() const { return CurrentState == ECharacterState::Dead; }
    
    /**
     * @brief 체력 변경 시 호출되는 이벤트
     */
    UPROPERTY(BlueprintAssignable, Category = "Character Events")
    FOnHealthChanged OnHealthChanged;

protected:
    /**
     * @brief 게임 시작 시 호출되는 함수
     */
    virtual void BeginPlay() override;

    /**
     * @brief 전투 시스템 컴포넌트
     * @details 공격, 데미지 처리 등 전투 관련 기능 제공
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSCombatComponent* CombatComponent;

    /**
     * @brief 피격 반응 컴포넌트
     * @details 피격 시 반응 효과 처리
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSHitReactionComponent* HitReactionComponent;
    
    /**
     * @brief 스탯 컴포넌트
     * @details 캐릭터의 능력치 관리
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSStatsComponent* StatsComponent;
    
    /**
     * @brief 인벤토리 컴포넌트
     * @details 아이템 보관 및 관리
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSInventoryComponent* InventoryComponent;
    
    /**
     * @brief 채집 컴포넌트
     * @details 자원 채집 기능 제공
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHSGatheringComponent* GatheringComponent;
    
    /**
     * @brief 팀 ID
     * @details 0: 중립, 1: 플레이어 팀, 2+: 적 팀
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Team")
    int32 TeamID = 0;

    /**
     * @brief 캐릭터 상태 변경 시 호출되는 블루프린트 이벤트
     * @param NewState 새로운 캐릭터 상태
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Character")
    void OnCharacterStateChanged(ECharacterState NewState);
    
    /**
     * @brief 스태미너 변경 시 호출되는 블루프린트 이벤트
     * @param NewStamina 새로운 스태미너 값
     * @param MaxStamina 최대 스태미너 값
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Character")
    void OnStaminaChanged(float NewStamina, float MaxStamina);

    /**
     * @brief 캐릭터의 현재 상태
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    ECharacterState CurrentState;

    /**
     * @brief 걷기 속도
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
    float WalkSpeed;

    /**
     * @brief 달리기 속도
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
    float RunSpeed;

    /**
     * @brief 기본 공격 애니메이션 몽타주
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Combat")
    class UAnimMontage* BasicAttackMontage;

    /**
     * @brief 기본 공격 지속 시간
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Combat")
    float BasicAttackDuration;
    
    /**
     * @brief 현재 스태미너 값
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaCurrent;
    
    /**
     * @brief 최대 스태미너 값
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaMax;
    
    /**
     * @brief 스태미너 재생 속도 (초당 회복량)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaRegenRate;
    
    /**
     * @brief 달리기 시 스태미너 소모율 (초당 소모량)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float SprintStaminaConsumptionRate;
    
    /**
     * @brief 스태미너 재생 지연 시간
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Stamina")
    float StaminaRegenDelay;
    
    /**
     * @brief 스태미너 회복 지연 타이머 핸들
     */
    FTimerHandle StaminaRegenDelayTimerHandle;
    
    /**
     * @brief 달리기 활성화 상태
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Movement")
    bool bSprintEnabled;

    /**
     * @brief 공격 쿨다운 타이머 핸들
     */
    FTimerHandle AttackCooldownTimerHandle;
    
    /**
     * @brief 스태미너 재생을 시작
     * @details 지연 시간 후 호출되는 함수
     */
    UFUNCTION()
    virtual void StartStaminaRegeneration();
    
    /**
     * @brief 스태미너 수치를 업데이트
     * @param DeltaTime 이전 프레임과의 시간 차이
     */
    UFUNCTION()
    virtual void UpdateStamina(float DeltaTime);
    
    /**
     * @brief 현재 체력
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
    float Health = 100.0f;
    
    /**
     * @brief 최대 체력
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
    float MaxHealth = 100.0f;
};
