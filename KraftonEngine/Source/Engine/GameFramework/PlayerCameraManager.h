#pragma once

#include "Camera/CameraTypes.h"
#include "GameFramework/AActor.h"

class APlayerController;
class UCameraComponent;
class UCameraModeComponent;

class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)

	APlayerCameraManager() = default;
	~APlayerCameraManager() override = default;

	void InitDefaultComponents() override;
	void EndPlay() override;

	void SetOwningController(APlayerController* InController) { OwningController = InController; }
	APlayerController* GetOwningController() const { return OwningController; }

	UCameraComponent* GetOutputCameraComponent() const { return OutputCameraComponent; }

	void SetViewTarget(AActor* NewTarget);
	void SetViewTargetWithBlend(AActor* NewTarget, const FCameraBlendParams& Params);
	AActor* GetViewTarget() const { return ViewTarget; }

	void SetCameraMode(ECameraModeId NewMode, const FCameraBlendParams& Params);
	ECameraModeId GetCameraMode() const { return CameraMode; }

	void UpdateCamera(float DeltaTime);
	void SnapToTarget();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

private:
	bool CalcDesiredCameraView(float DeltaTime, FCameraView& OutView) const;
	bool CalcViewFromTargetCamera(float DeltaTime, FCameraView& OutView) const;
	FCameraView BlendViews(const FCameraView& From, const FCameraView& To, float Alpha, const FCameraBlendParams& Params) const;
	FCameraView SmoothView(const FCameraView& From, const FCameraView& To, float DeltaTime) const;
	float EvaluateBlendAlpha(float RawAlpha, ECameraBlendFunction Function) const;
	void SyncEditorValuesToRuntime(bool bApplyMode);
	void SyncRuntimeToEditorValues();

private:
	APlayerController* OwningController = nullptr;
	UCameraComponent* OutputCameraComponent = nullptr;

	AActor* ViewTarget = nullptr;
	ECameraModeId CameraMode = ECameraModeId::ThirdPerson;

	FCameraView CurrentView;
	FCameraView BlendFromView;

	FCameraBlendParams DefaultBlendParams;
	FCameraBlendParams ActiveBlendParams;
	float BlendElapsedTime = 0.0f;
	bool bIsBlending = false;

	bool bEnableContinuousSmoothing = true;
	float LocationLagSpeed = 12.0f;
	float RotationLagSpeed = 12.0f;
	float FOVLagSpeed = 10.0f;
	float OrthoWidthLagSpeed = 10.0f;

	int32 CameraModeValue = static_cast<int32>(ECameraModeId::ThirdPerson);
	int32 BlendFunctionValue = static_cast<int32>(ECameraBlendFunction::EaseInOut);
	int32 ProjectionSwitchModeValue = static_cast<int32>(ECameraProjectionSwitchMode::SwitchAtHalf);
};
