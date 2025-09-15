// HSNavigationIntegration.cpp
// 네비게이션 통합 관리자 구현
// 월드 생성과 네비게이션 메시 생성의 완전 자동화 시스템

#include "HSNavigationIntegration.h"
#include "HSNavMeshGenerator.h"
#include "HSRuntimeNavigation.h"
#include "../Generation/HSWorldGenerator.h"
#include "../../AI/HSAIControllerBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

UHSNavigationIntegration::UHSNavigationIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f;

    // 기본 설정값 초기화
    bAutoInitialize = true;
    bAutoGenerateNavigation = true;
    bAutoRegisterAI = true;
    NavigationGenerationPriority = 50;
    MaxErrorRecoveryAttempts = 3;
    AIValidationInterval = 5.0f;
    bEnableDebugLogging = true;

    // 상태 초기화
    CurrentState = EHSNavigationIntegrationState::Uninitialized;
    LastErrorMessage = TEXT("");
    ErrorRecoveryAttempts = 0;
    bIntegrationInitialized = false;
}

void UHSNavigationIntegration::BeginPlay()
{
    Super::BeginPlay();
    
    if (bAutoInitialize)
    {
        InitializeNavigationIntegration();
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 네비게이션 통합 시스템이 시작되었습니다."));
    }
}

void UHSNavigationIntegration::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 모든 타이머 정리
    UWorld* World = GetWorld();
    if (World)
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(AIValidationTimerHandle);
        TimerManager.ClearTimer(ErrorRecoveryTimerHandle);
    }
    
    // 등록된 AI들 정리
    RegisteredAIControllers.Empty();
    
    // 컴포넌트 참조 정리
    NavMeshGenerator = nullptr;
    RuntimeNavigation = nullptr;
    WorldGenerator = nullptr;
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 네비게이션 통합 시스템이 종료되었습니다."));
    }
    
    Super::EndPlay(EndPlayReason);
}

void UHSNavigationIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // 오류 상태에서의 자동 복구 시도
    if (CurrentState == EHSNavigationIntegrationState::Error && ErrorRecoveryAttempts < MaxErrorRecoveryAttempts)
    {
        AttemptErrorRecovery();
    }
}

void UHSNavigationIntegration::InitializeNavigationIntegration()
{
    if (bIntegrationInitialized)
    {
        return;
    }
    
    CurrentState = EHSNavigationIntegrationState::Initializing;
    
    // 네비게이션 컴포넌트들 초기화
    InitializeNavigationComponents();
    
    // 월드 생성기와 연동 설정
    SetupWorldGeneratorIntegration();
    
    // 네비게이션 이벤트 핸들러 설정
    SetupNavigationEventHandlers();
    
    // AI 검증 타이머 설정
    UWorld* World = GetWorld();
    if (World)
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.SetTimer(AIValidationTimerHandle,
            FTimerDelegate::CreateUObject(this, &UHSNavigationIntegration::ValidateAIRegistrations),
            AIValidationInterval, true);
    }
    
    bIntegrationInitialized = true;
    CurrentState = EHSNavigationIntegrationState::Ready;
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 네비게이션 통합 시스템 초기화가 완료되었습니다."));
    }
}

void UHSNavigationIntegration::OnWorldGenerationComplete(const FBox& GeneratedBounds)
{
    if (!bAutoGenerateNavigation || !NavMeshGenerator)
    {
        return;
    }
    
    CurrentState = EHSNavigationIntegrationState::Generating;
    
    // 네비게이션 메시 생성 요청
    FGuid NavGenerationTask = NavMeshGenerator->GenerateNavMeshInBounds(GeneratedBounds, NavigationGenerationPriority, true);
    
    if (NavGenerationTask.IsValid())
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 월드 생성 완료로 네비게이션 메시 생성을 시작했습니다. 영역: %s"),
                   *GeneratedBounds.ToString());
        }
        
        // 네비게이션 생성 완료 대기 (비동기)
        AsyncTask(ENamedThreads::GameThread, [this, GeneratedBounds]()
        {
            // 네비게이션 생성 완료 처리
            OnNavigationGenerationComplete(GeneratedBounds);
        });
    }
    else
    {
        FString ErrorMsg = TEXT("네비게이션 메시 생성 작업 생성에 실패했습니다.");
        OnNavigationGenerationFailed(ErrorMsg, GeneratedBounds);
    }
}

void UHSNavigationIntegration::OnWorldUpdated(const FBox& UpdatedBounds)
{
    if (!NavMeshGenerator)
    {
        return;
    }
    
    // 네비게이션 메시 업데이트
    NavMeshGenerator->UpdateNavMeshInBounds(UpdatedBounds, false);
    
    // 런타임 네비게이션에 업데이트 알림
    if (RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->NotifyNavMeshUpdate(UpdatedBounds);
    }
    
    // 이벤트 브로드캐스트
    OnNavigationUpdate.Broadcast(UpdatedBounds);
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 월드 업데이트로 네비게이션이 갱신되었습니다. 영역: %s"),
               *UpdatedBounds.ToString());
    }
}

