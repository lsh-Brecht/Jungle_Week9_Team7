#include "GameClient/GameClientEngine.h"

#include "GameClient/GameClientRenderPipeline.h"
#include "Core/Notification.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Engine/Runtime/WindowsWindow.h"

IMPLEMENT_CLASS(UGameClientEngine, UEngine)

void UGameClientEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	Settings.Load();

	Session.Initialize(this);
	GameViewport.Initialize(this, InWindow, Renderer);
	Overlay.Initialize(InWindow, Renderer, this);

	SetGameViewportClient(GameViewport.GetViewportClient());
	SetRenderPipeline(std::make_unique<FGameClientRenderPipeline>(this, Renderer));
}

void UGameClientEngine::Shutdown()
{
	Overlay.Shutdown();
	SetGameViewportClient(nullptr);
	GameViewport.Shutdown();
	Session.Shutdown();

	UEngine::Shutdown();
}

void UGameClientEngine::Tick(float DeltaTime)
{
	TickAlways(DeltaTime);

	if (!bPauseMenuOpen)
	{
		TickInGame(DeltaTime);
	}

	Render(DeltaTime);
}

void UGameClientEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	GameViewport.OnWindowResized(Width, Height);
}

void UGameClientEngine::RenderOverlay(float DeltaTime)
{
	Overlay.Render(DeltaTime);
}

void UGameClientEngine::SetPauseMenuOpen(bool bOpen)
{
	if (bPauseMenuOpen == bOpen)
	{
		return;
	}

	bPauseMenuOpen = bOpen;
	GameViewport.SetInputEnabled(!bPauseMenuOpen);
	InputSystem::Get().ResetTransientState();
}

void UGameClientEngine::TogglePauseMenu()
{
	SetPauseMenuOpen(!bPauseMenuOpen);
}

void UGameClientEngine::RequestRestart()
{
	bRestartRequested = true;
	SetPauseMenuOpen(false);
}

void UGameClientEngine::RequestExit()
{
	if (Window && Window->GetHWND())
	{
		::PostMessage(Window->GetHWND(), WM_CLOSE, 0, 0);
		return;
	}

	::PostQuitMessage(0);
}

void UGameClientEngine::TickAlways(float DeltaTime)
{
	FDirectoryWatcher::Get().ProcessChanges();
	FNotificationManager::Get().Tick(DeltaTime);

	ProcessPendingCommands();

	InputSystem::Get().Tick();
	if (InputSystem::Get().GetKeyDown(VK_ESCAPE))
	{
		TogglePauseMenu();
	}

	Overlay.Update(DeltaTime);
	GameViewport.SetInputEnabled(!bPauseMenuOpen);
}

void UGameClientEngine::TickInGame(float DeltaTime)
{
	GameViewport.Tick(DeltaTime);
	TaskScheduler.Tick(DeltaTime);
	WorldTick(DeltaTime);
}

void UGameClientEngine::ProcessPendingCommands()
{
	if (bRestartRequested)
	{
		bRestartRequested = false;
		RestartGame();
	}
}

bool UGameClientEngine::RestartGame()
{
	GameViewport.ReleaseWorldBinding();
	if (!Session.Restart())
	{
		return false;
	}

	if (!GameViewport.RebuildCameraForCurrentWorld())
	{
		return false;
	}

	BeginPlay();
	return true;
}
