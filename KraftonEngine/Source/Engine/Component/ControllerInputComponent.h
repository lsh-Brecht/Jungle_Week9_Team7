#pragma once

#include "Component/ActorComponent.h"
#include "Math/Rotator.h"

class APlayerController;
class UCameraComponent;
struct FInputSystemSnapshot;
class FArchive;

enum class EControllerMovementBasis : int32
{
	World = 0,
	ControlRotation = 1,
	ViewCamera = 2,
};

class UControllerInputComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UControllerInputComponent, UActorComponent)

	UControllerInputComponent() = default;
	~UControllerInputComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	bool ApplyInput(APlayerController* Controller, UCameraComponent* FallbackCamera, float DeltaTime, const FInputSystemSnapshot& Snapshot);
	bool ApplyMovementInput(APlayerController* Controller, UCameraComponent* FallbackCamera, float DeltaTime, const FInputSystemSnapshot& Snapshot);
	bool ApplyLookInput(APlayerController* Controller, const FInputSystemSnapshot& Snapshot);

	EControllerMovementBasis GetMovementBasis() const { return static_cast<EControllerMovementBasis>(MovementBasis); }
	void SetMovementBasis(EControllerMovementBasis InBasis);

	float GetMoveSpeed() const { return MoveSpeed; }
	void SetMoveSpeed(float InSpeed);

	float GetSprintMultiplier() const { return SprintMultiplier; }
	void SetSprintMultiplier(float InMultiplier);

	float GetLookSensitivityX() const { return LookSensitivityX; }
	float GetLookSensitivityY() const { return LookSensitivityY; }
	void SetLookSensitivityX(float InSensitivity);
	void SetLookSensitivityY(float InSensitivity);
	void SetLookSensitivity(float InSensitivity);

	float GetMinPitch() const { return MinPitch; }
	void SetMinPitch(float InMinPitch);

	float GetMaxPitch() const { return MaxPitch; }
	void SetMaxPitch(float InMaxPitch);

private:
	UCameraComponent* ResolveViewCamera(APlayerController* Controller, UCameraComponent* FallbackCamera) const;
	void BuildMovementBasis(APlayerController* Controller, UCameraComponent* FallbackCamera, FVector& OutForward, FVector& OutRight) const;
	void NormalizeOptions();

private:
	int32 MovementBasis = static_cast<int32>(EControllerMovementBasis::ViewCamera);
	float MoveSpeed = 10.0f;
	float SprintMultiplier = 2.5f;
	float LookSensitivityX = 0.08f;
	float LookSensitivityY = 0.08f;
	bool bInvertY = false;
	float MinPitch = -89.0f;
	float MaxPitch = 89.0f;
};
