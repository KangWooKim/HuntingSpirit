/**
 * @file HSCameraComponent.h
 * @brief HuntingSpirit 게임의 탑다운 뷰 카메라 관리 컴포넌트
 * @author KangWooKim (https://github.com/KangWooKim)
 * @date 2025-06-03
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSCameraComponent.generated.h"

/**
 * @class UHSCameraComponent
 * @brief 탑다운 뷰 카메라 관리를 위한 컴포넌트
 * @details 플레이어 캐릭터의 카메라 줌, 각도 조정, 쉐이크 효과 등을 담당합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSCameraComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    /**
     * @brief 생성자
     */
    UHSCameraComponent();

protected:
    /**
     * @brief 게임 시작 시 호출되는 초기화 함수
     * @details 카메라 관련 컴포넌트를 찾고 초기 설정을 적용합니다.
     */
    virtual void BeginPlay() override;

    /** 기본 줌 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float DefaultZoomDistance;
    
    /** 최소 줌 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float MinZoomDistance;
    
    /** 최대 줌 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float MaxZoomDistance;
    
    /** 줌 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float ZoomSpeed;
    
    /** 기본 카메라 피치 각도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float DefaultPitch;
    
    /** 최소 카메라 피치 각도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float MinPitch;
    
    /** 최대 카메라 피치 각도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float MaxPitch;
    
    /** 현재 줌 거리 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float CurrentZoomDistance;
    
    /** 목표 줌 거리 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float TargetZoomDistance;
    
    /** 현재 피치 각도 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float CurrentPitch;
    
    /** 목표 피치 각도 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float TargetPitch;
    
    /** 카메라 이동 부드러움 강도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float SmoothSpeed;
    
    /** 카메라 쉐이크 활성화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
    bool bEnableCameraShake;
    
    /** 카메라 쉐이크 강도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
    float ShakeIntensity;

public:    
    /**
     * @brief 매 프레임 호출되는 틱 함수
     * @param DeltaTime 이전 프레임으로부터의 시간 간격
     * @param TickType 틱 타입
     * @param ThisTickFunction 이 컴포넌트의 틱 함수
     */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * @brief 카메라를 가까이 줌합니다.
     * @param Amount 줌 조정량
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ZoomIn(float Amount);
    
    /**
     * @brief 카메라를 멀리 줌합니다.
     * @param Amount 줌 조정량
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ZoomOut(float Amount);
    
    /**
     * @brief 카메라 피치 각도를 조정합니다.
     * @param Amount 각도 조정량
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void AdjustPitch(float Amount);
    
    /**
     * @brief 카메라 줌 거리를 직접 설정합니다.
     * @param NewDistance 새로운 줌 거리
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetZoomDistance(float NewDistance);
    
    /**
     * @brief 카메라 피치 각도를 직접 설정합니다.
     * @param NewPitch 새로운 피치 각도
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraPitch(float NewPitch);
    
    /**
     * @brief 카메라를 기본 설정으로 리셋합니다.
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ResetCamera();
    
    /**
     * @brief 카메라 쉐이크 효과를 적용합니다.
     * @param Intensity 쉐이크 강도
     * @param Duration 쉐이크 지속 시간
     */
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ShakeCamera(float Intensity, float Duration);

private:
    /**
     * @brief 카메라 위치와 각도를 업데이트합니다.
     * @param DeltaTime 이전 프레임으로부터의 시간 간격
     */
    void UpdateCameraTransform(float DeltaTime);
    
    /** 현재 쉐이크 진행 시간 */
    float CurrentShakeTime;
    
    /** 최대 쉐이크 시간 */
    float MaxShakeTime;
    
    /** 현재 쉐이크 강도 */
    float CurrentShakeIntensity;
    
    /** 쉐이크 진행 중 여부 */
    bool bIsShaking;
    
    /**
     * @brief 카메라 쉐이크를 처리합니다.
     * @param DeltaTime 이전 프레임으로부터의 시간 간격
     */
    void ProcessCameraShake(float DeltaTime);
    
    /**
     * @brief 쉐이크 오프셋을 계산합니다.
     * @return 계산된 쉐이크 오프셋
     */
    FVector CalculateShakeOffset() const;
};