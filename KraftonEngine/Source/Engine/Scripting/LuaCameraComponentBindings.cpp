#include "LuaBindings.h"
#include "SolInclude.h"
#include "LuaHandles.h"
#include "LuaBindingHelper.h"
#include "LuaWorldLibrary.h"

#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static constexpr float RAD2DEG = 180.0f / M_PI;
static constexpr float DEG2RAD = M_PI / 180.0f;

void RegisterCameraComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaCameraComponentHandle>(
		"CameraComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaCameraComponentHandle),

		"FOVDegrees",
		sol::property(
			[](const FLuaCameraComponentHandle& Self) -> float
			{
				UCameraComponent* C = Self.Resolve();
				return C ? C->GetFOV() * RAD2DEG : 0.0f;
			},
			[](const FLuaCameraComponentHandle& Self, float Degrees)
			{
				UCameraComponent* C = Self.Resolve();
				if (!C)
				{
					UE_LOG("[Lua] Invalid CameraComponent.FOVDegrees Access.");
					return;
				}
				C->SetFOV(Degrees * DEG2RAD);
			}
		),

		"WorldLocation",
		sol::property(
			[](const FLuaCameraComponentHandle& Self) -> FVector
			{
				UCameraComponent* C = Self.Resolve();
				return C ? C->GetWorldLocation() : FVector::ZeroVector;
			},
			[](const FLuaCameraComponentHandle& Self, const FVector& V)
			{
				UCameraComponent* C = Self.Resolve();
				if (!C)
				{
					UE_LOG("[Lua] Invalid CameraComponent.WorldLocation Access.");
					return;
				}
				C->SetWorldLocation(V);
			}
		),

		"RelativeLocation",
		sol::property(
			[](const FLuaCameraComponentHandle& Self) -> FVector
			{
				UCameraComponent* C = Self.Resolve();
				return C ? C->GetRelativeLocation() : FVector::ZeroVector;
			},
			[](const FLuaCameraComponentHandle& Self, const FVector& V)
			{
				UCameraComponent* C = Self.Resolve();
				if (!C)
				{
					UE_LOG("[Lua] Invalid CameraComponent.RelativeLocation Access.");
					return;
				}
				C->SetRelativeLocation(V);
			}
		),

		"RelativeRotation",
		sol::property(
			[](const FLuaCameraComponentHandle& Self) -> FRotator
			{
				UCameraComponent* C = Self.Resolve();
				return C ? C->GetRelativeRotation() : FRotator();
			},
			[](const FLuaCameraComponentHandle& Self, const FRotator& R)
			{
				UCameraComponent* C = Self.Resolve();
				if (!C)
				{
					UE_LOG("[Lua] Invalid CameraComponent.RelativeRotation Access.");
					return;
				}
				C->SetRelativeRotation(R);
			}
		),

		"WorldRotation",
		sol::property(
			[](const FLuaCameraComponentHandle& Self) -> FRotator
			{
				UCameraComponent* C = Self.Resolve();
				return C ? C->GetWorldMatrix().ToRotator() : FRotator();
			},
			[](const FLuaCameraComponentHandle& Self, const FRotator& R)
			{
				UCameraComponent* C = Self.Resolve();
				if (!C)
				{
					UE_LOG("[Lua] Invalid CameraComponent.WorldRotation Access.");
					return;
				}
				C->SetRelativeRotation(R);
			}
		)
	);
}
