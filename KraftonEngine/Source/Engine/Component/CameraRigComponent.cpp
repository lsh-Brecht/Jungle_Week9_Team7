#include "Component/CameraRigComponent.h"

#include "Component/CameraComponent.h"
#include "Engine/Core/Log.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Serialization/Archive.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"

#include <cmath>

IMPLEMENT_CLASS(UCameraRigComponent, UActorComponent)

namespace
{
	float SignNonZero(float Value)
	{
		return Value >= 0.0f ? 1.0f : -1.0f;
	}
}

void UCameraRigComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	ResolveCameraComponent();
	ApplyProjectionMode();
	SnapToTarget();
}

void UCameraRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateCamera(DeltaTime);
}

void UCameraRigComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Target Offset", EPropertyType::Vec3, &TargetOffset, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Orthographic View Offset", EPropertyType::Vec3, &OrthographicViewOffset, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Orthographic Width", EPropertyType::Float, &OrthographicWidth, 10.0f, 10000.0f, 1.0f });
	OutProps.push_back({ "Perspective Back Distance", EPropertyType::Float, &PerspectiveBackDistance, 0.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Perspective Height", EPropertyType::Float, &PerspectiveHeight, 0.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Perspective Side Offset", EPropertyType::Float, &PerspectiveSideOffset, -5000.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Perspective FOV", EPropertyType::Float, &PerspectiveFOV, 0.1f, 3.14f, 0.01f });
	OutProps.push_back({ "Near Z", EPropertyType::Float, &NearZ, 0.01f, 100.0f, 0.01f });
	OutProps.push_back({ "Far Z", EPropertyType::Float, &FarZ, 1.0f, 100000.0f, 10.0f });
	OutProps.push_back({ "Position Lag Speed", EPropertyType::Float, &PositionLagSpeed, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "LookAhead Lag Speed", EPropertyType::Float, &LookAheadLagSpeed, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Enable Mouse LookAhead", EPropertyType::Bool, &bEnableMouseLookAhead });
	OutProps.push_back({ "Mouse LookAhead Distance", EPropertyType::Float, &MouseLookAheadDistance, 0.0f, 5000.0f, 1.0f });
	OutProps.push_back({ "Stabilize Vertical In Orthographic", EPropertyType::Bool, &bStabilizeVerticalInOrthographic });
	OutProps.push_back({ "Vertical Dead Zone", EPropertyType::Float, &VerticalDeadZone, 0.0f, 1000.0f, 0.01f });
	OutProps.push_back({ "Vertical Follow Strength", EPropertyType::Float, &VerticalFollowStrength, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Vertical Lag Speed", EPropertyType::Float, &VerticalLagSpeed, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Snap On Projection Change", EPropertyType::Bool, &bSnapOnProjectionModeChange });
}

void UCameraRigComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << ProjectionMode;
	Ar << TargetOffset;
	Ar << OrthographicViewOffset;
	Ar << OrthographicWidth;
	Ar << PerspectiveBackDistance;
	Ar << PerspectiveHeight;
	Ar << PerspectiveSideOffset;
	Ar << PerspectiveFOV;
	Ar << NearZ;
	Ar << FarZ;
	Ar << PositionLagSpeed;
	Ar << LookAheadLagSpeed;
	Ar << bEnableMouseLookAhead;
	Ar << MouseLookAheadDistance;
	Ar << bStabilizeVerticalInOrthographic;
	Ar << VerticalDeadZone;
	Ar << VerticalFollowStrength;
	Ar << VerticalLagSpeed;
	Ar << bSnapOnProjectionModeChange;

	if (Ar.IsLoading())
	{
		TargetActor = nullptr;
		CameraComponent = nullptr;
		LookAheadInput = FVector2(0.0f, 0.0f);
		SmoothedLookAheadWorld = FVector::ZeroVector;
		StableFocusPoint = FVector::ZeroVector;
		StableFocusZ = 0.0f;
		bHasInitializedStableFocus = false;
	}
}

void UCameraRigComponent::SetLookAheadInput(const FVector2& InInput)
{
	LookAheadInput.X = Clamp(InInput.X, -1.0f, 1.0f);
	LookAheadInput.Y = Clamp(InInput.Y, -1.0f, 1.0f);
}

void UCameraRigComponent::SetProjectionMode(ECameraRigProjectionMode InMode)
{
	if (ProjectionMode == InMode)
	{
		return;
	}

	ProjectionMode = InMode;
	ApplyProjectionMode();
	UE_LOG("[CameraRig] ProjectionMode changed: %s", IsOrthographic() ? "Orthographic" : "Perspective");
	if (bSnapOnProjectionModeChange)
	{
		SnapToTarget();
	}
}

void UCameraRigComponent::ToggleProjectionMode()
{
	SetProjectionMode(IsOrthographic() ? ECameraRigProjectionMode::Perspective : ECameraRigProjectionMode::Orthographic);
}

void UCameraRigComponent::ApplyProjectionMode()
{
	UCameraComponent* Camera = ResolveCameraComponent();
	if (!Camera)
	{
		return;
	}

	FCameraState State = Camera->GetCameraState();
	State.bIsOrthogonal = IsOrthographic();
	State.OrthoWidth = OrthographicWidth;
	State.FOV = PerspectiveFOV;
	State.NearZ = NearZ;
	State.FarZ = FarZ;
	Camera->SetCameraState(State);
}

void UCameraRigComponent::SnapToTarget()
{
	UCameraComponent* Camera = ResolveCameraComponent();
	AActor* Target = ResolveTargetActor();
	if (!Camera || !Target)
	{
		return;
	}

	SmoothedLookAheadWorld = bEnableMouseLookAhead ? ComputeMouseLookAheadWorld() : FVector::ZeroVector;
	const FVector RawFocusPoint = Target->GetActorLocation() + TargetOffset + SmoothedLookAheadWorld;
	StableFocusPoint = RawFocusPoint;
	StableFocusZ = RawFocusPoint.Z;
	bHasInitializedStableFocus = true;
	Camera->SetWorldLocation(ComputeDesiredCameraLocation(StableFocusPoint));
	Camera->LookAt(StableFocusPoint);
}

UCameraComponent* UCameraRigComponent::ResolveCameraComponent()
{
	if (CameraComponent)
	{
		return CameraComponent;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			CameraComponent = Camera;
			return CameraComponent;
		}
	}

	return nullptr;
}

AActor* UCameraRigComponent::ResolveTargetActor() const
{
	if (TargetActor)
	{
		return TargetActor;
	}

	UWorld* World = GetWorld();
	if (World)
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

FVector UCameraRigComponent::ComputeFocusPoint(float DeltaTime)
{
	const FVector DesiredLookAheadWorld = bEnableMouseLookAhead ? ComputeMouseLookAheadWorld() : FVector::ZeroVector;
	const float Alpha = (LookAheadLagSpeed > 0.0f)
		? Clamp(DeltaTime * LookAheadLagSpeed, 0.0f, 1.0f)
		: 1.0f;
	SmoothedLookAheadWorld = SmoothedLookAheadWorld + (DesiredLookAheadWorld - SmoothedLookAheadWorld) * Alpha;

	AActor* Target = ResolveTargetActor();
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

FVector UCameraRigComponent::ComputeDesiredCameraLocation(const FVector& FocusPoint) const
{
	return FocusPoint + (IsOrthographic() ? ComputeOrthographicOffset() : ComputePerspectiveOffset());
}

FVector UCameraRigComponent::ComputeOrthographicOffset() const
{
	return OrthographicViewOffset;
}

FVector UCameraRigComponent::ComputePerspectiveOffset() const
{
	AActor* Target = ResolveTargetActor();
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

FVector UCameraRigComponent::ComputeMouseLookAheadWorld() const
{
	const UCameraComponent* Camera = CameraComponent;
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

void UCameraRigComponent::UpdateCamera(float DeltaTime)
{
	UCameraComponent* Camera = ResolveCameraComponent();
	AActor* Target = ResolveTargetActor();
	if (!Camera || !Target)
	{
		return;
	}

	ApplyProjectionMode();

	const FVector FocusPoint = ComputeFocusPoint(DeltaTime);
	const FVector DesiredLocation = ComputeDesiredCameraLocation(FocusPoint);
	const float Alpha = (PositionLagSpeed > 0.0f)
		? Clamp(DeltaTime * PositionLagSpeed, 0.0f, 1.0f)
		: 1.0f;
	const FVector CurrentLocation = Camera->GetWorldLocation();
	Camera->SetWorldLocation(CurrentLocation + (DesiredLocation - CurrentLocation) * Alpha);

	// TODO: Add quaternion-based rotation lag if gameplay camera smoothing needs it.
	Camera->LookAt(FocusPoint);
}
