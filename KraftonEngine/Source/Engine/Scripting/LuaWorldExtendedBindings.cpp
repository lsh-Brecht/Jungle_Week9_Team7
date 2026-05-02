#include "LuaBindings.h"
#include "SolInclude.h"
#include "LuaHandles.h"
#include "LuaWorldLibrary.h"

#include "Core/Log.h"
#include "GameFramework/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Object/ObjectFactory.h"
#include "Runtime/Engine.h"

void RegisterWorldExtendedBinding(sol::state& Lua)
{
	// 기존 World 테이블이 있으면 가져오고, 없으면 생성
	sol::table World = Lua.get_or("World", Lua.create_table());
	Lua["World"] = World;

	// World.GetPlayerController(index)
	World.set_function("GetPlayerController",
		[](int32 Index, sol::this_state TS) -> sol::object
		{
			sol::state_view LuaView(TS);

			UWorld* W = FLuaWorldLibrary::GetActiveWorld();
			if (!W)
			{
				UE_LOG("[Lua] World.GetPlayerController: No active world.");
				return sol::nil;
			}

			APlayerController* Controller = W->GetPlayerController(Index);
			if (!Controller)
			{
				return sol::nil;
			}

			FLuaPlayerControllerHandle Handle;
			Handle.UUID = Controller->GetUUID();
			return sol::make_object(LuaView, Handle);
		});

	// World.SpawnPawn([location])
	World.set_function("SpawnPawn",
		[](sol::optional<FVector> MaybeLocation, sol::this_state TS) -> sol::object
		{
			sol::state_view LuaView(TS);

			UWorld* W = FLuaWorldLibrary::GetActiveWorld();
			if (!W)
			{
				UE_LOG("[Lua] World.SpawnPawn: No active world.");
				return sol::nil;
			}

			APawn* Pawn = W->SpawnActor<APawn>();
			if (!Pawn)
			{
				UE_LOG("[Lua] World.SpawnPawn: SpawnActor failed.");
				return sol::nil;
			}

			// Pawn에 RootComponent가 없으면 SceneComponent 추가
			if (!Pawn->GetRootComponent())
			{
				USceneComponent* Root = Pawn->AddComponent<USceneComponent>();
				if (Root)
				{
					Pawn->SetRootComponent(Root);
				}
			}

			if (MaybeLocation.has_value())
			{
				Pawn->SetActorLocation(MaybeLocation.value());
			}

			FLuaPawnHandle Handle;
			Handle.UUID = Pawn->GetUUID();
			return sol::make_object(LuaView, Handle);
		});

	// World.GetActiveCamera()
	World.set_function("GetActiveCamera",
		[](sol::this_state TS) -> sol::object
		{
			sol::state_view LuaView(TS);

			UWorld* W = FLuaWorldLibrary::GetActiveWorld();
			if (!W) return sol::nil;

			UCameraComponent* Camera = W->GetActiveCamera();
			if (!Camera) return sol::nil;

			FLuaCameraComponentHandle Handle;
			Handle.UUID = Camera->GetUUID();
			return sol::make_object(LuaView, Handle);
		});

	// World.SetActiveCamera(cameraHandle)
	World.set_function("SetActiveCamera",
		[](const FLuaCameraComponentHandle& CameraHandle)
		{
			UWorld* W = FLuaWorldLibrary::GetActiveWorld();
			if (!W)
			{
				UE_LOG("[Lua] World.SetActiveCamera: No active world.");
				return;
			}
			UCameraComponent* Camera = CameraHandle.Resolve();
			W->SetActiveCamera(Camera);
		});

	// World.FindActorByName(name)
	World.set_function("FindActorByName",
		[](const FString& Name, sol::this_state TS) -> sol::object
		{
			sol::state_view LuaView(TS);

			UWorld* W = FLuaWorldLibrary::GetActiveWorld();
			if (!W) return sol::nil;

			for (AActor* Actor : W->GetActors())
			{
				if (!Actor) continue;
				if (Actor->GetFName().ToString() == Name)
				{
					FLuaGameObjectHandle Handle;
					Handle.UUID = Actor->GetUUID();
					return sol::make_object(LuaView, Handle);
				}
			}
			return sol::nil;
		});

	// World.FindActorsByTag(tag) — 이름 기반으로 다수 액터 반환
	World.set_function("FindActorsByTag",
		[](const FString& Tag, sol::this_state TS) -> sol::table
		{
			sol::state_view LuaView(TS);
			sol::table Result = LuaView.create_table();

			UWorld* W = FLuaWorldLibrary::GetActiveWorld();
			if (!W) return Result;

			int LuaIndex = 1;
			for (AActor* Actor : W->GetActors())
			{
				if (!Actor) continue;
				if (Actor->GetFName().ToString() == Tag)
				{
					FLuaGameObjectHandle Handle;
					Handle.UUID = Actor->GetUUID();
					Result[LuaIndex++] = Handle;
				}
			}
			return Result;
		});

	// Game 테이블
	sol::table Game = Lua.get_or("Game", Lua.create_table());
	Lua["Game"] = Game;

	Game.set_function("Restart", []()
	{
		// UGameClientEngine::RequestRestart와 연결은 별도 바인딩 계층에서 처리
		UE_LOG("[Lua] Game.Restart() called — connect to engine restart from GameClient layer.");
	});
}
