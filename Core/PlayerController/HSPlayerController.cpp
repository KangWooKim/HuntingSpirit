// 사냥의 영혼(HuntingSpirit) 게임의 플레이어 컨트롤러 구현

#include "HSPlayerController.h"
#include "HuntingSpirit/Characters/Player/HSPlayerCharacter.h"
#include "HuntingSpirit/Characters/Player/Warrior/HSWarriorCharacter.h"
#include "HuntingSpirit/Characters/Player/Thief/HSThiefCharacter.h"
#include "HuntingSpirit/Characters/Player/Mage/HSMageCharacter.h"
#include "HuntingSpirit/Characters/Components/HSCameraComponent.h"
#include "HuntingSpirit/Cooperation/Communication/HSCommunicationSystem.h"
#include "HuntingSpirit/Gathering/Inventory/HSInventoryComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/InputComponent.h"
#include "GameFramework/Character.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/GameInstance.h"

// 생성자
AHSPlayerController::AHSPlayerController()
{
    // 실시간으로 플레이어 틱 업데이트 활성화
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;
    TargetingMouseCursor = EMouseCursor::Crosshairs;
    
    // 클릭으로 이동 설정
    bEnableClickToMove = true;
    ClickMoveAcceptanceRadius = 120.0f; // 목표 도달 인정 거리
    
    // 초기 타겟 설정
    CurrentTarget = nullptr;
    
    // 스태미너 UI 표시 여부 초기화
    bShowStaminaUI = false;
    
    // UI 표시 상태 초기화
    bChatUIVisible = false;
    bVoiceChatActive = false;
    bInventoryUIVisible = false;
    bMenuUIVisible = false;
}

// 게임 시작 시 호출
void AHSPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    // 탑다운 뷰 카메라 설정
    SetupTopDownCamera();
    
    // 마우스 커서 표시 설정
    bShowMouseCursor = true;
    CurrentMouseCursor = DefaultMouseCursor;
    
    // 입력 모드 설정 (게임 + UI)
    FInputModeGameAndUI InputModeData;
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputModeData.SetHideCursorDuringCapture(false);
    SetInputMode(InputModeData);
    
    // 통신 시스템 참조 가져오기
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        CommunicationSystem = GameInstance->GetSubsystem<UHSCommunicationSystem>();
    }
}

// 프레임마다 호출
void AHSPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    
    // 필요한 추가 틱 로직 구현
}

// 입력 설정
void AHSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // 입력 바인딩
    UInputComponent* InputComp = InputComponent;
    if (InputComp)
    {
        // 마우스 이동 입력 바인딩 (좌클릭)
        InputComp->BindAction("LeftClick", IE_Pressed, this, &AHSPlayerController::OnClickToMove);
        
        // 기본 공격 입력 바인딩 (우클릭)
        InputComp->BindAction("RightClick", IE_Pressed, this, &AHSPlayerController::OnRightClickAttack);
        
        // 스킬 입력 바인딩 (QWER)
        InputComp->BindAction("SkillQ", IE_Pressed, this, &AHSPlayerController::OnSkillQ);
        InputComp->BindAction("SkillW", IE_Pressed, this, &AHSPlayerController::OnSkillW);
        InputComp->BindAction("SkillE", IE_Pressed, this, &AHSPlayerController::OnSkillE);
        InputComp->BindAction("SkillR", IE_Pressed, this, &AHSPlayerController::OnSkillR);
        
        // 채팅 입력 바인딩
        InputComp->BindAction("ToggleChat", IE_Pressed, this, &AHSPlayerController::OnToggleChat);
        InputComp->BindAction("VoiceChat", IE_Pressed, this, &AHSPlayerController::OnVoiceChatPressed);
        InputComp->BindAction("VoiceChat", IE_Released, this, &AHSPlayerController::OnVoiceChatReleased);
        
        // UI 토글 입력 바인딩
        InputComp->BindAction("ToggleInventory", IE_Pressed, this, &AHSPlayerController::OnToggleInventory);
        InputComp->BindAction("ToggleMenu", IE_Pressed, this, &AHSPlayerController::OnToggleMenu);
        
        // 타겟팅 입력 바인딩 (Tab 키)
        InputComp->BindAction("Target", IE_Pressed, this, &AHSPlayerController::TargetUnderCursor);
        InputComp->BindAction("ClearTarget", IE_Pressed, this, &AHSPlayerController::ClearCurrentTarget);
        
        // 달리기 토글 입력 바인딩 (Alt 키)
        InputComp->BindAction("ToggleSprint", IE_Pressed, this, &AHSPlayerController::OnToggleSprintPressed);
    }
}

