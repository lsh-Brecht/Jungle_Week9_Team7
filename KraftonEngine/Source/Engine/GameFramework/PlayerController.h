#pragma once
#include "Camera/CameraTypes.h"
#include "GameFramework/AActor.h"
#include "Math/Rotator.h"

class APawn;
class APlayerCameraManager;
class UCameraComponent;
class UControllerInputComponent;
class FArchive;
struct FControllerMovementInput;

class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController() = default;
	~APlayerController() override = default;

	void Serialize(FArchive& Ar) override;
	void InitDefaultComponents() override;
	void EndPlay() override;

	void Possess(AActor* InActor);
	void UnPossess();
	AActor* GetPossessedActor() const;

	void SetViewTarget(AActor* InViewTarget);
	void SetViewTargetWithBlend(AActor* InViewTarget, const FCameraBlendParams& Params);
	AActor* GetViewTarget() const;

	void SetCameraMode(ECameraModeId InMode, const FCameraBlendParams& Params);
	ECameraModeId GetCameraMode() const;

	void AddYawInput(float Value);
	void AddPitchInput(float Value, float MinPitch = -89.0f, float MaxPitch = 89.0f);
	bool ApplyControllerMovementInput(const FControllerMovementInput& Input);

	APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }
	UCameraComponent* ResolveViewCamera() const;
	UControllerInputComponent* FindControllerInputComponent() const;

	FRotator GetControlRotation() const { return ControlRotation; }
	void SetControlRotation(const FRotator& InRotation) { ControlRotation = InRotation; }

private:
	AActor* PossessedActor = nullptr;
	AActor* ViewTarget = nullptr;
	FRotator ControlRotation;
	APlayerCameraManager* PlayerCameraManager = nullptr;
};
