#include "LuaBindings.h"
#include "SolInclude.h"
#include "LuaHandles.h"
#include "LuaBindingHelper.h"
#include "LuaWorldLibrary.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Component/CameraComponent.h"
#include "Math/Rotator.h"

void RegisterPlayerControllerBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaPlayerControllerHandle>(
		"PlayerController",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaPlayerControllerHandle),

		"Possess",
		[](const FLuaPlayerControllerHandle& Self, const FLuaPawnHandle& PawnHandle)
		{
			APlayerController* Controller = Self.Resolve();
			if (!Controller)
			{
				UE_LOG("[Lua] Invalid PlayerController.Possess Call.");
				return;
			}
			APawn* Pawn = PawnHandle.Resolve();
			Controller->Possess(Pawn);
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

		"GetPawn",
		[](const FLuaPlayerControllerHandle& Self) -> FLuaPawnHandle
		{
			FLuaPawnHandle Handle;
			APlayerController* Controller = Self.Resolve();
			if (!Controller) return Handle;
			APawn* Pawn = Controller->GetPawn();
			if (Pawn)
			{
				Handle.UUID = Pawn->GetUUID();
			}
			return Handle;
		},

		// SetViewTarget은 Pawn 핸들과 GameObject 핸들 모두 수용
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
				APawn* Pawn = PawnHandle.Resolve();
				Controller->SetViewTarget(Pawn);
			},
			[](const FLuaPlayerControllerHandle& Self, const FLuaGameObjectHandle& ActorHandle)
			{
				APlayerController* Controller = Self.Resolve();
				if (!Controller)
				{
					UE_LOG("[Lua] Invalid PlayerController.SetViewTarget(Actor) Call.");
					return;
				}
				AActor* Target = ActorHandle.Resolve();
				Controller->SetViewTarget(Target);
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