// 달리기 토글 입력 처리
void AHSPlayerController::OnToggleSprintPressed()
{
    // 해당 플레이어 캐릭터가 있는지 확인
    APawn* ControlledPawn = GetPawn();
    AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(ControlledPawn);
    if (PlayerCharacter)
    {
        // 플레이어 캐릭터의 달리기 토글 함수 호출
        PlayerCharacter->ToggleSprint();
        
        // 스태미너 UI 표시 설정
        bShowStaminaUI = PlayerCharacter->ShouldShowStaminaBar();
    }
}

// 탑다운 뷰 카메라 설정
void AHSPlayerController::SetupTopDownCamera()
{
    // 현재 소유중인 캐릭터 확인
    ACharacter* ControlledCharacter = GetCharacter();
    if (!ControlledCharacter)
    {
        return;
    }
    
    // HSCameraComponent가 있는지 확인
    UHSCameraComponent* CameraComp = ControlledCharacter->FindComponentByClass<UHSCameraComponent>();
    if (!CameraComp)
    {
        // 없으면 새로 추가
        CameraComp = NewObject<UHSCameraComponent>(ControlledCharacter, TEXT("TopDownCameraComponent"));
        CameraComp->RegisterComponent();
        
        UE_LOG(LogTemp, Display, TEXT("카메라 컴포넌트 추가됨"));
    }
    
    // 뷰 회전 상속 설정
    bAutoManageActiveCameraTarget = true;
}

// 클릭으로 이동 처리
void AHSPlayerController::OnClickToMove()
{
    if (!bEnableClickToMove)
    {
        return;
    }
    
    // 마우스 커서 위치에서 월드 위치 추출
    FHitResult HitResult;
    GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResult);
    
    if (HitResult.bBlockingHit)
    {
        // 이동 가능한 위치인지 확인
        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        if (NavSys)
        {
            // 가장 가까운 이동 가능한 지점 찾기
            FNavLocation NavLocation;
            if (NavSys->ProjectPointToNavigation(HitResult.Location, NavLocation, FVector(1000.0f, 1000.0f, 1000.0f)))
            {
                // 이동 명령 화살표 생성
                SpawnMoveCommandArrow(NavLocation.Location);
                
                // 캐릭터를 해당 위치로 이동
                MoveToLocation(NavLocation.Location);
            }
        }
        else
        {
            // 네비게이션 시스템이 없으면 히트 위치로 직접 이동
            SpawnMoveCommandArrow(HitResult.Location);
            MoveToLocation(HitResult.Location);
        }
    }
}

// 마우스 커서 아래의 액터를 타겟팅
void AHSPlayerController::TargetUnderCursor()
{
    // 마우스 커서 아래의 액터 찾기
    FHitResult HitResult;
    GetHitResultUnderCursor(ECC_Pawn, false, HitResult);
    
    if (HitResult.bBlockingHit)
    {
        // 타겟팅 가능한 액터인지 확인 (예: 적 캐릭터)
        AActor* HitActor = HitResult.GetActor();
        if (HitActor && HitActor != GetPawn() && !HitActor->ActorHasTag(TEXT("NonTargetable")))
        {
            // 타겟 설정
            SetCurrentTarget(HitActor);
            
            // 타겟팅 커서로 변경
            CurrentMouseCursor = TargetingMouseCursor;
            
            UE_LOG(LogTemp, Display, TEXT("타겟팅: %s"), *HitActor->GetName());
        }
    }
}

