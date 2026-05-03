#include "Component/ControllerInputComponent.h"

#include "Component/CameraComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/PlayerController.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstring>

IMPLEMENT_LUA_COMPONENT(UControllerInputComponent, UActorComponent)

namespace
{
	constexpr int32 MovementBasisCount = 3;

	int32 NormalizeMovementBasisValue(int32 Value)
	{
		switch (Value)
		{
		case static_cast<int32>(EControllerMovementBasis::World):
		case static_cast<int32>(EControllerMovementBasis::ControlRotation):
		case static_cast<int32>(EControllerMovementBasis::ViewCamera):
			return Value;
		default:
			return static_cast<int32>(EControllerMovementBasis::ViewCamera);
		}
	}

	FVector FlattenAndNormalize(const FVector& Direction, const FVector& Fallback)
	{
		FVector Flat(Direction.X, Direction.Y, 0.0f);
		return Flat.IsNearlyZero() ? Fallback : Flat.Normalized();
	}
}

void UControllerInputComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << MovementBasis;
	Ar << MoveSpeed;
	Ar << SprintMultiplier;
	Ar << LookSensitivityX;
	Ar << LookSensitivityY;
	Ar << bInvertY;
	Ar << MinPitch;
	Ar << MaxPitch;

	if (Ar.IsLoading())
	{
		NormalizeOptions();
	}
}

void UControllerInputComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);

	static const char* MovementBasisNames[] = { "World","Control Rotation","View Camera" };

	OutProps.push_back({ "Movement Basis",EPropertyType::Enum,&MovementBasis,0.0f,0.0f,0.1f,MovementBasisNames,MovementBasisCount });
	OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed, 0.0f, 100000.0f, 0.1f });
	OutProps.push_back({ "Sprint Multiplier", EPropertyType::Float, &SprintMultiplier, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Look Sensitivity X",EPropertyType::Float,&LookSensitivityX,0.0f,100.0f,0.01f });
	OutProps.push_back({ "Look Sensitivity Y",EPropertyType::Float,&LookSensitivityY,0.0f,100.0f,0.01f });
	OutProps.push_back({ "Invert Y",EPropertyType::Bool,&bInvertY });
	OutProps.push_back({ "Min Pitch", EPropertyType::Float, &MinPitch, -180.0f, 180.0f, 0.1f });
	OutProps.push_back({ "Max Pitch", EPropertyType::Float, &MaxPitch, -180.0f, 180.0f, 0.1f });
}

void UControllerInputComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	if (std::strcmp(PropertyName, "Movement Basis") == 0)
	{
		NormalizeOptions();
		return;
	}
	if (MinPitch > MaxPitch)
	{
		const float Temp = MinPitch;
		MinPitch = MaxPitch;
		MaxPitch = Temp;
	}
	NormalizeOptions();
}

bool UControllerInputComponent::ApplyInput(APlayerController* Controller, UCameraComponent* FallbackCamera, float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	const bool bMoved = ApplyMovementInput(Controller, FallbackCamera, DeltaTime, Snapshot);
	const bool bLooked = ApplyLookInput(Controller, Snapshot);
	return bMoved || bLooked;
}

bool UControllerInputComponent::ApplyMovementInput(APlayerController* Controller, UCameraComponent* FallbackCamera, float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!Controller || Snapshot.bGuiUsingKeyboard || Snapshot.bGuiUsingTextInput)
	{
		return false;
	}

	FVector MoveInput(0.0f, 0.0f, 0.0f);
	if (Snapshot.IsDown('W')) MoveInput.X += 1.0f;
	if (Snapshot.IsDown('S')) MoveInput.X -= 1.0f;
	if (Snapshot.IsDown('A')) MoveInput.Y -= 1.0f;
	if (Snapshot.IsDown('D')) MoveInput.Y += 1.0f;
	if (Snapshot.IsDown('E') || Snapshot.IsDown(VK_SPACE)) MoveInput.Z += 1.0f;
	if (Snapshot.IsDown('Q') || Snapshot.IsDown(VK_CONTROL)) MoveInput.Z -= 1.0f;

	if (MoveInput.IsNearlyZero())
	{
		return false;
	}

	const FVector LocalInput = MoveInput.Normalized();

	FVector MoveForward = FVector::ForwardVector;
	FVector MoveRight = FVector::RightVector;
	BuildMovementBasis(Controller, FallbackCamera, MoveForward, MoveRight);

	const float SafeDeltaTime = DeltaTime > 0.0f ? DeltaTime : 1.0f / 60.0f;
	const float SpeedBoost = Snapshot.IsDown(VK_SHIFT) ? SprintMultiplier : 1.0f;

	FVector WorldDirection = MoveForward * LocalInput.X + MoveRight * LocalInput.Y + FVector::UpVector * LocalInput.Z;
	WorldDirection = !WorldDirection.IsNearlyZero() ? WorldDirection.Normalized() : FVector::ZeroVector;
	if (WorldDirection.IsNearlyZero())
	{
		return false;
	}

	FControllerMovementInput ControllerMoveInput;
	ControllerMoveInput.LocalInput = LocalInput;
	ControllerMoveInput.WorldDirection = WorldDirection;
	ControllerMoveInput.WorldDelta = WorldDirection * (MoveSpeed * SpeedBoost * SafeDeltaTime);
	ControllerMoveInput.DeltaTime = SafeDeltaTime;
	ControllerMoveInput.MoveSpeed = MoveSpeed;
	ControllerMoveInput.SpeedMultiplier = SpeedBoost;

	return Controller->ApplyControllerMovementInput(ControllerMoveInput);
}