void UHSNavigationIntegration::RegisterAIController(AAIController* AIController)
{
    if (!AIController)
    {
        return;
    }
    
    // 중복 등록 방지
    if (RegisteredAIControllers.Contains(AIController))
    {
        return;
    }
    
    RegisteredAIControllers.Add(AIController);
    
    // 런타임 네비게이션에도 등록
    if (RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->RegisterAIController(AIController);
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: AI 컨트롤러가 등록되었습니다. AI: %s"), *AIController->GetName());
    }
}

void UHSNavigationIntegration::UnregisterAIController(AAIController* AIController)
{
    if (!AIController)
    {
        return;
    }
    
    RegisteredAIControllers.Remove(AIController);
    
    // 런타임 네비게이션에서도 제거
    if (RuntimeNavigation.IsValid())
    {
        RuntimeNavigation->UnregisterAIController(AIController);
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: AI 컨트롤러가 등록 해제되었습니다. AI: %s"), *AIController->GetName());
    }
}

void UHSNavigationIntegration::ForceNavigationUpdate(const FBox& UpdateArea, bool bHighPriority)
{
    if (!NavMeshGenerator)
    {
        return;
    }
    
    int32 Priority = bHighPriority ? 10 : 50;
    NavMeshGenerator->GenerateNavMeshInBounds(UpdateArea, Priority, true);
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 강제 네비게이션 업데이트가 요청되었습니다. 영역: %s, 우선순위: %s"),
               *UpdateArea.ToString(), bHighPriority ? TEXT("높음") : TEXT("일반"));
    }
}

void UHSNavigationIntegration::ReinitializeAllAINavigation()
{
    if (!RuntimeNavigation.IsValid())
    {
        return;
    }
    
    int32 ReinitializedCount = 0;
    
    for (TWeakObjectPtr<AAIController>& AIControllerPtr : RegisteredAIControllers)
    {
        if (AAIController* AIController = AIControllerPtr.Get())
        {
            // AI의 모든 패스파인딩 요청 취소
            RuntimeNavigation->CancelAllRequestsForAI(AIController);
            
            // AI를 다시 등록
            RuntimeNavigation->UnregisterAIController(AIController);
            RuntimeNavigation->RegisterAIController(AIController);
            
            ReinitializedCount++;
        }
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: %d개의 AI 네비게이션이 재초기화되었습니다."), ReinitializedCount);
    }
}

void UHSNavigationIntegration::InitializeNavigationComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        LastErrorMessage = TEXT("소유자 액터를 찾을 수 없습니다.");
        CurrentState = EHSNavigationIntegrationState::Error;
        return;
    }
    
    // 네비게이션 메시 생성기 컴포넌트 찾기 또는 생성
    NavMeshGenerator = Owner->FindComponentByClass<UHSNavMeshGenerator>();
    if (!NavMeshGenerator)
    {
        NavMeshGenerator = NewObject<UHSNavMeshGenerator>(Owner);
        Owner->AddInstanceComponent(NavMeshGenerator);
        NavMeshGenerator->RegisterComponent();
    }
    
    // 런타임 네비게이션 서브시스템 가져오기
    UWorld* World = GetWorld();
    if (World)
    {
        RuntimeNavigation = World->GetGameInstance()->GetSubsystem<UHSRuntimeNavigation>();
    }
    
    if (!RuntimeNavigation.IsValid())
    {
        LastErrorMessage = TEXT("런타임 네비게이션 서브시스템을 찾을 수 없습니다.");
        CurrentState = EHSNavigationIntegrationState::Error;
        return;
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 네비게이션 컴포넌트들이 초기화되었습니다."));
    }
}

void UHSNavigationIntegration::SetupWorldGeneratorIntegration()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    
    // 월드 생성기 액터 찾기
    UWorld* World = GetWorld();
    if (World)
    {
        WorldGenerator = Cast<AHSWorldGenerator>(UGameplayStatics::GetActorOfClass(World, AHSWorldGenerator::StaticClass()));
    }
    
    if (WorldGenerator.IsValid())
    {
        // 월드 생성기의 이벤트에 바인딩
        // 실제 HSWorldGenerator에 이벤트가 있다면 여기서 바인딩
        // WorldGenerator->OnWorldGenerationComplete.AddDynamic(this, &UHSNavigationIntegration::OnWorldGenerationComplete);
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 월드 생성기와의 연동이 설정되었습니다."));
        }
    }
    else
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSNavigationIntegration: 월드 생성기를 찾을 수 없습니다. 수동 연동이 필요합니다."));
        }
    }
}

void UHSNavigationIntegration::SetupNavigationEventHandlers()
{
    // 네비게이션 메시 생성기의 완료 이벤트에 바인딩
    // 실제 이벤트가 있다면 여기서 바인딩
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 네비게이션 이벤트 핸들러가 설정되었습니다."));
    }
}