// 현재 타겟 제거
void AHSPlayerController::ClearCurrentTarget()
{
    // 타겟 제거
    SetCurrentTarget(nullptr);
    
    // 기본 커서로 복원
    CurrentMouseCursor = DefaultMouseCursor;
    
    UE_LOG(LogTemp, Display, TEXT("타겟팅 해제"));
}

// 타겟 설정
void AHSPlayerController::SetCurrentTarget(AActor* NewTarget)
{
    // 이전 타겟 효과 제거
    if (CurrentTarget)
    {
        // 타겟 아웃라인 등 시각적 효과 제거 (필요시 구현)
    }
    
    // 새 타겟 설정
    CurrentTarget = NewTarget;
    
    // 새 타겟에 시각적 효과 적용 (필요시 구현)
    if (CurrentTarget)
    {
        // 타겟 아웃라인 등 시각적 효과 적용
    }
}

// 지정한 위치로 캐릭터 이동
void AHSPlayerController::MoveToLocation(const FVector& Location)
{
    APawn* ControlledPawn = GetPawn();
    if (ControlledPawn)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Location);
        
        // 이동 목표 위치로 캐릭터 회전
        FVector Direction = Location - ControlledPawn->GetActorLocation();
        Direction.Z = 0.0f; // Z축 무시
        
        if (!Direction.IsNearlyZero())
        {
            FRotator NewRotation = Direction.Rotation();
            ControlledPawn->SetActorRotation(NewRotation);
        }
    }
}

// 지정한 액터로 캐릭터 이동
void AHSPlayerController::MoveToActor(AActor* TargetActor)
{
    if (TargetActor)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToActor(this, TargetActor);
    }
}

// 클릭으로 이동 활성화/비활성화
void AHSPlayerController::SetClickToMove(bool bEnable)
{
    bEnableClickToMove = bEnable;
}

// 이동 명령 화살표 생성
void AHSPlayerController::SpawnMoveCommandArrow(const FVector& Location)
{
    UWorld* World = GetWorld();
    if (!World || !MoveCommandArrowSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("나이아가라 시스템이 없습니다."));
        return;
    }
    
    // 화살표는 바닥에서 조금 떨어진 위치에 생성
    FVector ArrowLocation = Location + FVector(0, 0, 10.0f);
    
    // 나이아가라 시스템 스폰 - 자동 활성/비활성 설정
    UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World,
        MoveCommandArrowSystem,
        ArrowLocation,
        FRotator::ZeroRotator,
        FVector(MoveCommandArrowScale),
        true,  // Auto-activate
        true,  // Auto-destroy when complete
        ENCPoolMethod::AutoRelease
    );
    
    if (NiagaraComp)
    {
        UE_LOG(LogTemp, Display, TEXT("나이아가라 화살표 효과 생성 성공"));
        
        // 추가 파라미터 설정은 여기서 가능
        // NiagaraComp->SetVariableFloat("사용자정의변수", 1.0f);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("나이아가라 화살표 효과 생성 실패"));
    }
}

// 이동 명령 화살표 제거
void AHSPlayerController::RemoveMoveCommandArrow(AActor* ArrowActor)
{
    // 나이아가라 시스템은 자동으로 제거되므로 필요 없음
    // 하지만 추가 필요한 작업이 있다면 여기에 구현
}

