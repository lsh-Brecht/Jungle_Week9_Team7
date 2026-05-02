#include "GameFramework/Pawn.h"
#include "Component/CameraComponent.h"
#include "Component/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include <cmath>

IMPLEMENT_CLASS(APawn, AActor)

void APawn::EndPlay()
{
	if (APlayerController* CurrentController = GetController())
	{
		if (CurrentController->GetPawn() == this)
		{
			CurrentController->UnPossess();
		}
	}
	Controller = nullptr;
	AActor::EndPlay();
}


APlayerController* APawn::GetController() const
{
	if (!Controller || !IsAliveObject(Controller))
	{
		return nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		return World->IsActorInWorld(Controller) ? Controller : nullptr;
	}
	return Controller;
}

void APawn::AddMovementInput(const FVector& Direction, float Scale)
{
	if (Scale == 0.0f)
	{
		return;
	}

	const float LenSq = Direction.X * Direction.X + Direction.Y * Direction.Y + Direction.Z * Direction.Z;
	if (LenSq < 1e-8f)
	{
		return;
	}

	const float InvLen = 1.0f / sqrtf(LenSq);
	PendingMovementInput.X += Direction.X * InvLen * Scale;
	PendingMovementInput.Y += Direction.Y * InvLen * Scale;
	PendingMovementInput.Z += Direction.Z * InvLen * Scale;
}

FVector APawn::ConsumeMovementInputVector()
{
	FVector Result = PendingMovementInput;
	PendingMovementInput = FVector::ZeroVector;
	return Result;
}

UCameraComponent* APawn::FindPawnCamera() const
{
	for (UActorComponent* Component : GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			return Camera;
		}
	}
	return nullptr;
}
