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

#include <cstring>

IMPLEMENT_CLASS(UControllerInputComponent, UActorComponent)

namespace
{
	constexpr int32 MovementFrameCount = 2;
	constexpr int32 LookModeCount = 3;

	int32 NormalizeMovementFrameValue(int32 Value)
	{
		return Value == static_cast<int32>(EControllerMovementFrame::World)
			? static_cast<int32>(EControllerMovementFrame::World)
			: static_cast<int32>(EControllerMovementFrame::Camera);
	}

	int32 NormalizeLookModeValue(int32 Value)
	{
		switch (Value)
		{
		case static_cast<int32>(EControllerLookMode::CameraOnly):
			return static_cast<int32>(EControllerLookMode::CameraOnly);
		case static_cast<int32>(EControllerLookMode::PawnYawPawnPitch):
			return static_cast<int32>(EControllerLookMode::PawnYawPawnPitch);
		default:
			return static_cast<int32>(EControllerLookMode::Auto);
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
}

void UControllerInputComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << MovementFrame;
	Ar << LookMode;
	Ar << MoveSpeed;
	Ar << SprintMultiplier;
	Ar << LookSensitivity;
	Ar << MinPitch;
	Ar << MaxPitch;
	Ar << PossessedActorUUID;

	if (Ar.IsLoading())
	{
		NormalizeOptions();
	}
}

void UControllerInputComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);

	static const char* MovementFrameNames[] = { "World", "Camera" };
	static const char* LookModeNames[] = { "Auto", "Camera Only", "Pawn Yaw + Pawn Pitch" };

	OutProps.push_back({ "Movement Frame", EPropertyType::Enum, &MovementFrame, 0.0f, 0.0f, 0.1f, MovementFrameNames, MovementFrameCount });
	OutProps.push_back({ "Look Mode", EPropertyType::Enum, &LookMode, 0.0f, 0.0f, 0.1f, LookModeNames, LookModeCount });
	OutProps.push_back({ "PossessedActorUUID",EPropertyType::ActorRef,&PossessedActorUUID });
	OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed, 0.0f, 100000.0f, 0.1f });
	OutProps.push_back({ "Sprint Multiplier", EPropertyType::Float, &SprintMultiplier, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Look Sensitivity", EPropertyType::Float, &LookSensitivity, 0.0f, 100.0f, 0.01f });
	OutProps.push_back({ "Min Pitch", EPropertyType::Float, &MinPitch, -180.0f, 180.0f, 0.1f });
	OutProps.push_back({ "Max Pitch", EPropertyType::Float, &MaxPitch, -180.0f, 180.0f, 0.1f });
}

void UControllerInputComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	if (std::strcmp(PropertyName, "Movement Frame") == 0 || std::strcmp(PropertyName, "Look Mode") == 0)
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

	UCameraComponent* TargetCamera = ResolveTargetCamera(Controller, FallbackCamera);
	FVector MoveForward = FVector::ForwardVector;
	FVector MoveRight = FVector::RightVector;

	if (GetMovementFrame() == EControllerMovementFrame::Camera && TargetCamera)
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
	const FVector WorldDelta = WorldDirection * (MoveSpeed * SpeedBoost * SafeDeltaTime);

	if (Controller)
	{
		if (AActor* Actor = Controller->GetPossessedActor())
		{
			FControllerMovementInput ControllerMoveInput;
			ControllerMoveInput.LocalInput = LocalInput;
			ControllerMoveInput.WorldDirection = WorldDirection;
			ControllerMoveInput.WorldDelta = WorldDelta;
			ControllerMoveInput.DeltaTime = SafeDeltaTime;
			ControllerMoveInput.MoveSpeed = MoveSpeed;
			ControllerMoveInput.SpeedMultiplier = SpeedBoost;

			if (UMovementComponent* Movement = FindControllerDrivenMovementComponent(Actor))
			{
				if (Movement->ApplyControllerMovementInput(ControllerMoveInput))
				{
					return true;
				}
			}

			Actor->AddActorWorldOffset(WorldDelta);
			return true;
		}
	}

	if (TargetCamera)
	{
		TargetCamera->SetWorldLocation(TargetCamera->GetWorldLocation() + WorldDelta);
		return true;
	}
	return false;
}

bool UControllerInputComponent::ApplyLookInput(APlayerController* Controller, UCameraComponent* FallbackCamera, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.bGuiUsingMouse || (Snapshot.MouseDeltaX == 0 && Snapshot.MouseDeltaY == 0))
	{
		return false;
	}

	UCameraComponent* TargetCamera = ResolveTargetCamera(Controller, FallbackCamera);
	if (!TargetCamera)
	{
		return false;
	}

	FRotator Rotation = Controller ? Controller->GetControlRotation() : TargetCamera->GetRelativeRotation();
	Rotation.Yaw += static_cast<float>(Snapshot.MouseDeltaX) * LookSensitivity;
	Rotation.Pitch = Clamp(Rotation.Pitch + static_cast<float>(Snapshot.MouseDeltaY) * LookSensitivity, MinPitch, MaxPitch);
	Rotation.Roll = 0.0f;

	if (Controller)
	{
		Controller->SetControlRotation(Rotation);
		AActor* PossessedActor = Controller->GetPossessedActor();
		UCameraComponent* PawnCamera = nullptr;
		if (PossessedActor)
		{
			for (UActorComponent* Comp : PossessedActor->GetComponents())
			{
				if (UCameraComponent* CC = Cast<UCameraComponent>(Comp))
				{
					PawnCamera = CC;
					break;
				}
			}
		}
		const EControllerLookMode Mode = GetLookMode();
		const bool bUsePawnYawPawnPitch = PossessedActor != nullptr
			&& (Mode == EControllerLookMode::PawnYawPawnPitch
				|| (Mode == EControllerLookMode::Auto && PawnCamera == TargetCamera));

		if (bUsePawnYawPawnPitch)
		{
			PossessedActor->SetActorRotation(FRotator(Rotation.Pitch, Rotation.Yaw, 0.0f));
			return true;
		}
	}

	TargetCamera->SetRelativeRotation(Rotation);
	return true;
}

void UControllerInputComponent::SetMovementFrame(EControllerMovementFrame InFrame)
{
	MovementFrame = NormalizeMovementFrameValue(static_cast<int32>(InFrame));
}

void UControllerInputComponent::SetLookMode(EControllerLookMode InMode)
{
	LookMode = NormalizeLookModeValue(static_cast<int32>(InMode));
}
void UControllerInputComponent::RemapActorReferences(const TMap<uint32, uint32>& ActorUUIDRemap)
{
	if (PossessedActorUUID == 0)
	{
		return;
	}

	auto It = ActorUUIDRemap.find(PossessedActorUUID);
	if (It != ActorUUIDRemap.end())
	{
		PossessedActorUUID = It->second;
	}
	else
	{
		PossessedActorUUID = 0;
	}
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
	LookMode = NormalizeLookModeValue(LookMode);
}