// 우클릭 기본 공격
void AHSPlayerController::OnRightClickAttack()
{
    // 마우스 커서 아래의 타겟 확인
    FHitResult HitResult;
    GetHitResultUnderCursor(ECC_Pawn, false, HitResult);
    
    AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(GetPawn());
    if (!PlayerCharacter)
    {
        return;
    }
    
    // 타겟이 있으면 타겟에 대한 공격, 없으면 방향으로 공격
    if (HitResult.bBlockingHit && HitResult.GetActor() && HitResult.GetActor() != PlayerCharacter)
    {
        // 타겟 설정
        SetCurrentTarget(HitResult.GetActor());
        
        // 타겟을 향해 기본 공격 수행
        PlayerCharacter->PerformBasicAttack();
    }
    else
    {
        // 마우스 방향으로 공격
        GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResult);
        if (HitResult.bBlockingHit)
        {
            FVector AttackDirection = HitResult.Location - PlayerCharacter->GetActorLocation();
            AttackDirection.Z = 0.0f;
            AttackDirection.Normalize();
            
            // 캐릭터를 해당 방향으로 회전
            PlayerCharacter->SetActorRotation(AttackDirection.Rotation());
            
            // 기본 공격 수행
            PlayerCharacter->PerformBasicAttack();
        }
    }
}

// Q 스킬
void AHSPlayerController::OnSkillQ()
{
    UseSkill(0);
}

// W 스킬
void AHSPlayerController::OnSkillW()
{
    UseSkill(1);
}

// E 스킬
void AHSPlayerController::OnSkillE()
{
    UseSkill(2);
}

// R 스킬 (궁극기)
void AHSPlayerController::OnSkillR()
{
    UseSkill(3);
}

// 스킬 사용 통합 함수 - 직업별 스킬 시스템
void AHSPlayerController::UseSkill(int32 SkillIndex)
{
    AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(GetPawn());
    if (!PlayerCharacter)
    {
        return;
    }
    
    // 마우스 커서 방향 계산
    FHitResult HitResult;
    GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResult);
    
    FVector SkillDirection = FVector::ForwardVector;
    if (HitResult.bBlockingHit)
    {
        SkillDirection = HitResult.Location - PlayerCharacter->GetActorLocation();
        SkillDirection.Z = 0.0f;
        SkillDirection.Normalize();
        
        // 캐릭터를 스킬 방향으로 회전
        PlayerCharacter->SetActorRotation(SkillDirection.Rotation());
    }
    
    // 직업별 스킬 사용
    EHSPlayerClass PlayerClass = PlayerCharacter->GetPlayerClass();
    
    switch (PlayerClass)
    {
        case EHSPlayerClass::Warrior:
        {
            AHSWarriorCharacter* WarriorCharacter = Cast<AHSWarriorCharacter>(PlayerCharacter);
            if (WarriorCharacter)
            {
                switch (SkillIndex)
                {
                    case 0: // Q - 방어
                        WarriorCharacter->UseSkillQ();
                        break;
                    case 1: // W - 돌진
                        WarriorCharacter->UseSkillW();
                        break;
                    case 2: // E - 회전베기
                        WarriorCharacter->UseSkillE();
                        break;
                    case 3: // R - 광폭화
                        WarriorCharacter->UseSkillR();
                        break;
                }
            }
            break;
        }
        case EHSPlayerClass::Thief:
        {
            AHSThiefCharacter* ThiefCharacter = Cast<AHSThiefCharacter>(PlayerCharacter);
            if (ThiefCharacter)
            {
                switch (SkillIndex)
                {
                    case 0: // Q - 은신
                        ThiefCharacter->UseSkillQ();
                        break;
                    case 1: // W - 질주
                        ThiefCharacter->UseSkillW();
                        break;
                    case 2: // E - 회피
                        ThiefCharacter->UseSkillE();
                        break;
                    case 3: // R - 연속공격
                        ThiefCharacter->UseSkillR();
                        break;
                }
            }
            break;
        }
        case EHSPlayerClass::Mage:
        {
            AHSMageCharacter* MageCharacter = Cast<AHSMageCharacter>(PlayerCharacter);
            if (MageCharacter)
            {
                switch (SkillIndex)
                {
                    case 0: // Q - 화염구
                        MageCharacter->UseSkillQ();
                        break;
                    case 1: // W - 얼음창
                        MageCharacter->UseSkillW();
                        break;
                    case 2: // E - 번개
                        MageCharacter->UseSkillE();
                        break;
                    case 3: // R - 메테오
                        MageCharacter->UseSkillR();
                        break;
                }
            }
            break;
        }
        default:
            UE_LOG(LogTemp, Warning, TEXT("알 수 없는 캐릭터 클래스"));
            break;
    }
    
    // 블루프린트에서 추가 처리가 필요한 경우 이벤트 발생
    OnSkillUsed(SkillIndex, SkillDirection);
    
    UE_LOG(LogTemp, Log, TEXT("Skill %d used by %s in direction %s"), 
           SkillIndex, 
           *UEnum::GetValueAsString(PlayerClass), 
           *SkillDirection.ToString());
}

