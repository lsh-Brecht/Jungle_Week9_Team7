#pragma once

#include "Component/SceneComponent.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"

class UCameraComponent;
class FArchive;

class USpringArmComponent : public USceneComponent
{
public:
	DECLARE_CLASS(USpringArmComponent, USceneComponent)

	USpringArmComponent();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	float GetTargetArmLength() const { return TargetArmLength; }
	void SetTargetArmLength(float InLength);

	const FVector& GetTargetOffset() const { return TargetOffset; }
	void SetTargetOffset(const FVector& InOffset) { TargetOffset = InOffset; }

	const FVector& GetSocketOffset() const { return SocketOffset; }
	void SetSocketOffset(const FVector& InOffset) { SocketOffset = InOffset; }

	bool IsCollisionTestEnabled() const { return bDoCollisionTest; }
	void SetDoCollisionTest(bool bEnabled) { bDoCollisionTest = bEnabled; }

	float GetProbeSize() const { return ProbeSize; }
	void SetProbeSize(float InProbeSize);

	float GetMinArmLength() const { return MinArmLength; }
	void SetMinArmLength(float InLength) { MinArmLength = InLength; }

	bool UsesPawnControlRotation() const { return bUsePawnControlRotation; }
	void SetEnableCameraLag(bool bEnabled) { bEnableCameraLag = bEnabled; }

	float GetCameraLagSpeed() const { return CameraLagSpeed; }
	void SetCameraLagSpeed(float InSpeed);

	float GetCollisionPullInSpeed() const { return CollisionPullInSpeed; }
	void SetCollisionPullInSpeed(float InSpeed) { CollisionPullInSpeed = FMath::Max(InSpeed, 0.0f); }

	float GetCollisionRecoverSpeed() const { return CollisionRecoverSpeed; }
	void SetCollisionRecoverSpeed(float InSpeed) { CollisionRecoverSpeed = FMath::Max(InSpeed, 0.0f); }

	bool IsCollisionFixApplied() const { return bCollisionFixApplied; }
	float GetCurrentArmLength() const { return CurrentArmLength; }

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void NormalizeOption();
	FQuat GetDesiredRotation() const;
	FVector ComputeDesiredCameraLocation(const FVector& Pivot, const FQuat& DesiredRotation) const;
	FVector ResolveCollision(const FVector& Pivot, const FVector& DesiredRotation, float DeltaTime);
	void UpdateCameraChildren(const FVector& NewCameraLocation, const FQuat& CameraRotation);

private:
	float TargetArmLength = 5.0f;
	FVector TargetOffset = FVector::ZeroVector;
	FVector SocketOffset = FVector::ZeroVector;

	bool bDoCollisionTest = true;
	float ProbeSize = 0.2f;
	float MinArmLength = 0.25f;

	bool bUsePawnControlRotation = true;
	bool bInheritPitch = true;
	bool bInheritYaw = true;
	bool bInheritRoll = false;

	bool bEnableCameraLag = true;
	float CameraLagSpeed = 12.0f;
	float CollisionPullInSpeed = 1.0f;
	float CollisionRecoverSpeed = 0.1f;

	float CurrentArmLength = 1.0f;
	FVector LaggedCameraLocation = FVector::ZeroVector;
	bool bHasLaggedCameraLocation = false;
	bool bCollisionFixApplied = false;
};
