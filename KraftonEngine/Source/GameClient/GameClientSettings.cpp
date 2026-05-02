#include "GameClient/GameClientSettings.h"

#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
	constexpr const char* PathsKey = "Paths";
	constexpr const char* GameClientStartLevelKey = "GameClientStartLevel";
	constexpr const char* EditorStartLevelKey = "EditorStartLevel";

	FString ReadStartLevelFromSettings()
	{
		const std::filesystem::path SettingsPath(FPaths::SettingsFilePath());
		std::ifstream File(SettingsPath);
		if (!File.is_open())
		{
			return "Default";
		}

		const FString Content(
			(std::istreambuf_iterator<char>(File)),
			std::istreambuf_iterator<char>());

		json::JSON Root = json::JSON::Load(Content);
		if (!Root.hasKey(PathsKey))
		{
			return "Default";
		}

		json::JSON Paths = Root[PathsKey];
		if (Paths.hasKey(GameClientStartLevelKey))
		{
			return Paths[GameClientStartLevelKey].ToString();
		}
		if (Paths.hasKey(EditorStartLevelKey))
		{
			return Paths[EditorStartLevelKey].ToString();
		}

		return "Default";
	}

	FString BuildScenePath(const FString& StartLevel)
	{
		if (StartLevel.empty())
		{
			return "";
		}

		std::filesystem::path ScenePath(FPaths::ToWide(StartLevel));
		if (ScenePath.has_parent_path())
		{
			if (ScenePath.is_relative())
			{
				ScenePath = std::filesystem::path(FPaths::RootDir()) / ScenePath;
			}
			if (!ScenePath.has_extension())
			{
				ScenePath += L".Scene";
			}
			return FPaths::ToUtf8(ScenePath.wstring());
		}

		if (ScenePath.has_extension())
		{
			ScenePath = std::filesystem::path(FPaths::SceneDir()) / ScenePath.filename();
		}
		else
		{
			ScenePath = std::filesystem::path(FPaths::SceneDir()) / (FPaths::ToWide(StartLevel) + L".Scene");
		}

		return FPaths::ToUtf8(ScenePath.wstring());
	}
}

void FGameClientSettings::Load()
{
	StartupScenePath = BuildScenePath(ReadStartLevelFromSettings());
	WindowWidth = 1920;
	WindowHeight = 1080;
	bEnableOverlay = true;

	RenderOptions = FViewportRenderOptions();
	RenderOptions.ViewMode = EViewMode::Lit_Phong;
	RenderOptions.ShowFlags.bPrimitives = true;
	RenderOptions.ShowFlags.bGrid = false;
	RenderOptions.ShowFlags.bWorldAxis = false;
	RenderOptions.ShowFlags.bGizmo = false;
	RenderOptions.ShowFlags.bBillboardText = false;
	RenderOptions.ShowFlags.bBoundingVolume = false;
	RenderOptions.ShowFlags.bCollisionShapes = false;
	RenderOptions.ShowFlags.bDebugDraw = true;
	RenderOptions.ShowFlags.bOctree = false;
	RenderOptions.ShowFlags.bPickingBVH = false;
	RenderOptions.ShowFlags.bCollisionBVH = false;
	RenderOptions.ShowFlags.bFog = true;
	RenderOptions.ShowFlags.bFXAA = false;
	RenderOptions.ShowFlags.bViewLightCulling = false;
	RenderOptions.ShowFlags.bVisualize25DCulling = false;
	RenderOptions.ShowFlags.bShowShadowFrustum = false;
}
