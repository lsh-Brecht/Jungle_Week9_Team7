#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/ViewTypes.h"

struct FGameClientSettings
{
	FString StartupScenePath;
	int32 WindowWidth = 1920;
	int32 WindowHeight = 1080;
	bool bEnableOverlay = true;
	FViewportRenderOptions RenderOptions;

	void Load();
};
