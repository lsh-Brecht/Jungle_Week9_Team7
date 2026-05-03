#include "GameFramework/PlayerController.h"

#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "Component/ControllerInputComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(APlayerController, AActor)

static UCameraComponent* FindCameraOnActor(AActor* Target);

void APlayerController::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
	Ar << ControlRotation;
}

void APlayerController::InitDefaultComponents()
{
	if (!GetRootComponent())
	{
		if (UStaticMeshComponent* Root = AddComponent<UStaticMeshComponent>())
		{
			SetRootComponent(Root);
		}
	}
	if (!FindControllerInputComponent())
	{
		AddComponent<UControllerInputComponent>();
	}
}

void APlayerController::EndPlay()
{
	UnPossess();
	ViewTarget = nullptr;
	AActor::EndPlay();
}

static FRotator MakeControlRotationFromCamera(const UCameraComponent* Camera)
{
	FRotator Rotation = Camera ? Camera->GetWorldMatrix().ToRotator() : FRotator();
	Rotation.Roll = 0.0f;
	return Rotation;
}

void APlayerController::Possess(AActor* InActor)
{
	if (PossessedActor == InActor)
	{
		ViewTarget = InActor;
		return;
	}
	UnPossess();
	PossessedActor = InActor;
	ViewTarget = InActor;
	if (PossessedActor)
	{
		if (APawn* Pawn = Cast<APawn>(PossessedActor))
			Pawn->SetController(this);
		if (UCameraComponent* Camera = FindCameraOnActor(PossessedActor))
			ControlRotation = MakeControlRotationFromCamera(Camera);
	}
	if (UControllerInputComponent* Input = FindControllerInputComponent())
		Input->PossessedActorUUID = InActor ? InActor->GetUUID() : 0;
}

void APlayerController::UnPossess()
{
	if (APawn* Pawn = Cast<APawn>(PossessedActor))
	{
		if (Pawn->GetController() == this)
			Pawn->SetController(nullptr);
	}
	PossessedActor = nullptr;
	if (UControllerInputComponent* Input = FindControllerInputComponent())
		Input->PossessedActorUUID = 0;
}

AActor* APlayerController::GetPossessedActor() const
{
	if (!PossessedActor || !IsAliveObject(PossessedActor))
	{
		return nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		return World->IsActorInWorld(PossessedActor) ? PossessedActor : nullptr;
	}
	return PossessedActor;
}

void APlayerController::SetViewTarget(AActor* InViewTarget)
{
	ViewTarget = InViewTarget;
	if (UCameraComponent* Camera = FindCameraOnActor(InViewTarget))
	{
		ControlRotation = MakeControlRotationFromCamera(Camera);
	}
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
	if (UCameraComponent* Camera = FindCameraOnActor(GetViewTarget()))
	{
		return Camera;
	}
	if (UCameraComponent* Camera = FindCameraOnActor(GetPossessedActor()))
	{
		return Camera;
	}
	return nullptr;
}

UControllerInputComponent* APlayerController::FindControllerInputComponent() const
{
	for (UActorComponent* Component : GetComponents())
	{
		if (UControllerInputComponent* Input = Cast<UControllerInputComponent>(Component))
		{
			return Input;
		}
	}
	return nullptr;
}
