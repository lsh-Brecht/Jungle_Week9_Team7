#include "Component/ControllerInputComponent.h"

#include "Component/ActorComponent.h"
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

IMPLEMENT_CLASS(UControllerInputComponent, UActorComponent)

namespace
{
	constexpr int32 MovementFrameCount = 3;

	int32 NormalizeMovementFrameValue(int32 Value)
	{
		switch (Value)
		{
		case static_cast<int32>(EControllerMovementFrame::World):
		case static_cast<int32>(EControllerMovementFrame::ControlRotation):
		case static_cast<int32>(EControllerMovementFrame::ViewCamera):
			return Value;
		default:
			return static_cast<int32>(EControllerMovementFrame::ViewCamera);
		}
	}


	UMovementComponent* FindControllerDrivenMovementComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		UMovementComponent* BestMovement = nullptr;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			UMovementComponent* Movement = Cast<UMovementComponent>(Component);
			if (!Movement || !Movement->CanReceiveControllerInput())
			{
				continue;
			}

			if (!BestMovement || Movement->GetControllerInputPriority() > BestMovement->GetControllerInputPriority())
			{
				BestMovement = Movement;
			}
		}
		return BestMovement;
	}

	void RecordNoControllerMovement(APlayerController* Controller, float DeltaTime)
	{
		if (!Controller)
		{
			return;
		}

		UMovementComponent* Movement = FindControllerDrivenMovementComponent(Controller->GetPossessedActor());
		if (!Movement)
		{
			return;
		}

		FControllerMovementInput EmptyInput;
		EmptyInput.DeltaTime = (DeltaTime > 0.0f) ? DeltaTime : (1.0f / 60.0f);
		Movement->RecordControllerMovementInput(EmptyInput);
	}
}

void UControllerInputComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << MovementFrame;
	Ar << MoveSpeed;
	Ar << SprintMultiplier;
	Ar << LookSensitivity;
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

	static const char* MovementFrameNames[] = { "World", "Control Rotation", "View Camera" };

	OutProps.push_back({ "Movement Basis", EPropertyType::Enum, &MovementFrame, 0.0f, 0.0f, 0.1f, MovementFrameNames, MovementFrameCount });
	OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed, 0.0f, 100000.0f, 0.1f });
	OutProps.push_back({ "Sprint Multiplier", EPropertyType::Float, &SprintMultiplier, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Look Sensitivity", EPropertyType::Float, &LookSensitivity, 0.0f, 100.0f, 0.01f });
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
}

bool UControllerInputComponent::ApplyInput(APlayerController* Controller, UCameraComponent* FallbackCamera, float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	const bool bMoved = ApplyMovementInput(Controller, FallbackCamera, DeltaTime, Snapshot);
	const bool bLooked = ApplyLookInput(Controller, FallbackCamera, Snapshot);
	return bMoved || bLooked;
}

bool UControllerInputComponent::ApplyMovementInput(APlayerController* Controller, UCameraComponent* FallbackCamera, float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.bGuiUsingKeyboard || Snapshot.bGuiUsingTextInput)
	{
		RecordNoControllerMovement(Controller, DeltaTime);
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
		RecordNoControllerMovement(Controller, DeltaTime);
		return false;
	}
	const FVector LocalInput = MoveInput.Normalized();

	UCameraComponent* TargetCamera = ResolveTargetCamera(Controller, FallbackCamera);
	FVector MoveForward = FVector::ForwardVector;
	FVector MoveRight = FVector::RightVector;

	if (GetMovementFrame() == EControllerMovementFrame::ControlRotation && Controller)
	{
		FRotator BasisRotation = Controller->GetControlRotation();
		BasisRotation.Pitch = 0.0f;
		BasisRotation.Roll = 0.0f;
		MoveForward = BasisRotation.GetForwardVector();
		MoveRight = BasisRotation.GetRightVector();
	}
	else if (GetMovementFrame() == EControllerMovementFrame::ViewCamera && TargetCamera)
	{
		MoveForward = TargetCamera->GetForwardVector();
		MoveRight = TargetCamera->GetRightVector();
		MoveForward.Z = 0.0f;
		MoveRight.Z = 0.0f;
	}

	MoveForward = !MoveForward.IsNearlyZero() ? MoveForward.Normalized() : FVector::ForwardVector;
	MoveRight = !MoveRight.IsNearlyZero() ? MoveRight.Normalized() : FVector::RightVector;

	const float SafeDeltaTime = (DeltaTime > 0.0f) ? DeltaTime : (1.0f / 60.0f);
	const float SpeedBoost = Snapshot.IsDown(VK_SHIFT) ? SprintMultiplier : 1.0f;
	FVector WorldDirection = MoveForward * LocalInput.X + MoveRight * LocalInput.Y + FVector::UpVector * LocalInput.Z;
	WorldDirection = !WorldDirection.IsNearlyZero() ? WorldDirection.Normalized() : FVector::ZeroVector;
	return Controller
		? Controller->AddMovementInput(WorldDirection, MoveSpeed * SpeedBoost * SafeDeltaTime)
		: false;
}

bool UControllerInputComponent::ApplyLookInput(APlayerController* Controller, UCameraComponent* FallbackCamera, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.bGuiUsingMouse || (Snapshot.MouseDeltaX == 0 && Snapshot.MouseDeltaY == 0))
	{
		return false;
	}

	(void)FallbackCamera;
	if (!Controller)
	{
		return false;
	}

	Controller->AddYawInput(static_cast<float>(Snapshot.MouseDeltaX) * LookSensitivity);
	Controller->AddPitchInput(static_cast<float>(Snapshot.MouseDeltaY) * LookSensitivity);

	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Pitch = Clamp(Rotation.Pitch, MinPitch, MaxPitch);
	Rotation.Roll = 0.0f;
	Controller->SetControlRotation(Rotation);
	return true;
}

void UControllerInputComponent::SetMovementFrame(EControllerMovementFrame InFrame)
{
	MovementFrame = NormalizeMovementFrameValue(static_cast<int32>(InFrame));
}

void UControllerInputComponent::SetMoveSpeed(float InSpeed)
{
	MoveSpeed = InSpeed < 0.0f ? 0.0f : InSpeed;
}

void UControllerInputComponent::SetSprintMultiplier(float InMultiplier)
{
	SprintMultiplier = InMultiplier < 0.0f ? 0.0f : InMultiplier;
}

void UControllerInputComponent::SetLookSensitivity(float InSensitivity)
{
	LookSensitivity = InSensitivity < 0.0f ? 0.0f : InSensitivity;
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

void UControllerInputComponent::RemapActorReferences(const TMap<uint32, uint32>& ActorUUIDRemap)
{
	(void)ActorUUIDRemap;
}


UCameraComponent* UControllerInputComponent::ResolveTargetCamera(APlayerController* Controller, UCameraComponent* FallbackCamera) const
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

void UControllerInputComponent::NormalizeOptions()
{
	MovementFrame = NormalizeMovementFrameValue(MovementFrame);
}
