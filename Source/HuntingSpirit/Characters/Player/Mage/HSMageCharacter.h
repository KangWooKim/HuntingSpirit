// HuntingSpirit Game - Mage Character Header
// 마법사 캐릭터 클래스

#pragma once

#include "CoreMinimal.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HSMageCharacter.generated.h"

// 마법 공격 타입 열거형 (기존 유지하되 확장)
UENUM(BlueprintType)
enum class EMagicType : uint8
{
    Fire        UMETA(DisplayName = "Fire"),
    Ice         UMETA(DisplayName = "Ice"),
    Lightning   UMETA(DisplayName = "Lightning"),
    Arcane      UMETA(DisplayName = "Arcane")
};

// 마법사 스킬 타입 열거형
UENUM(BlueprintType)
enum class EMageSkillType : uint8
{
    None            UMETA(DisplayName = "None"),
    Fireball        UMETA(DisplayName = "Fireball"),        // Q 스킬 - 화염구
    IceShard        UMETA(DisplayName = "Ice Shard"),       // W 스킬 - 얼음창
    LightningBolt   UMETA(DisplayName = "Lightning Bolt"),  // E 스킬 - 번개
    Meteor          UMETA(DisplayName = "Meteor")           // R 스킬 - 메테오 (궁극기)
};

// 마법사 스킬 데이터 구조체
USTRUCT(BlueprintType)
struct FMageSkillData
{
    GENERATED_BODY()

    // 스킬 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    UAnimMontage* SkillMontage = nullptr;

    // 스킬 쿨다운 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Cooldown = 5.0f;

    // 스킬 캐스팅 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float CastTime = 1.0f;

    // 스킬 마나 소모량
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float ManaCost = 30.0f;

    // 스킬 데미지
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Damage = 50.0f;

    // 스킬 사거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    float Range = 800.0f;

    // 마법 타입
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    EMagicType MagicType = EMagicType::Arcane;

    // 발사체 클래스 (해당되는 경우)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    TSubclassOf<AActor> ProjectileClass = nullptr;

    FMageSkillData()
    {
        SkillMontage = nullptr;
        Cooldown = 5.0f;
        CastTime = 1.0f;
        ManaCost = 30.0f;
        Damage = 50.0f;
        Range = 800.0f;
        MagicType = EMagicType::Arcane;
        ProjectileClass = nullptr;
    }
};

/**
 * 마법사 캐릭터 클래스
 * 원거리 마법 공격과 다양한 마법 효과를 사용하는 캐릭터
 * Q: 화염구 - W: 얼음창 - E: 번개 - R: 메테오
 */
UCLASS()
class HUNTINGSPIRIT_API AHSMageCharacter : public AHSPlayerCharacter
{
    GENERATED_BODY()

public:
    // 생성자
    AHSMageCharacter();
    
    // 게임 시작 또는 스폰 시 호출
    virtual void BeginPlay() override;
    
    // 매 프레임 호출
    virtual void Tick(float DeltaTime) override;
    
    // 특수 공격 (오버라이드)
    virtual void PerformBasicAttack() override;
    
    // 마법사 스킬 시스템 (QWER)
    UFUNCTION(BlueprintCallable, Category = "Mage|Skills")
    void UseSkillQ(); // 화염구
    
    UFUNCTION(BlueprintCallable, Category = "Mage|Skills")
    void UseSkillW(); // 얼음창
    
    UFUNCTION(BlueprintCallable, Category = "Mage|Skills")
    void UseSkillE(); // 번개
    
    UFUNCTION(BlueprintCallable, Category = "Mage|Skills")
    void UseSkillR(); // 메테오 (궁극기)
    
    // 스킬 사용 가능 여부 확인
    UFUNCTION(BlueprintPure, Category = "Mage|Skills")
    bool CanUseSkill(EMageSkillType SkillType) const;
    
    // 스킬 쿨다운 남은 시간 반환
    UFUNCTION(BlueprintPure, Category = "Mage|Skills")
    float GetSkillCooldownRemaining(EMageSkillType SkillType) const;
    
    // 스킬 데이터 가져오기
    UFUNCTION(BlueprintPure, Category = "Mage|Skills")
    FMageSkillData GetSkillData(EMageSkillType SkillType) const;
    
    // 마나 관련 함수
    UFUNCTION(BlueprintPure, Category = "Mage|Mana")
    float GetCurrentMana() const { return ManaCurrent; }
    
    UFUNCTION(BlueprintPure, Category = "Mage|Mana")
    float GetMaxMana() const { return ManaMax; }
    
    UFUNCTION(BlueprintPure, Category = "Mage|Mana")
    float GetManaPercentage() const { return ManaMax > 0.0f ? ManaCurrent / ManaMax : 0.0f; }
    
    UFUNCTION(BlueprintCallable, Category = "Mage|Mana")
    bool ConsumeMana(float ManaAmount);
    
    UFUNCTION(BlueprintCallable, Category = "Mage|Mana")
    void RestoreMana(float ManaAmount);
    
    // 캐스팅 중인지 확인
    UFUNCTION(BlueprintPure, Category = "Mage|State")
    bool IsCasting() const { return bIsCasting; }

protected:
    // 마법사 스킬 데이터
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Mage")
    FMageSkillData FireballData;        // Q 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Mage")
    FMageSkillData IceShardData;        // W 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Mage")
    FMageSkillData LightningBoltData;   // E 스킬
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills|Mage")
    FMageSkillData MeteorData;          // R 스킬
    
    // 스킬 쿨다운 타이머 핸들
    UPROPERTY()
    TMap<EMageSkillType, FTimerHandle> SkillCooldownTimers;
    
    // 마나 특성
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mage|Mana")
    float ManaCurrent = 100.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mage|Mana")
    float ManaMax = 100.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mage|Mana")
    float ManaRegenRate = 15.0f; // 초당 마나 회복량
    
    // 캐스팅 관련
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mage|State")
    bool bIsCasting = false;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mage|State")
    EMageSkillType CurrentCastingSkill = EMageSkillType::None;
    
    // 현재 선택된 마법 타입 (기존 유지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mage|Magic")
    EMagicType CurrentMagicType = EMagicType::Fire;
    
    // 마법사 스킬 구현 내부 함수
    void ExecuteFireball();         // Q 스킬 실행
    void ExecuteIceShard();         // W 스킬 실행
    void ExecuteLightningBolt();    // E 스킬 실행
    void ExecuteMeteor();           // R 스킬 실행
    
    // 캐스팅 관리
    void StartCasting(EMageSkillType SkillType, float CastTime);
    
    UFUNCTION()
    void FinishCasting();
    
    UFUNCTION()
    void InterruptCasting();
    
    // 발사체 생성
    void SpawnMagicProjectile(TSubclassOf<AActor> ProjectileClass, const FVector& Direction);
    
    // 마법 타입 전환 (기존 유지)
    UFUNCTION(BlueprintCallable, Category = "Mage|Magic")
    void CycleMagicType();
    
    // 마나 재생
    void RegenerateMana(float DeltaTime);
    
    // 마법사 특화 스탯 설정
    void SetupMageStats();
    
    // 스킬 초기화
    void InitializeMageSkills();
    
    // 스킬 쿨다운 시작
    void StartSkillCooldown(EMageSkillType SkillType, float CooldownTime);
    
    // 캐스팅 타이머
    FTimerHandle CastingTimerHandle;
    
    // 특수 효과용 타이머
    FTimerHandle MeteorImpactTimerHandle;
};