// 채팅 토글
void AHSPlayerController::OnToggleChat()
{
    bChatUIVisible = !bChatUIVisible;
    
    if (CommunicationSystem)
    {
        if (bChatUIVisible)
        {
            // 채팅 UI 열기는 블루프린트에서 처리
            // 입력 모드를 UI 우선으로 변경
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(nullptr); // 채팅 위젯으로 포커스 설정 필요
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);
            
            // 블루프린트 이벤트 호출
            OnChatToggled(true);
            
            UE_LOG(LogTemp, Log, TEXT("Chat UI Opened"));
        }
        else
        {
            // 채팅 UI 닫기는 블루프린트에서 처리
            // 입력 모드를 게임으로 복원
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);
            
            // 블루프린트 이벤트 호출
            OnChatToggled(false);
            
            UE_LOG(LogTemp, Log, TEXT("Chat UI Closed"));
        }
    }
}

// 보이스 채팅 시작
void AHSPlayerController::OnVoiceChatPressed()
{
    bVoiceChatActive = true;
    
    if (CommunicationSystem)
    {
        CommunicationSystem->StartVoiceChat();
        UE_LOG(LogTemp, Log, TEXT("Voice Chat Started"));
    }
}

// 보이스 채팅 종료
void AHSPlayerController::OnVoiceChatReleased()
{
    bVoiceChatActive = false;
    
    if (CommunicationSystem)
    {
        CommunicationSystem->StopVoiceChat();
        UE_LOG(LogTemp, Log, TEXT("Voice Chat Stopped"));
    }
}

// 인벤토리 토글
void AHSPlayerController::OnToggleInventory()
{
    bInventoryUIVisible = !bInventoryUIVisible;
    
    AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(GetPawn());
    if (!PlayerCharacter)
    {
        return;
    }
    
    // 인벤토리 UI 열기/닫기는 블루프린트에서 처리
    // 인벤토리 컴포넌트 가져오기
    if (UHSInventoryComponent* InventoryComp = PlayerCharacter->FindComponentByClass<UHSInventoryComponent>())
    {
        if (bInventoryUIVisible)
        {
            // 인벤토리 UI 열기는 블루프린트에서 처리
            // 입력 모드를 UI로 변경
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);
            
            // 블루프린트 이벤트 호출
            OnInventoryToggled(true);
            
            UE_LOG(LogTemp, Log, TEXT("Inventory UI Opened"));
        }
        else
        {
            // 인벤토리 UI 닫기는 블루프린트에서 처리
            // 입력 모듌를 게임으로 복원
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);
            
            // 블루프린트 이벤트 호출
            OnInventoryToggled(false);
            
            UE_LOG(LogTemp, Log, TEXT("Inventory UI Closed"));
        }
    }
}

// 메뉴 토글
void AHSPlayerController::OnToggleMenu()
{
    bMenuUIVisible = !bMenuUIVisible;
    
    if (bMenuUIVisible)
    {
        // 게임 일시정지
        SetPause(true);
        
        // 메뉴 UI 열기 (블루프린트에서 처리)
        OnMenuToggled(true);
    }
    else
    {
        // 게임 재개
        SetPause(false);
        
        // 메뉴 UI 닫기 (블루프린트에서 처리)
        OnMenuToggled(false);
    }
}