#include "GameFramework/PlayerController.h"

#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "Component/ControllerInputComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerCameraManager.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
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

	if (!PlayerCameraManager)
	{
		PlayerCameraManager = UObjectManager::Get().CreateObject<APlayerCameraManager>(this);
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetOwningController(this);
			PlayerCameraManager->InitDefaultComponents();
		}
	}
}

void APlayerController::EndPlay()
{
	if (UWorld* World = GetWorld())
	{
		UCameraComponent* OutputCamera = PlayerCameraManager ? PlayerCameraManager->GetOutputCameraComponent() : nullptr;
		if (OutputCamera && World->GetActiveCamera() == OutputCamera)
		{
			World->SetActiveCamera(nullptr);
		}
		if (OutputCamera && World->GetViewCamera() == OutputCamera)
		{
			World->SetViewCamera(nullptr);
		}
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->EndPlay();
		UObjectManager::Get().DestroyObject(PlayerCameraManager);
		PlayerCameraManager = nullptr;
	}

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
	if (InActor)
	{
		UWorld* ControllerWorld = GetWorld();
		UWorld* TargetWorld = InActor->GetWorld();

		if (ControllerWorld && TargetWorld && ControllerWorld != TargetWorld)
		{
			return;
		}
	}

	if (PossessedActor == InActor)
	{
		ViewTarget = InActor;
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetViewTarget(InActor);
		}
		return;
	}

	UnPossess();

	PossessedActor = InActor;
	ViewTarget = InActor;

	if (PossessedActor)
	{
		if (APawn* Pawn = Cast<APawn>(PossessedActor))
		{
			Pawn->SetController(this);
		}

		if (UCameraComponent* Camera = FindCameraOnActor(PossessedActor))
		{
			ControlRotation = MakeControlRotationFromCamera(Camera);
		}
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(ViewTarget);
	}
}

void APlayerController::UnPossess()
{
	if (APawn* Pawn = Cast<APawn>(PossessedActor))
	{
		if (Pawn->GetController() == this)
			Pawn->SetController(nullptr);
	}
	PossessedActor = nullptr;
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
	FCameraBlendParams Params;
	Params.BlendTime = 0.0f;
	SetViewTargetWithBlend(InViewTarget, Params);
}

void APlayerController::SetViewTargetWithBlend(AActor* InViewTarget, const FCameraBlendParams& Params)
{
	ViewTarget = InViewTarget;
	if (UCameraComponent* Camera = FindCameraOnActor(InViewTarget))
	{
		ControlRotation = MakeControlRotationFromCamera(Camera);
	}
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTargetWithBlend(InViewTarget, Params);
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
	if (PlayerCameraManager)
	{
		if (UCameraComponent* OutputCamera = PlayerCameraManager->GetOutputCameraComponent())
		{
			return OutputCamera;
		}
	}
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

void APlayerController::SetCameraMode(ECameraModeId InMode, const FCameraBlendParams& Params)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetCameraMode(InMode, Params);
	}
}

ECameraModeId APlayerController::GetCameraMode() const
{
	return PlayerCameraManager ? PlayerCameraManager->GetCameraMode() : ECameraModeId::ThirdPerson;
}

void APlayerController::AddYawInput(float Value)
{
	ControlRotation.Yaw += Value;
	ControlRotation.Roll = 0.0f;
}

void APlayerController::AddPitchInput(float Value, float MinPitch, float MaxPitch)
{
	ControlRotation.Pitch = Clamp(ControlRotation.Pitch + Value, MinPitch, MaxPitch);
	ControlRotation.Roll = 0.0f;
}

bool APlayerController::ApplyControllerMovementInput(const FControllerMovementInput& Input)
{
	AActor* Actor = GetPossessedActor();
	if (!Actor)
	{
		return false;
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

	if (BestMovement && BestMovement->ApplyControllerMovementInput(Input))
	{
		return true;
	}

	Actor->AddActorWorldOffset(Input.WorldDelta);
	return true;
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
