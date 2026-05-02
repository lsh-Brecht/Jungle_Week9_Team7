#include "GameClient/GameClientViewport.h"

#include "GameClient/GameClientEngine.h"
#include "Component/CameraComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
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
	SetInputEnabled(true);

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

void FGameClientViewport::BindDebugCamera(UCameraComponent* DebugCamera)
{
	if (ViewportClient)
	{
		ViewportClient->SetDrivingCamera(DebugCamera);
		ViewportClient->SetPossessed(bInputEnabled);
	}
}

void FGameClientViewport::BindPlayerController(APlayerController* PlayerController)
{
	if (ViewportClient)
	{
		ViewportClient->SetPlayerController(PlayerController);
	}
}

void FGameClientViewport::Tick(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!bInputEnabled || !ViewportClient)
	{
		return;
	}

	ViewportClient->Tick(DeltaTime, Snapshot);
}

void FGameClientViewport::ReleaseWorldBinding()
{
	if (ViewportClient)
	{
		ViewportClient->SetPossessed(false);
		ViewportClient->SetPlayerController(nullptr);
		ViewportClient->SetDrivingCamera(nullptr);
	}
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

	Engine = nullptr;
}
