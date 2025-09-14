// 사냥의 영혼(HuntingSpirit) 게임의 카메라 컴포넌트 구현

#include "HSCameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/SceneComponent.h"

// 생성자
UHSCameraComponent::UHSCameraComponent()
{
    // 이 컴포넌트가 매 프레임 틱을 받도록 설정
    PrimaryComponentTick.bCanEverTick = true;

    // 기본 값 설정
    DefaultZoomDistance = 1000.0f; // 기본 카메라 거리
    MinZoomDistance = 500.0f;      // 최소 카메라 거리
    MaxZoomDistance = 2000.0f;     // 최대 카메라 거리
    ZoomSpeed = 100.0f;            // 줌 속도
    
    DefaultPitch = -60.0f;         // 탑다운 뷰 기본 각도 (60도 아래로)
    MinPitch = -80.0f;             // 가장 위에서 내려다보는 각도
    MaxPitch = -30.0f;             // 가장 낮은 각도
    
    CurrentZoomDistance = DefaultZoomDistance;
    TargetZoomDistance = DefaultZoomDistance;
    CurrentPitch = DefaultPitch;
    TargetPitch = DefaultPitch;
    
    SmoothSpeed = 5.0f;            // 카메라 움직임 부드러움 정도
    
    // 카메라 쉐이크 설정
    bEnableCameraShake = true;     // 카메라 쉐이크 활성화
    ShakeIntensity = 3.0f;         // 기본 쉐이크 강도
    bIsShaking = false;            // 쉐이크 비활성화 상태로 시작
    CurrentShakeTime = 0.0f;
    MaxShakeTime = 0.0f;
    CurrentShakeIntensity = 0.0f;
}

// 게임 시작 시 호출
void UHSCameraComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 소유한 액터가 캐릭터이고, 카메라 붐과 카메라가 있는지 확인
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        // 카메라 초기 설정을 위한 컴포넌트 찾기
        USpringArmComponent* FoundCameraBoom = nullptr;
        
        // 소유 액터에서 카메라 컴포넌트 찾기
        TArray<USpringArmComponent*> SpringArmComponents;
        OwnerCharacter->GetComponents<USpringArmComponent>(SpringArmComponents);
        
        if (SpringArmComponents.Num() > 0)
        {
            FoundCameraBoom = SpringArmComponents[0];
            
            if (FoundCameraBoom)
            {
                // 현재 설정 가져오기
                CurrentZoomDistance = FoundCameraBoom->TargetArmLength;
                TargetZoomDistance = CurrentZoomDistance;
                
                // 회전 값 가져오기
                FRotator ArmRotation = FoundCameraBoom->GetRelativeRotation();
                CurrentPitch = ArmRotation.Pitch;
                TargetPitch = CurrentPitch;
            }
        }
    }
}

// 프레임마다 호출
void UHSCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 카메라 업데이트
    UpdateCameraTransform(DeltaTime);
    
    // 카메라 쉐이크 처리
    if (bIsShaking)
    {
        ProcessCameraShake(DeltaTime);
    }
}

// 카메라 업데이트 함수
void UHSCameraComponent::UpdateCameraTransform(float DeltaTime)
{
    // 소유 액터가 캐릭터인지 확인
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        return;
    }
    
    // 카메라 붐 컴포넌트 찾기
    USpringArmComponent* CameraBoom = nullptr;
    TArray<USpringArmComponent*> SpringArmComponents;
    OwnerCharacter->GetComponents<USpringArmComponent>(SpringArmComponents);
    
    if (SpringArmComponents.Num() > 0)
    {
        CameraBoom = SpringArmComponents[0];
    }
    
    if (CameraBoom)
    {
        // 부드러운 카메라 줌 적용
        CurrentZoomDistance = FMath::FInterpTo(CurrentZoomDistance, TargetZoomDistance, DeltaTime, SmoothSpeed);
        CameraBoom->TargetArmLength = CurrentZoomDistance;
        
        // 부드러운 카메라 각도 적용
        CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaTime, SmoothSpeed);
        FRotator NewRotation = CameraBoom->GetRelativeRotation();
        NewRotation.Pitch = CurrentPitch;
        CameraBoom->SetRelativeRotation(NewRotation);
        
        // 카메라 쉐이크 효과 적용
        if (bIsShaking)
        {
            // 카메라 컴포넌트 찾기
            UCameraComponent* Camera = nullptr;
            TArray<UCameraComponent*> CameraComponents;
            OwnerCharacter->GetComponents<UCameraComponent>(CameraComponents);
            
            if (CameraComponents.Num() > 0)
            {
                Camera = CameraComponents[0];
                
                if (Camera)
                {
                    // 쉐이크 오프셋 계산 및 적용
                    FVector ShakeOffset = CalculateShakeOffset();
                    Camera->SetRelativeLocation(ShakeOffset);
                }
            }
        }
    }
}

