#pragma once

#include "Component/ActorComponent.h"
#include "Math/Vector.h"

class AActor;
class UCameraComponent;

enum class ECameraRigProjectionMode : uint8
{
	Orthographic,
	Perspective
};

class UCameraRigComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UCameraRigComponent, UActorComponent)

	UCameraRigComponent() = default;
	~UCameraRigComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	void SetTargetActor(AActor* InTargetActor) { TargetActor = InTargetActor; }
	AActor* GetTargetActor() const { return TargetActor; }
	void SetCameraComponent(UCameraComponent* InCameraComponent) { CameraComponent = InCameraComponent; }
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }
	void SetLookAheadInput(const FVector2& InInput);
	void SetProjectionMode(ECameraRigProjectionMode InMode);
	ECameraRigProjectionMode GetProjectionMode() const { return ProjectionMode; }
	bool IsOrthographic() const { return ProjectionMode == ECameraRigProjectionMode::Orthographic; }
	bool IsPerspective() const { return ProjectionMode == ECameraRigProjectionMode::Perspective; }
	void ToggleProjectionMode();
	void ApplyProjectionMode();
	void SnapToTarget();

protected:
	UCameraComponent* ResolveCameraComponent();
	AActor* ResolveTargetActor() const;
	FVector ComputeFocusPoint(float DeltaTime);
	FVector ComputeDesiredCameraLocation(const FVector& FocusPoint) const;
	FVector ComputeOrthographicOffset() const;
	FVector ComputePerspectiveOffset() const;
	FVector ComputeMouseLookAheadWorld() const;
	void UpdateCamera(float DeltaTime);

	ECameraRigProjectionMode ProjectionMode = ECameraRigProjectionMode::Orthographic;
	AActor* TargetActor = nullptr;
	UCameraComponent* CameraComponent = nullptr;

	FVector TargetOffset = FVector(0.0f, 0.0f, 0.8f);

	FVector OrthographicViewOffset = FVector(-3.0f, 3.0f, 3.0f);
	float OrthographicWidth = 12.0f;

	float PerspectiveBackDistance = 5.0f;
	float PerspectiveHeight = 3.0f;
	float PerspectiveSideOffset = 0.0f;
	float PerspectiveFOV = 3.14159265f / 3.0f;

	float NearZ = 0.01f;
	float FarZ = 200.0f;

	float PositionLagSpeed = 6.0f;
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

	bool bSnapOnProjectionModeChange = false;
};