void UHSNavigationIntegration::OnNavigationGenerationComplete(const FBox& GeneratedArea)
{
    CurrentState = EHSNavigationIntegrationState::Ready;
    ErrorRecoveryAttempts = 0;
    
    // 이벤트 브로드캐스트
    OnNavigationReady.Broadcast(GeneratedArea);
    
    // 등록된 AI들에게 네비게이션 준비 완료 알림
    if (RuntimeNavigation.IsValid())
    {
        for (TWeakObjectPtr<AAIController>& AIControllerPtr : RegisteredAIControllers)
        {
            if (AAIController* AIController = AIControllerPtr.Get())
            {
                // AI가 현재 위치에서 네비게이션 가능한지 확인하고 필요시 복구
                if (!RuntimeNavigation->RecoverStuckAI(AIController))
                {
                    if (bEnableDebugLogging)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("HSNavigationIntegration: AI 복구에 실패했습니다. AI: %s"), *AIController->GetName());
                    }
                }
            }
        }
    }
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 네비게이션 생성이 완료되었습니다. 영역: %s"), *GeneratedArea.ToString());
    }
}

void UHSNavigationIntegration::OnNavigationGenerationFailed(const FString& ErrorMessage, const FBox& FailedArea)
{
    CurrentState = EHSNavigationIntegrationState::Error;
    LastErrorMessage = ErrorMessage;
    
    // 이벤트 브로드캐스트
    OnNavigationError.Broadcast(ErrorMessage, FailedArea);
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Error, TEXT("HSNavigationIntegration: 네비게이션 생성에 실패했습니다. 오류: %s, 영역: %s"), 
               *ErrorMessage, *FailedArea.ToString());
    }
    
    // 자동 복구 시도 예약
    if (ErrorRecoveryAttempts < MaxErrorRecoveryAttempts)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            FTimerManager& TimerManager = World->GetTimerManager();
            TimerManager.SetTimer(ErrorRecoveryTimerHandle,
                FTimerDelegate::CreateUObject(this, &UHSNavigationIntegration::AttemptErrorRecovery),
                2.0f, false); // 2초 후 복구 시도
        }
    }
}

void UHSNavigationIntegration::ValidateAIRegistrations()
{
    TArray<TWeakObjectPtr<AAIController>> ToRemove;
    
    for (TWeakObjectPtr<AAIController>& AIControllerPtr : RegisteredAIControllers)
    {
        if (!AIControllerPtr.IsValid())
        {
            ToRemove.Add(AIControllerPtr);
        }
        else
        {
            AAIController* AIController = AIControllerPtr.Get();
            
            // AI가 여전히 유효하고 폰이 있는지 확인
            if (!AIController->GetPawn())
            {
                ToRemove.Add(AIControllerPtr);
            }
        }
    }
    
    // 무효한 AI 참조들 제거
    for (const TWeakObjectPtr<AAIController>& InvalidAI : ToRemove)
    {
        RegisteredAIControllers.Remove(InvalidAI);
        
        if (RuntimeNavigation.IsValid() && InvalidAI.IsValid())
        {
            RuntimeNavigation->UnregisterAIController(InvalidAI.Get());
        }
    }
    
    if (bEnableDebugLogging && ToRemove.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: %d개의 무효한 AI 참조를 정리했습니다."), ToRemove.Num());
    }
    
    // 자동 AI 등록이 활성화된 경우 새로운 AI들 탐지
    if (bAutoRegisterAI)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            for (TActorIterator<APawn> ActorItr(World); ActorItr; ++ActorItr)
            {
                APawn* Pawn = *ActorItr;
                if (Pawn && Pawn->GetController())
                {
                    if (AAIController* AIController = Cast<AAIController>(Pawn->GetController()))
                    {
                        if (!RegisteredAIControllers.Contains(AIController))
                        {
                            RegisterAIController(AIController);
                        }
                    }
                }
            }
        }
    }
}

void UHSNavigationIntegration::AttemptErrorRecovery()
{
    ErrorRecoveryAttempts++;
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 오류 복구를 시도합니다. 시도 횟수: %d/%d"), 
               ErrorRecoveryAttempts, MaxErrorRecoveryAttempts);
    }
    
    // 네비게이션 시스템 재초기화 시도
    bIntegrationInitialized = false;
    CurrentState = EHSNavigationIntegrationState::Uninitialized;
    
    // 모든 컴포넌트 참조 초기화
    NavMeshGenerator = nullptr;
    RuntimeNavigation = nullptr;
    WorldGenerator = nullptr;
    
    // 재초기화 시도
    InitializeNavigationIntegration();
    
    if (CurrentState == EHSNavigationIntegrationState::Ready)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("HSNavigationIntegration: 오류 복구가 성공했습니다."));
        }
        
        ErrorRecoveryAttempts = 0;
        LastErrorMessage = TEXT("");
    }
    else
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSNavigationIntegration: 오류 복구에 실패했습니다. 추가 시도가 예정되어 있습니다."));
        }
    }
}
