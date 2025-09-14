// HuntingSpirit Game - Player Character Implementation

#include "HSPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "HuntingSpirit/Core/PlayerController/HSPlayerController.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"

// 생성자
AHSPlayerCharacter::AHSPlayerCharacter()
{
    // 카메라 붐 생성 (충돌 시 카메라를 당김)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 800.0f; // 카메라와의 거리
    CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // 탑다운 시점을 위한 회전
    CameraBoom->bUsePawnControlRotation = false; // 컨트롤러 기반 회전 비활성화
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bDoCollisionTest = false; // 충돌 테스트 비활성화

    // 팔로우 카메라 생성
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false; // 카메라가 붐을 기준으로 회전하지 않도록 설정

    // 캐릭터 이동 설정
    GetCharacterMovement()->bOrientRotationToMovement = true; // 이동 방향으로 회전
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // 회전 속도
    GetCharacterMovement()->bConstrainToPlane = true;
    GetCharacterMovement()->bSnapToPlaneAtStart = true;

    // 컨트롤러 기반 회전 비활성화
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    
    // 기본 캐릭터 클래스 설정
    PlayerClass = EHSPlayerClass::Warrior;
    
    // 스태미너 UI 관련 설정
    StaminaUIDisplayTime = 3.0f;
    bShowStaminaBar = false;
}

// 게임 시작 또는 스폰 시 호출
void AHSPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 애니메이션 참조 설정
    SetupAnimationReferences();
    
    // 직업에 따른 스태미너 설정 (예: 전사는 더 많은 스태미너, 시프는 빠른 회복률)
    switch (PlayerClass)
    {
        case EHSPlayerClass::Warrior:
            StaminaMax = 120.0f;
            StaminaRegenRate = 10.0f;
            SprintStaminaConsumptionRate = 15.0f;
            break;
            
        case EHSPlayerClass::Thief:
            StaminaMax = 100.0f;
            StaminaRegenRate = 15.0f;
            SprintStaminaConsumptionRate = 12.0f;
            break;
            
        case EHSPlayerClass::Mage:
            StaminaMax = 80.0f;
            StaminaRegenRate = 8.0f;
            SprintStaminaConsumptionRate = 18.0f;
            break;
            
        default:
            break;
    }
    
    // 스태미너 초기화
    StaminaCurrent = StaminaMax;
}

// 매 프레임 호출
void AHSPlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 입력 컴포넌트 설정
void AHSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // HSPlayerController에서 이미 클릭 이동 및 타겟팅 입력 처리
    // 여기서는 추가적인 캐릭터 액션만 설정
    
    // 달리기 토글 - Alt 키
    PlayerInputComponent->BindAction("ToggleSprint", IE_Pressed, this, &AHSPlayerCharacter::OnToggleSprintPressed);
    
    // 공격 입력 설정 - 레거시 입력
    PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AHSPlayerCharacter::PerformBasicAttack);
}

// Alt 키를 누르면 달리기/걷기 전환
void AHSPlayerCharacter::OnToggleSprintPressed()
{
    ToggleSprint();
}

// 달리기 토글 함수 오버라이드
void AHSPlayerCharacter::ToggleSprint()
{
    // 부모 클래스의 ToggleSprint 호출
    Super::ToggleSprint();
    
    // 스태미너 UI 표시 타이머 재설정
    GetWorldTimerManager().ClearTimer(StaminaUITimerHandle);
    bShowStaminaBar = true;
    GetWorldTimerManager().SetTimer(StaminaUITimerHandle, this, &AHSPlayerCharacter::HideStaminaBar, StaminaUIDisplayTime, false);
}

