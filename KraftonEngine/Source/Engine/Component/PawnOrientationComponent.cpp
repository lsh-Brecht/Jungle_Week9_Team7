#include "Component/PawnOrientationComponent.h"

#include "Component/Movement/MovementComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>

IMPLEMENT_CLASS(UPawnOrientationComponent, UActorComponent)

namespace
{
	constexpr int32 FacingModeCount = 6;

	int32 NormalizeFacingModeValue(int32 Value)
	{
		switch (Value)
		{
		case static_cast<int32>(EPawnFacingMode::None):
		case static_cast<int32>(EPawnFacingMode::ControlRotationYaw):
		case static_cast<int32>(EPawnFacingMode::MovementInputDirection):
		case static_cast<int32>(EPawnFacingMode::MovementVelocityDirection):
		case static_cast<int32>(EPawnFacingMode::MovementDirectionWithControlFallback):
		case static_cast<int32>(EPawnFacingMode::CustomWorldDirection):
			return Value;
		default:
			return static_cast<int32>(EPawnFacingMode::MovementDirectionWithControlFallback);
		}
	}

	float NormalizeAngle180(float Angle)
	{
		Angle = std::fmod(Angle + 180.0f, 360.0f);
		if (Angle < 0.0f)
		{
			Angle += 360.0f;
		}
		return Angle - 180.0f;
	}

	float MoveAngleTowards(float Current, float Target, float MaxDelta)
	{
		const float Delta = NormalizeAngle180(Target - Current);
		if (std::fabs(Delta) <= MaxDelta)
		{
			return Target;
		}
		return Current + (Delta > 0.0f ? MaxDelta : -MaxDelta);
	}

	float YawFromDirection(const FVector& Direction)
	{
		return std::atan2(Direction.Y, Direction.X) * RAD_TO_DEG;
	}

	FVector FlattenDirection(const FVector& Direction)
	{
		FVector Flat(Direction.X, Direction.Y, 0.0f);
		return Flat.IsNearlyZero() ? FVector::ZeroVector : Flat.Normalized();
	}
}

void UPawnOrientationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	float DesiredYaw = 0.0f;
	bDebugHasValidSource = ComputeDesiredFacingYaw(DesiredYaw);
	DebugDesiredYaw = DesiredYaw;

	if (bDebugHasValidSource)
	{
		ApplyFacingYaw(DesiredYaw, DeltaTime);
	}
}

void UPawnOrientationComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << FacingMode;
	Ar << RotationSpeed;
	Ar << bYawOnly;
	Ar << MinInputSize;
	Ar << MinVelocitySize;
	Ar << CustomFacingDirection;

	if (Ar.IsLoading())
	{
		NormalizeOptions();
		DebugDesiredYaw = 0.0f;
		DebugCurrentYaw = 0.0f;
		DebugSourceDirection = FVector::ZeroVector;
		bDebugHasValidSource = false;
	}
}

void UPawnOrientationComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);

	static const char* FacingModeNames[] =
	{
		"None",
		"Control Rotation Yaw",
		"Movement Input Direction",
		"Movement Velocity Direction",
		"Movement Direction + Control Fallback",
		"Custom World Direction"
	};

	OutProps.push_back({ "Facing Mode", EPropertyType::Enum, &FacingMode, 0.0f, 0.0f, 0.1f, FacingModeNames, FacingModeCount });
	OutProps.push_back({ "Rotation Speed", EPropertyType::Float, &RotationSpeed, 0.0f, 10000.0f, 1.0f });
	OutProps.push_back({ "Yaw Only", EPropertyType::Bool, &bYawOnly });
	OutProps.push_back({ "Min Input Size", EPropertyType::Float, &MinInputSize, 0.0f, 1000.0f, 0.01f });
	OutProps.push_back({ "Min Velocity Size", EPropertyType::Float, &MinVelocitySize, 0.0f, 100000.0f, 0.1f });
	OutProps.push_back({ "Custom Facing Direction", EPropertyType::Vec3, &CustomFacingDirection, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Debug Desired Yaw", EPropertyType::Float, &DebugDesiredYaw, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Debug Current Yaw", EPropertyType::Float, &DebugCurrentYaw, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Debug Source Direction", EPropertyType::Vec3, &DebugSourceDirection, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Debug Has Valid Source", EPropertyType::Bool, &bDebugHasValidSource });
}

void UPawnOrientationComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	if (std::strcmp(PropertyName, "Facing Mode") == 0)
	{
		NormalizeOptions();
	}
	RotationSpeed = RotationSpeed < 0.0f ? 0.0f : RotationSpeed;
	MinInputSize = MinInputSize < 0.0f ? 0.0f : MinInputSize;
	MinVelocitySize = MinVelocitySize < 0.0f ? 0.0f : MinVelocitySize;
}

