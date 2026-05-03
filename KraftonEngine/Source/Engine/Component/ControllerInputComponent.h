#pragma once

#include "Component/ActorComponent.h"
#include "Math/Rotator.h"

class APlayerController;
class UCameraComponent;
struct FInputSystemSnapshot;
class FArchive;

enum class EControllerMovementFrame : int32
{
	World = 0,
	Camera = 1,
};

enum class EControllerLookMode : int32
{
	Auto = 0,
	CameraOnly = 1,
	PawnYawPawnPitch = 2,
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
	bool ApplyLookInput(APlayerController* Controller, UCameraComponent* FallbackCamera, const FInputSystemSnapshot& Snapshot);

	EControllerMovementFrame GetMovementFrame() const { return static_cast<EControllerMovementFrame>(MovementFrame); }
	void SetMovementFrame(EControllerMovementFrame InFrame);

	EControllerLookMode GetLookMode() const { return static_cast<EControllerLookMode>(LookMode); }
	void SetLookMode(EControllerLookMode InMode);

	float GetMoveSpeed() const { return MoveSpeed; }
	void SetMoveSpeed(float InSpeed);

	float GetSprintMultiplier() const { return SprintMultiplier; }
	void SetSprintMultiplier(float InMultiplier);

	float GetLookSensitivity() const { return LookSensitivity; }
	void SetLookSensitivity(float InSensitivity);

	float GetMinPitch() const { return MinPitch; }
	void SetMinPitch(float InMinPitch);

	float GetMaxPitch() const { return MaxPitch; }
	void SetMaxPitch(float InMaxPitch);

	void RemapActorReferences(const TMap<uint32, uint32>& ActorUUIDRemap) override;

	uint32 PossessedActorUUID = 0;
	
private:
	UCameraComponent* ResolveTargetCamera(APlayerController* Controller, UCameraComponent* FallbackCamera) const;
	void NormalizeOptions();

private:
	int32 MovementFrame = static_cast<int32>(EControllerMovementFrame::Camera);
	int32 LookMode = static_cast<int32>(EControllerLookMode::Auto);
	float MoveSpeed = 10.0f;
	float SprintMultiplier = 2.5f;
	float LookSensitivity = 0.08f;
	float MinPitch = -89.0f;
	float MaxPitch = 89.0f;
};
