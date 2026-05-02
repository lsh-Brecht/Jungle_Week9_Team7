#include "GameClient/GameClientViewport.h"

#include "GameClient/GameClientEngine.h"
#include "Component/CameraComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"

bool FGameClientViewport::Initialize(
	UGameClientEngine* InEngine,
	FWindowsWindow* Window,
	FRenderer& Renderer)
{
	Engine = InEngine;
	if (!Engine || !Window)
	{
		return false;
	}

	if (!CreateCamera())
	{
		return false;
	}

	if (!CreateViewport(Window, Renderer))
	{
		return false;
	}

	if (!CreateViewportClient(Window))
	{
		return false;
	}

	Viewport->SetClient(ViewportClient);
	ViewportClient->SetViewport(Viewport);
	ViewportClient->SetOwnerWindow(Window->GetHWND());
	ViewportClient->SetDrivingCamera(Camera);
	SetInputEnabled(true);

	if (UWorld* World = Engine->GetWorld())
	{
		World->SetActiveCamera(Camera);
	}

	return true;
}

bool FGameClientViewport::CreateCamera()
{
	UWorld* World = Engine ? Engine->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	AActor* CameraActor = World->SpawnActor<AActor>();
	if (!CameraActor)
	{
		return false;
	}

	Camera = CameraActor->AddComponent<UCameraComponent>();
	if (!Camera)
	{
		return false;
	}

	CameraActor->SetRootComponent(Camera);
	World->SetActiveCamera(Camera);

	return true;
}

bool FGameClientViewport::CreateViewport(FWindowsWindow* Window, FRenderer& Renderer)
{
	Viewport = new FViewport();
	return Viewport->Initialize(
		Renderer.GetFD3DDevice().GetDevice(),
		static_cast<uint32>(Window->GetWidth()),
		static_cast<uint32>(Window->GetHeight()));
}

bool FGameClientViewport::CreateViewportClient(FWindowsWindow* Window)
{
	ViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
	if (ViewportClient && Window)
	{
		ViewportClient->SetOwnerWindow(Window->GetHWND());
	}
	return ViewportClient != nullptr;
}

void FGameClientViewport::Tick(float DeltaTime)
{
	if (!bInputEnabled || !ViewportClient)
	{
		return;
	}

	const FInputSystemSnapshot Snapshot = InputSystem::Get().MakeSnapshot();
	ViewportClient->Tick(DeltaTime, Snapshot);
}

bool FGameClientViewport::RebuildCameraForCurrentWorld()
{
	Camera = nullptr;
	if (!CreateCamera())
	{
		return false;
	}

	if (ViewportClient)
	{
		ViewportClient->SetDrivingCamera(Camera);
		ViewportClient->SetPossessed(bInputEnabled);
	}

	return true;
}

void FGameClientViewport::ReleaseWorldBinding()
{
	if (ViewportClient)
	{
		ViewportClient->SetPossessed(false);
		ViewportClient->SetDrivingCamera(nullptr);
	}
	Camera = nullptr;
}

void FGameClientViewport::SetInputEnabled(bool bEnabled)
{
	if (bInputEnabled == bEnabled)
	{
		return;
	}

	bInputEnabled = bEnabled;
	if (ViewportClient)
	{
		ViewportClient->SetPossessed(bInputEnabled);
	}
}

void FGameClientViewport::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Viewport)
	{
		Viewport->RequestResize(Width, Height);
	}
}

void FGameClientViewport::Shutdown()
{
	if (ViewportClient)
	{
		ViewportClient->OnEndPIE();
		UObjectManager::Get().DestroyObject(ViewportClient);
		ViewportClient = nullptr;
	}

	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	Camera = nullptr;
	Engine = nullptr;
}
