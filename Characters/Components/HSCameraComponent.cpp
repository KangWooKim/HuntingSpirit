/**
 * @file HSCameraComponent.cpp
 * @brief HuntingSpirit 게임의 탑다운 뷰 카메라 관리 컴포넌트 구현
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#include "HSCameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/SceneComponent.h"

UHSCameraComponent::UHSCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    DefaultZoomDistance = 1000.0f;
    MinZoomDistance = 500.0f;
    MaxZoomDistance = 2000.0f;
    ZoomSpeed = 100.0f;
    
    DefaultPitch = -60.0f;
    MinPitch = -80.0f;
    MaxPitch = -30.0f;
    
    CurrentZoomDistance = DefaultZoomDistance;
    TargetZoomDistance = DefaultZoomDistance;
    CurrentPitch = DefaultPitch;
    TargetPitch = DefaultPitch;
    
    SmoothSpeed = 5.0f;
    
    bEnableCameraShake = true;
    ShakeIntensity = 3.0f;
    bIsShaking = false;
    CurrentShakeTime = 0.0f;
    MaxShakeTime = 0.0f;
    CurrentShakeIntensity = 0.0f;
}

void UHSCameraComponent::BeginPlay()
{
    Super::BeginPlay();
    
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        USpringArmComponent* FoundCameraBoom = nullptr;
        
        TArray<USpringArmComponent*> SpringArmComponents;
        OwnerCharacter->GetComponents<USpringArmComponent>(SpringArmComponents);
        
        if (SpringArmComponents.Num() > 0)
        {
            FoundCameraBoom = SpringArmComponents[0];
            
            if (FoundCameraBoom)
            {
                CurrentZoomDistance = FoundCameraBoom->TargetArmLength;
                TargetZoomDistance = CurrentZoomDistance;
                
                FRotator ArmRotation = FoundCameraBoom->GetRelativeRotation();
                CurrentPitch = ArmRotation.Pitch;
                TargetPitch = CurrentPitch;
            }
        }
    }
}

void UHSCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateCameraTransform(DeltaTime);
    
    if (bIsShaking)
    {
        ProcessCameraShake(DeltaTime);
    }
}

void UHSCameraComponent::UpdateCameraTransform(float DeltaTime)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        return;
    }
    
    USpringArmComponent* CameraBoom = nullptr;
    TArray<USpringArmComponent*> SpringArmComponents;
    OwnerCharacter->GetComponents<USpringArmComponent>(SpringArmComponents);
    
    if (SpringArmComponents.Num() > 0)
    {
        CameraBoom = SpringArmComponents[0];
    }
    
    if (CameraBoom)
    {
        CurrentZoomDistance = FMath::FInterpTo(CurrentZoomDistance, TargetZoomDistance, DeltaTime, SmoothSpeed);
        CameraBoom->TargetArmLength = CurrentZoomDistance;
        
        CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaTime, SmoothSpeed);
        FRotator NewRotation = CameraBoom->GetRelativeRotation();
        NewRotation.Pitch = CurrentPitch;
        CameraBoom->SetRelativeRotation(NewRotation);
        
        if (bIsShaking)
        {
            UCameraComponent* Camera = nullptr;
            TArray<UCameraComponent*> CameraComponents;
            OwnerCharacter->GetComponents<UCameraComponent>(CameraComponents);
            
            if (CameraComponents.Num() > 0)
            {
                Camera = CameraComponents[0];
                
                if (Camera)
                {
                    FVector ShakeOffset = CalculateShakeOffset();
                    Camera->SetRelativeLocation(ShakeOffset);
                }
            }
        }
    }
}

void UHSCameraComponent::ZoomIn(float Amount)
{
    TargetZoomDistance = FMath::Clamp(TargetZoomDistance - (Amount * ZoomSpeed), MinZoomDistance, MaxZoomDistance);
}

void UHSCameraComponent::ZoomOut(float Amount)
{
    TargetZoomDistance = FMath::Clamp(TargetZoomDistance + (Amount * ZoomSpeed), MinZoomDistance, MaxZoomDistance);
}

void UHSCameraComponent::AdjustPitch(float Amount)
{
    TargetPitch = FMath::Clamp(TargetPitch + Amount, MinPitch, MaxPitch);
}

void UHSCameraComponent::SetZoomDistance(float NewDistance)
{
    TargetZoomDistance = FMath::Clamp(NewDistance, MinZoomDistance, MaxZoomDistance);
}

void UHSCameraComponent::SetCameraPitch(float NewPitch)
{
    TargetPitch = FMath::Clamp(NewPitch, MinPitch, MaxPitch);
}

void UHSCameraComponent::ResetCamera()
{
    TargetZoomDistance = DefaultZoomDistance;
    TargetPitch = DefaultPitch;
}

void UHSCameraComponent::ShakeCamera(float Intensity, float Duration)
{
    if (bEnableCameraShake)
    {
        bIsShaking = true;
        CurrentShakeTime = 0.0f;
        MaxShakeTime = Duration;
        CurrentShakeIntensity = FMath::Min(Intensity, ShakeIntensity);
    }
}

void UHSCameraComponent::ProcessCameraShake(float DeltaTime)
{
    if (bIsShaking)
    {
        CurrentShakeTime += DeltaTime;
        
        if (CurrentShakeTime >= MaxShakeTime)
        {
            bIsShaking = false;
            
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
            float TimeRatio = 1.0f - (CurrentShakeTime / MaxShakeTime);
            CurrentShakeIntensity = ShakeIntensity * TimeRatio;
        }
    }
}

FVector UHSCameraComponent::CalculateShakeOffset() const
{
    if (!bIsShaking || CurrentShakeIntensity <= 0.0f)
    {
        return FVector::ZeroVector;
    }
    
    float ShakeOffsetX = FMath::RandRange(-1.0f, 1.0f) * CurrentShakeIntensity;
    float ShakeOffsetY = FMath::RandRange(-1.0f, 1.0f) * CurrentShakeIntensity;
    float ShakeOffsetZ = FMath::RandRange(-1.0f, 1.0f) * CurrentShakeIntensity * 0.5f;
    
    return FVector(ShakeOffsetX, ShakeOffsetY, ShakeOffsetZ);
}