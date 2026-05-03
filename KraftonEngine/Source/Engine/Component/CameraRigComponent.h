#pragma once

#include "Camera/CameraTypes.h"
#include "Component/CameraModeComponent.h"
#include "Math/Vector.h"

class AActor;
class APlayerController;

enum class ECameraRigProjectionMode : uint8
{
	Orthographic,
	Perspective
};

class UCameraRigComponent : public UCameraModeComponent
{
public:
	DECLARE_CLASS(UCameraRigComponent, UCameraModeComponent)

	UCameraRigComponent() = default;
	~UCameraRigComponent() override = default;

	void BeginPlay() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;
	void RemapActorReferences(const TMap<uint32, uint32>& ActorUUIDRemap) override;

	ECameraModeId GetCameraModeId() const override;

	bool CalcCameraView(
		APlayerController* Controller,
		AActor* ViewTarget,
		float DeltaTime,
		FCameraView& OutView) override;

	void SetLookAheadInput(const FVector2& InInput);
	void SetProjectionMode(ECameraRigProjectionMode InMode);
	ECameraRigProjectionMode GetProjectionMode() const { return static_cast<ECameraRigProjectionMode>(ProjectionMode); }
	bool IsOrthographic() const { return GetProjectionMode() == ECameraRigProjectionMode::Orthographic; }
	bool IsPerspective() const { return GetProjectionMode() == ECameraRigProjectionMode::Perspective; }
	void ToggleProjectionMode();
	void SnapInternalState(AActor* ViewTarget);

private:
	AActor* ResolveTargetActor(APlayerController* Controller, AActor* ViewTarget) const;
	FVector ComputeFocusPoint(APlayerController* Controller, AActor* ViewTarget, float DeltaTime);
	FVector ComputeDesiredCameraLocation(APlayerController* Controller, AActor* ViewTarget, const FVector& FocusPoint) const;
	FVector ComputeOrthographicOffset() const;
	FVector ComputePerspectiveOffset(AActor* Target) const;
	FVector ComputeMouseLookAheadWorld(APlayerController* Controller) const;
	FQuat MakeLookAtRotationQuat(const FVector& Location, const FVector& Target) const;

private:
	int32 ProjectionMode = static_cast<int32>(ECameraRigProjectionMode::Perspective);
	uint32 TargetActorUUID = 0;

	FVector TargetOffset = FVector(0.0f, 0.0f, 0.8f);

	FVector OrthographicViewOffset = FVector(-3.0f, 3.0f, 3.0f);
	float OrthographicWidth = 12.0f;

	float PerspectiveBackDistance = 5.0f;
	float PerspectiveHeight = 3.0f;
	float PerspectiveSideOffset = 0.0f;
	float PerspectiveFOV = 3.14159265f / 3.0f;

	float NearZ = 0.01f;
	float FarZ = 200.0f;

	float LookAheadLagSpeed = 8.0f;
	bool bEnableMouseLookAhead = true;
	float MouseLookAheadDistance = 1.0f;
	FVector2 LookAheadInput = FVector2(0.0f, 0.0f);
	FVector SmoothedLookAheadWorld = FVector::ZeroVector;
	FVector StableFocusPoint = FVector::ZeroVector;
	float StableFocusZ = 0.0f;
	bool bHasInitializedStableFocus = false;

	bool bStabilizeVerticalInOrthographic = true;
	float VerticalDeadZone = 0.4f;
	float VerticalFollowStrength = 0.15f;
	float VerticalLagSpeed = 2.0f;
};