// 카메라 줌 인
void UHSCameraComponent::ZoomIn(float Amount)
{
    // 줌 인 - 카메라를 캐릭터에 가깝게
    TargetZoomDistance = FMath::Clamp(TargetZoomDistance - (Amount * ZoomSpeed), MinZoomDistance, MaxZoomDistance);
}

// 카메라 줌 아웃
void UHSCameraComponent::ZoomOut(float Amount)
{
    // 줌 아웃 - 카메라를 캐릭터에서 멀리
    TargetZoomDistance = FMath::Clamp(TargetZoomDistance + (Amount * ZoomSpeed), MinZoomDistance, MaxZoomDistance);
}

// 카메라 각도 조정
void UHSCameraComponent::AdjustPitch(float Amount)
{
    // 카메라 피치 각도 조정 (위/아래 각도)
    TargetPitch = FMath::Clamp(TargetPitch + Amount, MinPitch, MaxPitch);
}

// 카메라 줌 거리 설정
void UHSCameraComponent::SetZoomDistance(float NewDistance)
{
    // 카메라 줌 거리 직접 설정 (제한 범위 내에서)
    TargetZoomDistance = FMath::Clamp(NewDistance, MinZoomDistance, MaxZoomDistance);
}

// 카메라 각도 설정
void UHSCameraComponent::SetCameraPitch(float NewPitch)
{
    // 카메라 피치 각도 직접 설정 (제한 범위 내에서)
    TargetPitch = FMath::Clamp(NewPitch, MinPitch, MaxPitch);
}

// 카메라 초기화
void UHSCameraComponent::ResetCamera()
{
    // 카메라 기본 설정으로 복원
    TargetZoomDistance = DefaultZoomDistance;
    TargetPitch = DefaultPitch;
}

// 카메라 쉐이크 시작
void UHSCameraComponent::ShakeCamera(float Intensity, float Duration)
{
    // 카메라 쉐이크 활성화 옵션이 켜져 있는지 확인
    if (bEnableCameraShake)
    {
        bIsShaking = true;
        CurrentShakeTime = 0.0f;
        MaxShakeTime = Duration;
        CurrentShakeIntensity = FMath::Min(Intensity, ShakeIntensity); // 최대 강도 제한
    }
}

// 카메라 쉐이크 처리
void UHSCameraComponent::ProcessCameraShake(float DeltaTime)
{
    if (bIsShaking)
    {
        // 쉐이크 시간 진행
        CurrentShakeTime += DeltaTime;
        
        // 쉐이크 시간이 끝나면 쉐이크 중지
        if (CurrentShakeTime >= MaxShakeTime)
        {
            bIsShaking = false;
            
            // 카메라 위치 리셋
            ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
            if (OwnerCharacter)
            {
                TArray<UCameraComponent*> CameraComponents;
                OwnerCharacter->GetComponents<UCameraComponent>(CameraComponents);
                
                if (CameraComponents.Num() > 0)
                {
                    UCameraComponent* Camera = CameraComponents[0];
                    if (Camera)
                    {
                        Camera->SetRelativeLocation(FVector::ZeroVector);
                    }
                }
            }
        }
        else
        {
            // 시간에 따른 쉐이크 강도 감소 (시간이 지날수록 더 약하게 흔들림)
            float TimeRatio = 1.0f - (CurrentShakeTime / MaxShakeTime);
            CurrentShakeIntensity = ShakeIntensity * TimeRatio;
        }
    }
}

// 쉐이크 오프셋 계산
FVector UHSCameraComponent::CalculateShakeOffset() const
{
    if (!bIsShaking || CurrentShakeIntensity <= 0.0f)
    {
        return FVector::ZeroVector;
    }
    
    // 랜덤한 쉐이크 오프셋 계산
    float ShakeOffsetX = FMath::RandRange(-1.0f, 1.0f) * CurrentShakeIntensity;
    float ShakeOffsetY = FMath::RandRange(-1.0f, 1.0f) * CurrentShakeIntensity;
    float ShakeOffsetZ = FMath::RandRange(-1.0f, 1.0f) * CurrentShakeIntensity * 0.5f; // Z축은 덜 흔들림
    
    return FVector(ShakeOffsetX, ShakeOffsetY, ShakeOffsetZ);
}