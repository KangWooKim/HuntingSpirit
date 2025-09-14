// 사냥의 영혼(HuntingSpirit) 게임의 카메라 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSCameraComponent.generated.h"

/**
 * 탑다운 뷰 카메라 관리를 위한 컴포넌트
 * 플레이어 캐릭터의 추가 카메라 기능 제공
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HUNTINGSPIRIT_API UHSCameraComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    // 생성자
    UHSCameraComponent();

protected:
    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 카메라 관련 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float DefaultZoomDistance; // 기본 줌 거리
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float MinZoomDistance; // 최소 줌 거리
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float MaxZoomDistance; // 최대 줌 거리
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float ZoomSpeed; // 줌 속도
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float DefaultPitch; // 기본 카메라 피치 각도
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float MinPitch; // 최소 카메라 피치 각도
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float MaxPitch; // 최대 카메라 피치 각도
    
    // 카메라 부드러운 전환을 위한 변수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "카메라 상태")
    float CurrentZoomDistance; // 현재 줌 거리
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "카메라 상태")
    float TargetZoomDistance; // 목표 줌 거리
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "카메라 상태")
    float CurrentPitch; // 현재 피치 각도
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "카메라 상태")
    float TargetPitch; // 목표 피치 각도
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 설정")
    float SmoothSpeed; // 카메라 이동 부드러움 강도
    
    // 카메라 쉐이크 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 쉐이크")
    bool bEnableCameraShake; // 카메라 쉐이크 활성화 여부
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "카메라 쉐이크")
    float ShakeIntensity; // 카메라 쉐이크 강도

public:    
    // 프레임마다 호출
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 카메라 줌 인/아웃 함수
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void ZoomIn(float Amount); // 줌 인 - 가까이
    
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void ZoomOut(float Amount); // 줌 아웃 - 멀리
    
    // 카메라 각도 조정 함수
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void AdjustPitch(float Amount); // 카메라 피치 각도 조정
    
    // 카메라 줌 거리 설정 함수
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void SetZoomDistance(float NewDistance); // 줌 거리 직접 설정
    
    // 카메라 각도 설정 함수
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void SetCameraPitch(float NewPitch); // 카메라 피치 직접 설정
    
    // 카메라 기본 설정으로 리셋
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void ResetCamera(); // 카메라 초기 설정으로 복원
    
    // 카메라 쉐이크 함수
    UFUNCTION(BlueprintCallable, Category = "카메라")
    void ShakeCamera(float Intensity, float Duration); // 카메라 흔들기 효과 적용

private:
    // 내부 함수: 카메라 업데이트
    void UpdateCameraTransform(float DeltaTime); // 카메라 위치/각도 업데이트
    
    // 카메라 쉐이크 관련 변수
    float CurrentShakeTime; // 현재 쉐이크 진행 시간
    float MaxShakeTime;     // 최대 쉐이크 시간
    float CurrentShakeIntensity; // 현재 쉐이크 강도
    bool bIsShaking;        // 쉐이크 진행 중 여부
    
    // 카메라 쉐이크 처리 함수
    void ProcessCameraShake(float DeltaTime); // 쉐이크 진행 처리
    FVector CalculateShakeOffset() const; // 쉐이크 오프셋 계산
};