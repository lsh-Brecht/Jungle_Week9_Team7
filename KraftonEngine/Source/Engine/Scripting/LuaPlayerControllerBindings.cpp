#include "LuaBindings.h"
#include "SolInclude.h"
#include "LuaHandles.h"
#include "LuaBindingHelper.h"
#include "LuaWorldLibrary.h"

#include "Component/ControllerInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/Rotator.h"

void RegisterPlayerControllerBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaPlayerControllerHandle>(
		"PlayerController",
		
		sol::no_constructor,
		
		LUA_HANDLE_COMMON(FLuaPlayerControllerHandle),

		"Possess",
		[](const FLuaPlayerControllerHandle& Self, const FLuaGameObjectHandle& ActorHandle)
		{
			APlayerController* Controller = Self.Resolve();
			if (!Controller)
			{
				UE_LOG("[Lua] Invalid PlayerController.Possess Call.");
				return;
			}
			Controller->Possess(ActorHandle.Resolve());
		},

		"UnPossess",
		[](const FLuaPlayerControllerHandle& Self)
		{
			APlayerController* Controller = Self.Resolve();
			if (!Controller)
			{
				UE_LOG("[Lua] Invalid PlayerController.UnPossess Call.");
				return;
			}
			Controller->UnPossess();
		},

		"GetPossessedActor",
		[](const FLuaPlayerControllerHandle& Self) -> FLuaGameObjectHandle
		{
			FLuaGameObjectHandle Handle;
			APlayerController* Controller = Self.Resolve();
			AActor* Actor = Controller ? Controller->GetPossessedActor() : nullptr;
			if (Actor)
			{
				Handle.UUID = Actor->GetUUID();
			}
			return Handle;
		},

		"GetControllerInput",
		[](const FLuaPlayerControllerHandle& Self) -> FLuaActorComponentHandle
		{
			FLuaActorComponentHandle Handle;
			APlayerController* Controller = Self.Resolve();
			if (Controller)
			{
				if (UControllerInputComponent* Input = Controller->FindControllerInputComponent())
				{
					Handle.UUID = Input->GetUUID();
				}
			}
			return Handle;
		},

		"SetViewTarget",
		sol::overload(
			[](const FLuaPlayerControllerHandle& Self, const FLuaPawnHandle& PawnHandle)
			{
				APlayerController* Controller = Self.Resolve();
				if (!Controller)
				{
					UE_LOG("[Lua] Invalid PlayerController.SetViewTarget(Pawn) Call.");
					return;
				}
				Controller->SetViewTarget(PawnHandle.Resolve());
			},
			[](const FLuaPlayerControllerHandle& Self, const FLuaGameObjectHandle& ActorHandle)
			{
				APlayerController* Controller = Self.Resolve();
				if (!Controller)
				{
					UE_LOG("[Lua] Invalid PlayerController.SetViewTarget(Actor) Call.");
					return;
				}
				Controller->SetViewTarget(ActorHandle.Resolve());
			}
		),

		"GetControlRotation",
		[](const FLuaPlayerControllerHandle& Self) -> FRotator
		{
			APlayerController* Controller = Self.Resolve();
			return Controller ? Controller->GetControlRotation() : FRotator();
		},

		"SetControlRotation",
		[](const FLuaPlayerControllerHandle& Self, const FRotator& Rotation)
		{
			APlayerController* Controller = Self.Resolve();
			if (!Controller)
			{
				UE_LOG("[Lua] Invalid PlayerController.SetControlRotation Call.");
				return;
			}
			Controller->SetControlRotation(Rotation);
		}
	);
}
