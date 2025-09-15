// 사냥의 영혼(HuntingSpirit) 게임의 플레이어 컨트롤러 클래스

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraSystem.h"
#include "HSPlayerController.generated.h"

/**
 * 사냥의 영혼 게임의 플레이어 컨트롤러
 * 디아블로 스타일 탑다운 뷰의 조작 및 카메라 제어 담당
 */
UCLASS()
class HUNTINGSPIRIT_API AHSPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    // 생성자
    AHSPlayerController();

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 탑다운 뷰를 위한 카메라 설정 함수
    void SetupTopDownCamera();

    // 마우스 이동에 따른 이동 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "조작 설정")
    bool bEnableClickToMove; // 클릭으로 이동 활성화 여부

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "조작 설정")
    float ClickMoveAcceptanceRadius; // 이동 목표 도달 인정 반경
    
    // 마우스 커서 설정 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|커서")
    TEnumAsByte<EMouseCursor::Type> TargetingMouseCursor; // 타겟팅 시 마우스 커서
    
    // 통신 시스템 레퍼런스
    UPROPERTY()
    class UHSCommunicationSystem* CommunicationSystem;
    
    // 채팅 UI 표시 여부
    UPROPERTY(BlueprintReadOnly, Category = "UI|채팅")
    bool bChatUIVisible;
    
    // 보이스 채팅 활성화 여부
    UPROPERTY(BlueprintReadOnly, Category = "UI|채팅")
    bool bVoiceChatActive;
    
    // 인벤토리 UI 표시 여부
    UPROPERTY(BlueprintReadOnly, Category = "UI|인벤토리")
    bool bInventoryUIVisible;
    
    // 메뉴 UI 표시 여부
    UPROPERTY(BlueprintReadOnly, Category = "UI|메뉴")
    bool bMenuUIVisible;
    
    // 이동 명령 화살표 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|이동 표시")
    UNiagaraSystem* MoveCommandArrowSystem; // 이동 명령 시 표시할 화살표 나이아가라 시스템
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|이동 표시")
    float MoveCommandArrowScale = 1.0f; // 화살표 크기

    // 타겟팅 관련 변수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "타겟팅")
    AActor* CurrentTarget; // 현재 타겟팅된 액터
    
    // 스태미너 UI 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|스태미너")
    bool bShowStaminaUI;

    // 클릭으로 이동 구현 함수
    void OnClickToMove();
    
    // 달리기 토글 입력 처리
    void OnToggleSprintPressed();
    
    // 우클릭 기본 공격
    void OnRightClickAttack();
    
    // 스킬 입력 처리 (QWER) - 직업별 스킬 시스템
    void OnSkillQ();
    void OnSkillW();
    void OnSkillE();
    void OnSkillR(); // 궁극기
    
    // 채팅 관련 입력
    void OnToggleChat();
    void OnVoiceChatPressed();
    void OnVoiceChatReleased();
    
    // UI 토글 입력
    void OnToggleInventory();
    void OnToggleMenu();
    
    // 이동 명령 화살표 생성 함수
    UFUNCTION(BlueprintCallable, Category = "UI|이동 표시")
    void SpawnMoveCommandArrow(const FVector& Location);
    
    // 이동 명령 화살표 제거 함수
    UFUNCTION()
    void RemoveMoveCommandArrow(AActor* ArrowActor);

    // 타겟팅 시스템 함수
    UFUNCTION(BlueprintCallable, Category = "타겟팅")
    void TargetUnderCursor();

    UFUNCTION(BlueprintCallable, Category = "타겟팅")
    void ClearCurrentTarget();

public:
    // 프레임마다 호출
    virtual void PlayerTick(float DeltaTime) override;

    // 입력 설정
    virtual void SetupInputComponent() override;

    // 타겟 관련 함수
    UFUNCTION(BlueprintPure, Category = "타겟팅")
    AActor* GetCurrentTarget() const { return CurrentTarget; }

    UFUNCTION(BlueprintCallable, Category = "타겟팅")
    void SetCurrentTarget(AActor* NewTarget);
    
    // 탑다운 뷰에서 지정한 위치로 캐릭터 이동
    UFUNCTION(BlueprintCallable, Category = "조작")
    void MoveToLocation(const FVector& Location);
    
    // 탑다운 뷰에서 지정한 액터로 캐릭터 이동
    UFUNCTION(BlueprintCallable, Category = "조작")
    void MoveToActor(AActor* TargetActor);
    
    // 조작 설정 함수
    UFUNCTION(BlueprintCallable, Category = "조작 설정")
    void SetClickToMove(bool bEnable);
    
    // 스태미너 UI 표시 여부 설정
    UFUNCTION(BlueprintCallable, Category = "UI|스태미너")
    void SetShowStaminaUI(bool bShow) { bShowStaminaUI = bShow; }
    
    // 스태미너 UI 표시 여부 확인
    UFUNCTION(BlueprintPure, Category = "UI|스태미너")
    bool ShouldShowStaminaUI() const { return bShowStaminaUI; }
    
    // 채팅 UI 표시 여부 확인
    UFUNCTION(BlueprintPure, Category = "UI|채팅")
    bool IsChatUIVisible() const { return bChatUIVisible; }
    
    // 보이스 채팅 활성화 여부 확인
    UFUNCTION(BlueprintPure, Category = "UI|채팅")
    bool IsVoiceChatActive() const { return bVoiceChatActive; }
    
    // 인벤토리 UI 표시 여부 확인
    UFUNCTION(BlueprintPure, Category = "UI|인벤토리")
    bool IsInventoryUIVisible() const { return bInventoryUIVisible; }
    
    // 메뉴 UI 표시 여부 확인
    UFUNCTION(BlueprintPure, Category = "UI|메뉴")
    bool IsMenuUIVisible() const { return bMenuUIVisible; }
    
    // 스킬 사용 함수 (블루프린트에서 호출 가능)
    UFUNCTION(BlueprintCallable, Category = "전투|스킬")
    void UseSkill(int32 SkillIndex); // 0=Q, 1=W, 2=E, 3=R
    
    // 블루프린트 이벤트들 (UI 처리용)
    UFUNCTION(BlueprintImplementableEvent, Category = "전투|스킬")
    void OnSkillUsed(int32 SkillIndex, const FVector& SkillDirection);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|채팅")
    void OnChatToggled(bool bIsOpen);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|인벤토리")
    void OnInventoryToggled(bool bIsOpen);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|메뉴")
    void OnMenuToggled(bool bIsOpen);
};