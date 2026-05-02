#pragma once

#include "Core/CoreTypes.h"

class FRenderer;
class FViewport;
class FWindowsWindow;
class UCameraComponent;
class UGameClientEngine;
class UGameViewportClient;

class FGameClientViewport
{
public:
	bool Initialize(UGameClientEngine* InEngine, FWindowsWindow* Window, FRenderer& Renderer);
	void Shutdown();
	void Tick(float DeltaTime);
	void OnWindowResized(uint32 Width, uint32 Height);
	bool RebuildCameraForCurrentWorld();
	void ReleaseWorldBinding();
	void SetInputEnabled(bool bEnabled);

	FViewport* GetViewport() { return Viewport; }
	const FViewport* GetViewport() const { return Viewport; }

	UGameViewportClient* GetViewportClient() const { return ViewportClient; }
	UCameraComponent* GetCamera() const { return Camera; }

private:
	bool CreateCamera();
	bool CreateViewport(FWindowsWindow* Window, FRenderer& Renderer);
	bool CreateViewportClient(FWindowsWindow* Window);

private:
	UGameClientEngine* Engine = nullptr;
	FViewport* Viewport = nullptr;
	UGameViewportClient* ViewportClient = nullptr;
	UCameraComponent* Camera = nullptr;
	bool bInputEnabled = false;
};