bool UControllerInputComponent::ApplyLookInput(APlayerController* Controller, const FInputSystemSnapshot& Snapshot)
{
	if (!Controller || Snapshot.bGuiUsingMouse || (Snapshot.MouseDeltaX == 0 && Snapshot.MouseDeltaY == 0))
	{
		return false;
	}

	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw += static_cast<float>(Snapshot.MouseDeltaX) * LookSensitivityX;

	const float PitchSign = bInvertY ? -1.0f : 1.0f;
	Rotation.Pitch = Clamp(
		Rotation.Pitch + static_cast<float>(Snapshot.MouseDeltaY) * LookSensitivityY * PitchSign,
		MinPitch,
		MaxPitch);

	Rotation.Roll = 0.0f;
	Controller->SetControlRotation(Rotation);
	return true;
}

void UControllerInputComponent::SetMovementBasis(EControllerMovementBasis InBasis)
{
	MovementBasis = NormalizeMovementBasisValue(static_cast<int32>(InBasis));
}

void UControllerInputComponent::SetMoveSpeed(float InSpeed)
{
	MoveSpeed = InSpeed < 0.0f ? 0.0f : InSpeed;
}

void UControllerInputComponent::SetSprintMultiplier(float InMultiplier)
{
	SprintMultiplier = InMultiplier < 0.0f ? 0.0f : InMultiplier;
}

void UControllerInputComponent::SetLookSensitivityX(float InSensitivity)
{
	LookSensitivityX = InSensitivity < 0.0f ? 0.0f : InSensitivity;
}

void UControllerInputComponent::SetLookSensitivityY(float InSensitivity)
{
	LookSensitivityY = InSensitivity < 0.0f ? 0.0f : InSensitivity;
}

void UControllerInputComponent::SetLookSensitivity(float InSensitivity)
{
	SetLookSensitivityX(InSensitivity);
	SetLookSensitivityY(InSensitivity);
}

void UControllerInputComponent::SetMinPitch(float InMinPitch)
{
	MinPitch = InMinPitch;
	MaxPitch = (std::max)(MinPitch, MaxPitch);
}

void UControllerInputComponent::SetMaxPitch(float InMaxPitch)
{
	MaxPitch = InMaxPitch;
	MinPitch = (std::min)(MinPitch, MaxPitch);
}

UCameraComponent* UControllerInputComponent::ResolveViewCamera(APlayerController* Controller, UCameraComponent* FallbackCamera) const
{
	if (Controller)
	{
		if (UCameraComponent* Camera = Controller->ResolveViewCamera())
		{
			return Camera;
		}
	}
	return FallbackCamera;
}

void UControllerInputComponent::BuildMovementBasis(APlayerController* Controller, UCameraComponent* FallbackCamera, FVector& OutForward, FVector& OutRight) const
{
	OutForward = FVector::ForwardVector;
	OutRight = FVector::RightVector;

	switch (GetMovementBasis())
	{
	case EControllerMovementBasis::World:
		break;

	case EControllerMovementBasis::ControlRotation:
		if (Controller)
		{
			OutForward = Controller->GetControlRotation().GetForwardVector();
			OutRight = Controller->GetControlRotation().GetRightVector();
		}
		break;

	case EControllerMovementBasis::ViewCamera:
	default:
		if (UCameraComponent* Camera = ResolveViewCamera(Controller, FallbackCamera))
		{
			OutForward = Camera->GetForwardVector();
			OutRight = Camera->GetRightVector();
		}
		break;
	}

	OutForward = FlattenAndNormalize(OutForward, FVector::ForwardVector);
	OutRight = FlattenAndNormalize(OutRight, FVector::RightVector);
}

void UControllerInputComponent::NormalizeOptions()
{
	MovementBasis = NormalizeMovementBasisValue(MovementBasis);
	MoveSpeed = MoveSpeed < 0.0f ? 0.0f : MoveSpeed;
	SprintMultiplier = SprintMultiplier < 0.0f ? 0.0f : SprintMultiplier;
	LookSensitivityX = LookSensitivityX < 0.0f ? 0.0f : LookSensitivityX;
	LookSensitivityY = LookSensitivityY < 0.0f ? 0.0f : LookSensitivityY;
	if (MinPitch > MaxPitch)
	{
		const float Temp = MinPitch;
		MinPitch = MaxPitch;
		MaxPitch = Temp;
	}
}
