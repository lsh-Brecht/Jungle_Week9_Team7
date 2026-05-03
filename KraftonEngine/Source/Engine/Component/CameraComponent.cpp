#include "Component/CameraComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

IMPLEMENT_CLASS(UCameraComponent, USceneComponent)

void UCameraComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << ProjectionSettings.FOV;
	Ar << ProjectionSettings.AspectRatio;
	Ar << ProjectionSettings.NearZ;
	Ar << ProjectionSettings.FarZ;
	Ar << ProjectionSettings.OrthoWidth;
	Ar << ProjectionSettings.bIsOrthographic;
}

FMatrix UCameraComponent::GetViewMatrix() const
{
	UpdateWorldMatrix();
	return FMatrix::MakeViewMatrix(GetRightVector(), GetUpVector(), GetForwardVector(), GetWorldLocation());
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	if (!ProjectionSettings.bIsOrthographic)
	{
		return FMatrix::PerspectiveFovLH(
			ProjectionSettings.FOV,
			ProjectionSettings.AspectRatio,
			ProjectionSettings.NearZ,
			ProjectionSettings.FarZ);
	}

	const float HalfW = ProjectionSettings.OrthoWidth * 0.5f;
	const float HalfH = HalfW / ProjectionSettings.AspectRatio;
	return FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, ProjectionSettings.NearZ, ProjectionSettings.FarZ);
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
	return GetViewMatrix() * GetProjectionMatrix();
}

FConvexVolume UCameraComponent::GetConvexVolume() const
{
	FConvexVolume ConvexVolume;
	ConvexVolume.UpdateFromMatrix(GetViewMatrix() * GetProjectionMatrix());
	return ConvexVolume;
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = Target - Position;
	if (Diff.IsNearlyZero())
	{
		return;
	}

	Diff = Diff.Normalized();

	FRotator LookRotation;
	LookRotation.Pitch = -asinf(Diff.Z) * RAD_TO_DEG;
	LookRotation.Yaw = fabsf(Diff.Z) < 0.999f ? atan2f(Diff.Y, Diff.X) * RAD_TO_DEG : 0.0f;
	LookRotation.Roll = 0.0f;
	SetWorldRotation(LookRotation);
}

FCameraView UCameraComponent::GetCameraView() const
{
	FCameraView View;
	View.Location = GetWorldLocation();
	View.Rotation = GetWorldRotationQuat();
	View.Projection = ProjectionSettings;
	View.bValid = true;
	return View;
}

void UCameraComponent::ApplyCameraView(const FCameraView& View)
{
	if (!View.bValid)
	{
		return;
	}

	SetWorldLocation(View.Location);
	SetWorldRotation(View.Rotation);
	SetProjectionSettings(View.Projection);
}

void UCameraComponent::SetProjectionSettings(const FCameraProjectionSettings& NewSettings)
{
	ProjectionSettings = NewSettings;
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	ProjectionSettings.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight)
{
	float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	FVector NdcNear(NdcX, NdcY, 1.0f);
	FVector NdcFar(NdcX, NdcY, 0.0f);

	FMatrix ViewProj = GetViewMatrix() * GetProjectionMatrix();
	FMatrix InverseViewProjection = ViewProj.GetInverse();

	FVector WorldNear = InverseViewProjection.TransformPositionWithW(NdcNear);
	FVector WorldFar = InverseViewProjection.TransformPositionWithW(NdcFar);

	FRay Ray;
	Ray.Origin = WorldNear;

	FVector Dir = WorldFar - WorldNear;
	float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
	Ray.Direction = (Length > 1e-4f) ? Dir / Length : FVector(1, 0, 0);

	return Ray;
}

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "FOV",          EPropertyType::Float, &ProjectionSettings.FOV, 0.1f,   3.14f,    0.01f });
	OutProps.push_back({ "Aspect Ratio", EPropertyType::Float, &ProjectionSettings.AspectRatio, 0.01f,  10.0f,    0.01f });
	OutProps.push_back({ "Near Z",       EPropertyType::Float, &ProjectionSettings.NearZ, 0.01f,  100.0f,   0.01f });
	OutProps.push_back({ "Far Z",        EPropertyType::Float, &ProjectionSettings.FarZ, 1.0f,   100000.0f, 10.0f });
	OutProps.push_back({ "Orthographic", EPropertyType::Bool,  &ProjectionSettings.bIsOrthographic });
	OutProps.push_back({ "Ortho Width",  EPropertyType::Float, &ProjectionSettings.OrthoWidth, 0.1f,   1000.0f,  0.5f });
}
