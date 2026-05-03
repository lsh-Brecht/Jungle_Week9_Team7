#include "Component/CameraRigComponent.h"

#include "Component/CameraComponent.h"
#include "Engine/Core/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

IMPLEMENT_CLASS(UCameraRigComponent, UCameraModeComponent)

namespace
{
	constexpr int32 ProjectionModeCount = 2;

	float SignNonZero(float Value)
	{
		return Value >= 0.0f ? 1.0f : -1.0f;
	}
}

void UCameraRigComponent::BeginPlay()
{
	UCameraModeComponent::BeginPlay();
	SnapInternalState(nullptr);
}

void UCameraRigComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UCameraModeComponent::GetEditableProperties(OutProps);

	static const char* ProjectionModeNames[] = { "Orthographic","Perspective" };
	OutProps.push_back({ "Projection Mode",EPropertyType::Enum,&ProjectionMode,0.0f,0.0f,0.1f,ProjectionModeNames,ProjectionModeCount });
	OutProps.push_back({ "Target Actor",EPropertyType::ActorRef,&TargetActorUUID });
	OutProps.push_back({ "Target Offset", EPropertyType::Vec3, &TargetOffset, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Orthographic View Offset", EPropertyType::Vec3, &OrthographicViewOffset, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Orthographic Width", EPropertyType::Float, &OrthographicWidth, 10.0f, 10000.0f, 1.0f });
	OutProps.push_back({ "Perspective Back Distance", EPropertyType::Float, &PerspectiveBackDistance, 0.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Perspective Height", EPropertyType::Float, &PerspectiveHeight, 0.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Perspective Side Offset", EPropertyType::Float, &PerspectiveSideOffset, -5000.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Perspective FOV", EPropertyType::Float, &PerspectiveFOV, 0.1f, 3.14f, 0.01f });
	OutProps.push_back({ "Near Z", EPropertyType::Float, &NearZ, 0.01f, 100.0f, 0.01f });
	OutProps.push_back({ "Far Z", EPropertyType::Float, &FarZ, 1.0f, 100000.0f, 10.0f });
	OutProps.push_back({ "LookAhead Lag Speed", EPropertyType::Float, &LookAheadLagSpeed, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Enable Mouse LookAhead", EPropertyType::Bool, &bEnableMouseLookAhead });
	OutProps.push_back({ "Mouse LookAhead Distance", EPropertyType::Float, &MouseLookAheadDistance, 0.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Stabilize Vertical In Orthographic", EPropertyType::Bool, &bStabilizeVerticalInOrthographic });
	OutProps.push_back({ "Vertical Dead Zone", EPropertyType::Float, &VerticalDeadZone, 0.0f, 1000.0f, 0.01f });
	OutProps.push_back({ "Vertical Follow Strength", EPropertyType::Float, &VerticalFollowStrength, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Vertical Lag Speed", EPropertyType::Float, &VerticalLagSpeed, 0.0f, 100.0f, 0.1f });
}

void UCameraRigComponent::Serialize(FArchive& Ar)
{
	UCameraModeComponent::Serialize(Ar);
	Ar << ProjectionMode;
	Ar << TargetActorUUID;
	Ar << TargetOffset;
	Ar << OrthographicViewOffset;
	Ar << OrthographicWidth;
	Ar << PerspectiveBackDistance;
	Ar << PerspectiveHeight;
	Ar << PerspectiveSideOffset;
	Ar << PerspectiveFOV;
	Ar << NearZ;
	Ar << FarZ;
	Ar << LookAheadLagSpeed;
	Ar << bEnableMouseLookAhead;
	Ar << MouseLookAheadDistance;
	Ar << bStabilizeVerticalInOrthographic;
	Ar << VerticalDeadZone;
	Ar << VerticalFollowStrength;
	Ar << VerticalLagSpeed;

	if (Ar.IsLoading())
	{
		if (ProjectionMode < 0 || ProjectionMode >= ProjectionModeCount)
		{
			ProjectionMode = static_cast<int32>(ECameraRigProjectionMode::Perspective);
		}
		LookAheadInput = FVector2(0.0f, 0.0f);
		SmoothedLookAheadWorld = FVector::ZeroVector;
		StableFocusPoint = FVector::ZeroVector;
		StableFocusZ = 0.0f;
		bHasInitializedStableFocus = false;
	}
}

void UCameraRigComponent::RemapActorReferences(const TMap<uint32, uint32>& ActorUUIDRemap)
{
	if (TargetActorUUID == 0)
	{
		return;
	}

	auto It = ActorUUIDRemap.find(TargetActorUUID);
	TargetActorUUID = It != ActorUUIDRemap.end() ? It->second : 0;
}

ECameraModeId UCameraRigComponent::GetCameraModeId() const
{
	return IsOrthographic()
	? ECameraModeId::OrthographicFollow
	: ECameraModeId::ThirdPerson;
}

bool UCameraRigComponent::CalcCameraView(APlayerController* Controller, AActor* ViewTarget, float DeltaTime, FCameraView& OutView)
{
	AActor* Target = ResolveTargetActor(Controller, ViewTarget);
	if (!Target)
	{
		return false;
	}

	const FVector FocusPoint = ComputeFocusPoint(Controller, Target, DeltaTime);
	const FVector Location = ComputeDesiredCameraLocation(Controller, Target, FocusPoint);

	OutView.Location = Location;
	OutView.Rotation = MakeLookAtRotationQuat(Location, FocusPoint);
	OutView.Projection.bIsOrthographic = IsOrthographic();
	OutView.Projection.OrthoWidth = OrthographicWidth;
	OutView.Projection.FOV = PerspectiveFOV;
	OutView.Projection.NearZ = NearZ;
	OutView.Projection.FarZ = FarZ;
	OutView.bValid = true;
	return true;
}

void UCameraRigComponent::SetLookAheadInput(const FVector2& InInput)
{
	LookAheadInput.X = Clamp(InInput.X, -1.0f, 1.0f);
	LookAheadInput.Y = Clamp(InInput.Y, -1.0f, 1.0f);
}

void UCameraRigComponent::SetProjectionMode(ECameraRigProjectionMode InMode)
{
	const int32 NewMode = static_cast<int32>(InMode);
	if (ProjectionMode == NewMode)
	{
		return;
	}

	ProjectionMode = NewMode;
	UE_LOG("[CameraRig] ProjectionMode changed: %s", IsOrthographic() ? "Orthographic" : "Perspective");
	SnapInternalState(nullptr);
}

void UCameraRigComponent::ToggleProjectionMode()
{
	SetProjectionMode(IsOrthographic() ? ECameraRigProjectionMode::Perspective : ECameraRigProjectionMode::Orthographic);
}

void UCameraRigComponent::SnapInternalState(AActor* ViewTarget)
{
	AActor* Target = ResolveTargetActor(nullptr, ViewTarget);
	if (!Target)
	{
		return;
	}

	SmoothedLookAheadWorld = FVector::ZeroVector;
	const FVector RawFocusPoint = Target->GetActorLocation() + TargetOffset;
	StableFocusPoint = RawFocusPoint;
	StableFocusZ = RawFocusPoint.Z;
	bHasInitializedStableFocus = true;
}

AActor* UCameraRigComponent::ResolveTargetActor(APlayerController* Controller, AActor* ViewTarget) const
{
	if (UWorld* World = GetWorld())
	{
		if (AActor* Target = World->FindActorByUUIDInWorld(TargetActorUUID))
		{
			return Target;
		}
	}

	if (Controller)
	{
		if (AActor* Possessed = Controller->GetPossessedActor())
		{
			return Possessed;
		}
	}

	if (ViewTarget)
	{
		return ViewTarget;
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetPlayerController(0))
		{
			if (AActor* Possessed = PC->GetPossessedActor())
			{
				return Possessed;
			}
		}

		if (AActor* Possessable = World->FindFirstPossessableActor())
		{
			return Possessable;
		}
	}

	return GetOwner();
}

FVector UCameraRigComponent::ComputeFocusPoint(APlayerController* Controller, AActor* ViewTarget, float DeltaTime)
{
	const FVector DesiredLookAheadWorld = bEnableMouseLookAhead ? ComputeMouseLookAheadWorld(Controller) : FVector::ZeroVector;
	const float Alpha = (LookAheadLagSpeed > 0.0f)
		? Clamp(DeltaTime * LookAheadLagSpeed, 0.0f, 1.0f)
		: 1.0f;
	SmoothedLookAheadWorld = SmoothedLookAheadWorld + (DesiredLookAheadWorld - SmoothedLookAheadWorld) * Alpha;

	AActor* Target = ResolveTargetActor(Controller, ViewTarget);
	if (!Target)
	{
		return FVector::ZeroVector;
	}

	const FVector RawFocusPoint = Target->GetActorLocation() + TargetOffset + SmoothedLookAheadWorld;
	if (!bHasInitializedStableFocus)
	{
		StableFocusPoint = RawFocusPoint;
		StableFocusZ = RawFocusPoint.Z;
		bHasInitializedStableFocus = true;
	}

	if (IsOrthographic() && bStabilizeVerticalInOrthographic)
	{
		const float RawZ = RawFocusPoint.Z;
		const float DeltaZ = RawZ - StableFocusZ;
		float DesiredZ = StableFocusZ;
		const float SafeDeadZone = VerticalDeadZone > 0.0f ? VerticalDeadZone : 0.0f;

		if (std::fabs(DeltaZ) > SafeDeadZone)
		{
			const float ExcessZ = DeltaZ - SignNonZero(DeltaZ) * SafeDeadZone;
			DesiredZ = StableFocusZ + ExcessZ * Clamp(VerticalFollowStrength, 0.0f, 1.0f);
		}

		const float VerticalAlpha = (VerticalLagSpeed > 0.0f)
			? Clamp(DeltaTime * VerticalLagSpeed, 0.0f, 1.0f)
			: 1.0f;
		StableFocusZ = StableFocusZ + (DesiredZ - StableFocusZ) * VerticalAlpha;
		StableFocusPoint = FVector(RawFocusPoint.X, RawFocusPoint.Y, StableFocusZ);
		return StableFocusPoint;
	}

	StableFocusPoint = RawFocusPoint;
	StableFocusZ = RawFocusPoint.Z;
	return RawFocusPoint;
}

FVector UCameraRigComponent::ComputeDesiredCameraLocation(APlayerController* Controller, AActor* ViewTarget, const FVector& FocusPoint) const
{
	AActor* Target = ResolveTargetActor(Controller, ViewTarget);
	return FocusPoint + (IsOrthographic() ? ComputeOrthographicOffset() : ComputePerspectiveOffset(Target));
}

FVector UCameraRigComponent::ComputeOrthographicOffset() const
{
	return OrthographicViewOffset;
}

FVector UCameraRigComponent::ComputePerspectiveOffset(AActor* Target) const
{
	FVector Forward = Target ? Target->GetActorForward() : FVector(1.0f, 0.0f, 0.0f);
	Forward.Z = 0.0f;
	if (Forward.IsNearlyZero())
	{
		Forward = FVector(1.0f, 0.0f, 0.0f);
	}
	else
	{
		Forward = Forward.Normalized();
	}

	FVector Right(Forward.Y, -Forward.X, 0.0f);
	if (Right.IsNearlyZero())
	{
		Right = FVector(0.0f, 1.0f, 0.0f);
	}
	else
	{
		Right = Right.Normalized();
	}

	return (Forward * -PerspectiveBackDistance)
		+ (Right * PerspectiveSideOffset)
		+ (FVector::UpVector * PerspectiveHeight);
}

FVector UCameraRigComponent::ComputeMouseLookAheadWorld(APlayerController* Controller) const
{
	const UCameraComponent* Camera = Controller ? Controller->ResolveViewCamera() : nullptr;
	if (!Camera && GetWorld())
	{
		Camera = GetWorld()->GetViewCamera();
	}
	if (!Camera)
	{
		return FVector::ZeroVector;
	}

	FVector CameraRightFlat = Camera->GetRightVector();
	FVector CameraForwardFlat = Camera->GetForwardVector();
	CameraRightFlat.Z = 0.0f;
	CameraForwardFlat.Z = 0.0f;

	if (CameraRightFlat.IsNearlyZero())
	{
		CameraRightFlat = FVector(0.0f, 1.0f, 0.0f);
	}
	else
	{
		CameraRightFlat = CameraRightFlat.Normalized();
	}

	if (CameraForwardFlat.IsNearlyZero())
	{
		CameraForwardFlat = FVector(1.0f, 0.0f, 0.0f);
	}
	else
	{
		CameraForwardFlat = CameraForwardFlat.Normalized();
	}

	return (CameraRightFlat * LookAheadInput.X * MouseLookAheadDistance)
		+ (CameraForwardFlat * LookAheadInput.Y * MouseLookAheadDistance);
}

FQuat UCameraRigComponent::MakeLookAtRotationQuat(const FVector& Location, const FVector& Target) const
{
	FVector Diff = Target - Location;
	if (Diff.IsNearlyZero())
	{
		return FQuat::Identity;
	}

	Diff = Diff.Normalized();

	FRotator LookRotation;
	LookRotation.Pitch = -asinf(Diff.Z) * RAD_TO_DEG;
	LookRotation.Yaw = fabsf(Diff.Z) < 0.999f ? atan2f(Diff.Y, Diff.X) * RAD_TO_DEG : 0.0f;
	LookRotation.Roll = 0.0f;
	return LookRotation.ToQuaternion();
}
