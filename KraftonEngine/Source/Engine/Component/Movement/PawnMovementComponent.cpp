#include "Component/Movement/PawnMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

IMPLEMENT_LUA_COMPONENT(UPawnMovementComponent, UMovementComponent)

UPawnMovementComponent::UPawnMovementComponent()
{
	bReceiveControllerInput = true;
	ControllerInputPriority = 0;
}

void UPawnMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ApplyPendingMovement(DeltaTime);
}

void UPawnMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
}

void UPawnMovementComponent::AddMovementInput(const FVector& WorldDirection, float Scale)
{
	if (Scale == 0.0f || WorldDirection.IsNearlyZero())
	{
		return;
	}

	const FVector Direction = WorldDirection.Normalized();
	PendingMovementInput += Direction * Scale;
	LastMovementInput = Direction;
}

FVector UPawnMovementComponent::ConsumeMovementInputVector()
{
	FVector Result = PendingMovementInput;
	PendingMovementInput = FVector::ZeroVector;
	return Result;
}

bool UPawnMovementComponent::ApplyControllerMovementInput(const FControllerMovementInput& Input)
{
	if (Input.WorldDirection.IsNearlyZero())
	{
		return false;
	}

	const float Distance = Input.MoveSpeed * Input.SpeedMultiplier * Input.DeltaTime;
	AddMovementInput(Input.WorldDirection, Distance);
	return true;
}

void UPawnMovementComponent::ApplyPendingMovement(float DeltaTime)
{
	const FVector Delta = ConsumeMovementInputVector();
	if (Delta.IsNearlyZero())
	{
		Velocity = FVector::ZeroVector;
		return;
	}
	if (!ResolveUpdatedComponent())
	{
		Velocity = FVector::ZeroVector;
		return;
	}
	if (USceneComponent* Scene = GetUpdatedComponent())
	{
		Scene->AddWorldOffset(Delta);
		Velocity = DeltaTime > 0.0f ? Delta / DeltaTime : FVector::ZeroVector;
	}
}