// 공격 함수 오버라이드
void AHSPlayerCharacter::PerformBasicAttack()
{
    // 공격 시 스태미너 소모
    float AttackStaminaCost = 10.0f;
    
    // 충분한 스태미너가 있는지 확인
    if (UseStamina(AttackStaminaCost))
    {
        Super::PerformBasicAttack();
        
        // 스태미너 UI 표시 타이머 재설정
        GetWorldTimerManager().ClearTimer(StaminaUITimerHandle);
        bShowStaminaBar = true;
        GetWorldTimerManager().SetTimer(StaminaUITimerHandle, this, &AHSPlayerCharacter::HideStaminaBar, StaminaUIDisplayTime, false);
    }
    else
    {
        // 스태미너 부족 시 피드백 제공 (소리, 애니메이션 등)
        // 여기서는 스태미너 바만 표시함
        GetWorldTimerManager().ClearTimer(StaminaUITimerHandle);
        bShowStaminaBar = true;
        GetWorldTimerManager().SetTimer(StaminaUITimerHandle, this, &AHSPlayerCharacter::HideStaminaBar, StaminaUIDisplayTime, false);
    }
}

// 스태미너 UI 표시 종료
void AHSPlayerCharacter::HideStaminaBar()
{
    bShowStaminaBar = false;
}

// 스태미너 UI 표시 여부 확인
bool AHSPlayerCharacter::ShouldShowStaminaBar() const
{
    // 달리기 중이거나 타이머가 활성화되어 있으면 표시
    return bShowStaminaBar || IsSprintEnabled();
}

// 공통 애니메이션 가져오기 - 대기
UAnimMontage* AHSPlayerCharacter::GetIdleAnimMontage() const
{
    return CommonAnimations.IdleMontage;
}

// 공통 애니메이션 가져오기 - 걷기
UAnimMontage* AHSPlayerCharacter::GetWalkAnimMontage() const
{
    return CommonAnimations.WalkMontage;
}

// 공통 애니메이션 가져오기 - 달리기
UAnimMontage* AHSPlayerCharacter::GetRunAnimMontage() const
{
    return CommonAnimations.RunMontage;
}

// 공통 애니메이션 가져오기 - 기본 공격
UAnimMontage* AHSPlayerCharacter::GetAttackAnimMontage() const
{
    return CommonAnimations.BasicAttackMontage;
}

// 공통 애니메이션 가져오기 - 점프
UAnimMontage* AHSPlayerCharacter::GetJumpAnimMontage() const
{
    return CommonAnimations.JumpMontage;
}

// 공통 애니메이션 가져오기 - 피격
UAnimMontage* AHSPlayerCharacter::GetHitReactionAnimMontage() const
{
    return CommonAnimations.HitReactionMontage;
}

// 공통 애니메이션 가져오기 - 사망
UAnimMontage* AHSPlayerCharacter::GetDeathAnimMontage() const
{
    return CommonAnimations.DeathMontage;
}

// 플레이어 클래스 설정
void AHSPlayerCharacter::SetPlayerClass(EHSPlayerClass NewPlayerClass)
{
    if (PlayerClass != NewPlayerClass)
    {
        PlayerClass = NewPlayerClass;
        
        // 클래스에 따른 스태미너 설정 업데이트
        switch (PlayerClass)
        {
            case EHSPlayerClass::Warrior:
                StaminaMax = 120.0f;
                StaminaRegenRate = 10.0f;
                SprintStaminaConsumptionRate = 15.0f;
                break;
                
            case EHSPlayerClass::Thief:
                StaminaMax = 100.0f;
                StaminaRegenRate = 15.0f;
                SprintStaminaConsumptionRate = 12.0f;
                break;
                
            case EHSPlayerClass::Mage:
                StaminaMax = 80.0f;
                StaminaRegenRate = 8.0f;
                SprintStaminaConsumptionRate = 18.0f;
                break;
                
            default:
                break;
        }
        
        // 스태미너 초기화
        StaminaCurrent = StaminaMax;
        
        // 기본 공격 애니메이션 업데이트
        BasicAttackMontage = CommonAnimations.BasicAttackMontage;
    }
}



// 애니메이션 참조 설정
void AHSPlayerCharacter::SetupAnimationReferences()
{
    // 공통 애니메이션 설정
    BasicAttackMontage = CommonAnimations.BasicAttackMontage;
}
