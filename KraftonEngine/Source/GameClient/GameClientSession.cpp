#include "GameClient/GameClientSession.h"

#include "GameClient/GameClientEngine.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Serialization/SceneSaveManager.h"

namespace
{
	FWorldContext MakeGameWorldContext(UWorld* World, const FName& Handle)
	{
		FWorldContext Context;
		Context.WorldType = EWorldType::Game;
		Context.ContextHandle = Handle;
		Context.ContextName = "GameClient";
		Context.World = World;
		if (Context.World)
		{
			Context.World->SetWorldType(EWorldType::Game);
		}
		return Context;
	}
}

bool FGameClientSession::Initialize(UGameClientEngine* InEngine)
{
	Engine = InEngine;
	if (!Engine)
	{
		return false;
	}

	const FString& StartupScenePath = Engine->GetSettings().StartupScenePath;
	if (!StartupScenePath.empty())
	{
		FWorldContext LoadedContext;
		FPerspectiveCameraData CameraData;
		FSceneSaveManager::LoadSceneFromJSON(StartupScenePath, LoadedContext, CameraData);

		if (LoadedContext.World)
		{
			UWorld* GameWorld = LoadedContext.World->DuplicateAs(EWorldType::Game);
			LoadedContext.World->EndPlay();
			UObjectManager::Get().DestroyObject(LoadedContext.World);
			LoadedContext.World = nullptr;

			if (!GameWorld)
			{
				return false;
			}

			Engine->GetWorldList().push_back(MakeGameWorldContext(GameWorld, WorldHandle));
			Engine->SetActiveWorld(WorldHandle);
			World = GameWorld;
			return true;
		}
	}

	FWorldContext& Context = Engine->CreateWorldContext(
		EWorldType::Game,
		WorldHandle,
		"GameClient");

	Engine->SetActiveWorld(Context.ContextHandle);

	World = Context.World;
	if (!World)
	{
		return false;
	}

	World->InitWorld();
	return true;
}

bool FGameClientSession::Restart()
{
	if (!Engine)
	{
		return false;
	}

	UGameClientEngine* Owner = Engine;
	DestroyWorld();
	return Initialize(Owner);
}

void FGameClientSession::Shutdown()
{
	DestroyWorld();
	World = nullptr;
	Engine = nullptr;
}

void FGameClientSession::DestroyWorld()
{
	if (Engine && World)
	{
		Engine->DestroyWorldContext(WorldHandle);
	}
	World = nullptr;
}
