#include "GameFramework/PlayerController.h"
#include "Component/CameraComponent.h"
#include "Component/ActorComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/Pawn.h"

IMPLEMENT_CLASS(APlayerController, AActor)

void APlayerController::EndPlay()
{
	UnPossess();
	ViewTarget = nullptr;
	AActor::EndPlay();
}

void APlayerController::Possess(APawn* InPawn)
{
	if (Pawn == InPawn)
	{
		// 같은 Pawn을 다시 Possess해도 ViewTarget은 Pawn으로 맞춘다.
		ViewTarget = InPawn;
		return;
	}

	UnPossess();

	Pawn = InPawn;
	ViewTarget = InPawn;

	if (Pawn)
	{
		Pawn->SetController(this);
	}
}

void APlayerController::UnPossess()
{
	APawn* CurrentPawn = GetPawn();
	if (CurrentPawn && CurrentPawn->GetController() == this)
	{
		CurrentPawn->SetController(nullptr);
	}
	Pawn = nullptr;
}

APawn* APlayerController::GetPawn() const
{
	if (!Pawn || !IsAliveObject(Pawn))
	{
		return nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		return World->IsActorInWorld(Pawn) ? Pawn : nullptr;
	}
	return Pawn;
}

void APlayerController::SetViewTarget(AActor* InViewTarget)
{
	ViewTarget = InViewTarget;
}

AActor* APlayerController::GetViewTarget() const
{
	if (!ViewTarget || !IsAliveObject(ViewTarget))
	{
		return nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		return World->IsActorInWorld(ViewTarget) ? ViewTarget : nullptr;
	}
	return ViewTarget;
}

static UCameraComponent* FindCameraOnActor(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}
	for (UActorComponent* Component : Target->GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			return Camera;
		}
	}
	return nullptr;
}

UCameraComponent* APlayerController::ResolveViewCamera() const
{
	// ViewTarget에 카메라가 있으면 먼저 사용한다.
	if (UCameraComponent* Camera = FindCameraOnActor(GetViewTarget()))
	{
		return Camera;
	}

	// ViewTarget이 카메라 없는 Actor여도, Possess 중인 Pawn 카메라는 fallback으로 쓴다.
	if (APawn* CurrentPawn = GetPawn())
	{
		if (UCameraComponent* Camera = CurrentPawn->FindPawnCamera())
		{
			return Camera;
		}
	}

	return nullptr;
}