void UPawnOrientationComponent::SetFacingMode(EPawnFacingMode InMode)
{
	FacingMode = NormalizeFacingModeValue(static_cast<int32>(InMode));
}

void UPawnOrientationComponent::SetRotationSpeed(float InSpeed)
{
	RotationSpeed = InSpeed < 0.0f ? 0.0f : InSpeed;
}

void UPawnOrientationComponent::SetCustomFacingDirection(const FVector& InDirection)
{
	CustomFacingDirection = InDirection;
}

APlayerController* UPawnOrientationComponent::ResolveController() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (APawn* Pawn = Cast<APawn>(OwnerActor))
	{
		return Pawn->GetController();
	}

	if (UWorld* World = GetWorld())
	{
		for (APlayerController* Controller : World->GetPlayerControllers())
		{
			if (Controller && Controller->GetPossessedActor() == OwnerActor)
			{
				return Controller;
			}
		}
	}

	return nullptr;
}

UMovementComponent* UPawnOrientationComponent::ResolveMovementComponent() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	UMovementComponent* BestMovement = nullptr;
	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		UMovementComponent* Movement = Cast<UMovementComponent>(Component);
		if (!Movement)
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

bool UPawnOrientationComponent::ComputeDesiredFacingYaw(float& OutYaw) const
{
	const EPawnFacingMode Mode = GetFacingMode();
	if (Mode == EPawnFacingMode::None)
	{
		return false;
	}

	auto UseControlRotationYaw = [&]() -> bool
	{
		APlayerController* Controller = ResolveController();
		if (!Controller)
		{
			return false;
		}
		OutYaw = Controller->GetControlRotation().Yaw;
		const_cast<UPawnOrientationComponent*>(this)->DebugSourceDirection = Controller->GetControlRotation().GetForwardVector();
		return true;
	};

	auto UseMovementInputYaw = [&]() -> bool
	{
		UMovementComponent* Movement = ResolveMovementComponent();
		if (!Movement)
		{
			return false;
		}
		const FVector Direction = FlattenDirection(Movement->GetLastMovementInput());
		if (Direction.Length() < MinInputSize)
		{
			return false;
		}
		OutYaw = YawFromDirection(Direction);
		const_cast<UPawnOrientationComponent*>(this)->DebugSourceDirection = Direction;
		return true;
	};

	auto UseVelocityYaw = [&]() -> bool
	{
		UMovementComponent* Movement = ResolveMovementComponent();
		if (!Movement)
		{
			return false;
		}
		const FVector Direction = FlattenDirection(Movement->GetVelocity());
		if (Movement->GetVelocity().Length() < MinVelocitySize || Direction.IsNearlyZero())
		{
			return false;
		}
		OutYaw = YawFromDirection(Direction);
		const_cast<UPawnOrientationComponent*>(this)->DebugSourceDirection = Direction;
		return true;
	};

	switch (Mode)
	{
	case EPawnFacingMode::ControlRotationYaw:
		return UseControlRotationYaw();
	case EPawnFacingMode::MovementInputDirection:
		return UseMovementInputYaw();
	case EPawnFacingMode::MovementVelocityDirection:
		return UseVelocityYaw();
	case EPawnFacingMode::MovementDirectionWithControlFallback:
		return UseMovementInputYaw() || UseVelocityYaw() || UseControlRotationYaw();
	case EPawnFacingMode::CustomWorldDirection:
	{
		const FVector Direction = FlattenDirection(CustomFacingDirection);
		if (Direction.IsNearlyZero())
		{
			return false;
		}
		OutYaw = YawFromDirection(Direction);
		const_cast<UPawnOrientationComponent*>(this)->DebugSourceDirection = Direction;
		return true;
	}
	default:
		return false;
	}
}

void UPawnOrientationComponent::ApplyFacingYaw(float TargetYaw, float DeltaTime)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	FRotator Rotation = OwnerActor->GetActorRotation();
	DebugCurrentYaw = Rotation.Yaw;

	if (RotationSpeed <= 0.0f || DeltaTime <= 0.0f)
	{
		Rotation.Yaw = TargetYaw;
	}
	else
	{
		Rotation.Yaw = MoveAngleTowards(Rotation.Yaw, TargetYaw, RotationSpeed * DeltaTime);
	}

	if (bYawOnly)
	{
		Rotation.Pitch = 0.0f;
		Rotation.Roll = 0.0f;
	}

	OwnerActor->SetActorRotation(Rotation);
}

void UPawnOrientationComponent::NormalizeOptions()
{
	FacingMode = NormalizeFacingModeValue(FacingMode);
	RotationSpeed = RotationSpeed < 0.0f ? 0.0f : RotationSpeed;
	MinInputSize = MinInputSize < 0.0f ? 0.0f : MinInputSize;
	MinVelocitySize = MinVelocitySize < 0.0f ? 0.0f : MinVelocitySize;
}
