#include "GameClient/GameCameraManager.h"

#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerController.h"
#include "Object/ObjectFactory.h"
#include "Object/Object.h"

void FGameCameraManager::SetWorld(UWorld* InWorld)
{
	World = InWorld;
}

void FGameCameraManager::ClearWorldBinding()
{
	World = nullptr;
	PlayerController = nullptr;
	DebugCamera = nullptr;
	StartupGameplayCamera = nullptr;
	ViewCamera = nullptr;
}

bool FGameCameraManager::CreateDebugCamera()
{
	if (!World)
	{
		return false;
	}

	AActor* CameraActor = World->SpawnActor<AActor>();
	if (!CameraActor)
	{
		return false;
	}

	DebugCamera = CameraActor->AddComponent<UCameraComponent>();
	if (!DebugCamera)
	{
		return false;
	}

	CameraActor->SetRootComponent(DebugCamera);
	return true;
}

bool FGameCameraManager::FindStartupGameplayCamera()
{
	if (!World)
	{
		return false;
	}

	UCameraComponent* Existing = World->ResolveGameplayViewCamera(PlayerController);
	if (!Existing)
	{
		return false;
	}

	StartupGameplayCamera = Existing;
	World->SetActiveCamera(Existing);
	World->SetViewCamera(Existing);
	return true;
}

bool FGameCameraManager::CreateFallbackGameplayCamera()
{
	if (!World)
	{
		return false;
	}

	AActor* CameraActor = World->SpawnActor<AActor>();
	if (!CameraActor)
	{
		return false;
	}

	UCameraComponent* Camera = CameraActor->AddComponent<UCameraComponent>();
	if (!Camera)
	{
		return false;
	}

	CameraActor->SetRootComponent(Camera);
	StartupGameplayCamera = Camera;
	World->SetActiveCamera(Camera);
	World->SetViewCamera(Camera);
	return true;
}

void FGameCameraManager::SetDebugFreeCameraEnabled(bool bEnabled)
{
	bDebugFreeCameraEnabled = bEnabled;
}

void FGameCameraManager::SetActiveCamera(UCameraComponent* Camera)
{
	if (World)
	{
		World->SetActiveCamera(Camera);
	}
}

UCameraComponent* FGameCameraManager::GetActiveCamera() const
{
	return World ? World->GetActiveCamera() : nullptr;
}

UCameraComponent* FGameCameraManager::ResolveViewCamera() const
{
	if (bDebugFreeCameraEnabled && IsAliveObject(DebugCamera))
	{
		return DebugCamera;
	}

	if (World)
	{
		if (UCameraComponent* GameplayCamera = World->ResolveGameplayViewCamera(PlayerController))
		{
			return GameplayCamera;
		}
	}

	return IsAliveObject(DebugCamera) ? DebugCamera : nullptr;
}


UCameraComponent* FGameCameraManager::GetViewCamera() const
{
	if (World)
	{
		if (UCameraComponent* Camera = World->GetViewCamera())
		{
			return Camera;
		}
	}
	return IsAliveObject(ViewCamera) ? ViewCamera : nullptr;
}

void FGameCameraManager::SyncWorldViewCamera()
{
	ViewCamera = ResolveViewCamera();

	if (World)
	{
		World->SetViewCamera(ViewCamera);
	}
}
