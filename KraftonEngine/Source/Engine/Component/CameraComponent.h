#pragma once

#include "Camera/CameraTypes.h"
#include "Collision/ConvexVolume.h"
#include "Component/SceneComponent.h"
#include "Engine/Core/RayTypes.h"
#include "Math/Matrix.h"

class UCameraComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCameraComponent, USceneComponent)

	UCameraComponent() = default;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void LookAt(const FVector& Target);

	FCameraView GetCameraView() const;
	void ApplyCameraView(const FCameraView& View);

	void SetProjectionSettings(const FCameraProjectionSettings& NewSettings);
	const FCameraProjectionSettings& GetProjectionSettings() const { return ProjectionSettings; }

	void SetFOV(float InFOV) { ProjectionSettings.FOV = InFOV; }
	void SetAspectRatio(float InAspectRatio) { if (InAspectRatio > 0.0f) ProjectionSettings.AspectRatio = InAspectRatio; }
	void SetNearPlane(float InNearZ) { ProjectionSettings.NearZ = InNearZ; }
	void SetFarPlane(float InFarZ) { ProjectionSettings.FarZ = InFarZ; }
	void SetOrthoWidth(float InWidth) { ProjectionSettings.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { ProjectionSettings.bIsOrthographic = bOrtho; }

	void OnResize(int32 Width, int32 Height);

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;
	FConvexVolume GetConvexVolume() const;

	float GetFOV() const { return ProjectionSettings.FOV; }
	float GetAspectRatio() const { return ProjectionSettings.AspectRatio; }
	float GetNearPlane() const { return ProjectionSettings.NearZ; }
	float GetFarPlane() const { return ProjectionSettings.FarZ; }
	float GetOrthoWidth() const { return ProjectionSettings.OrthoWidth; }
	bool IsOrthogonal() const { return ProjectionSettings.bIsOrthographic; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

private:
	FCameraProjectionSettings ProjectionSettings;
};
