#include "LuaBindings.h"

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUAJIT 1

#include <sol/sol.hpp>

#include "LuaHandles.h"

#include "Core/Log.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Component/ActorComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include <sol/state_view.hpp>

namespace
{
	UProjectileMovementComponent* FindProjectileMovementComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (UProjectileMovementComponent* Movement = Cast<UProjectileMovementComponent>(Component))
			{
				return Movement;
			}
		}

		return nullptr;
	}

	UProjectileMovementComponent* EnsureProjectileMovementComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UProjectileMovementComponent* Existing = FindProjectileMovementComponent(Actor))
		{
			return Existing;
		}

		return Actor->AddComponent<UProjectileMovementComponent>();
	}

	UStaticMeshComponent* FindShapeComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (UStaticMeshComponent* Shape = Cast<UStaticMeshComponent>(Component))
			{
				return Shape;
			}
		}

		return nullptr;
	}
}

// Lua에 함수를 바인딩함
void RegisterLuaBindings(sol::state& Lua)
{
	RegisterFVectorBinding(Lua);
	RegisterShapeComponentBinding(Lua);
	RegisterGameObjectBinding(Lua);
	RegisterDelegateBinding(Lua);
}

// Lua에 FVector를 등록
void RegisterFVectorBinding(sol::state& Lua)
{
	Lua.new_usertype<FVector>(
		"FVector",
		sol::constructors<FVector(), FVector(float, float, float)>(),

		"x",
		sol::property([](const FVector& V) { return V.X; }, [](FVector& V, float Value) { V.X = Value; }),

		"y",
		sol::property([](const FVector& V) { return V.Y; }, [](FVector& V, float Value) { V.Y = Value; }),

		"z",
		sol::property([](const FVector& V) { return V.Z; }, [](FVector& V, float Value) { V.Z = Value; }),

		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,

		"Length",
		[](const FVector& Self) { return Self.Length(); },

		"Normalized",
		[](const FVector& Self) { return Self.Normalized(); },

		"Dot",
		[](const FVector& Self, const FVector& Other) { return Self.Dot(Other); },

		"Cross",
		[](const FVector& Self, const FVector& Other) { return Self.Cross(Other); },

		sol::meta_function::addition,
		[](const FVector& A, const FVector& B) { return A + B; },

		sol::meta_function::subtraction,
		[](const FVector& A, const FVector& B) { return A - B; },

		sol::meta_function::multiplication,
		sol::overload([](const FVector& V, float Scalar) { return V * Scalar; }, [](float Scalar, const FVector& V) { return V * Scalar; }),

		sol::meta_function::division,
		[](const FVector& V, float Scalar) { return V / Scalar; }

	);

	Lua["VectorZero"] = FVector::ZeroVector;
	Lua["VectorOne"] = FVector::OneVector;
	Lua["VectorUp"] = FVector::UpVector;
	Lua["VectorRight"] = FVector::RightVector;
	Lua["VectorForward"] = FVector::ForwardVector;
}

// Lua에 GameObject를 등록
// Handle을 통해 UUID로만 접근 가능
void RegisterGameObjectBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaGameObjectHandle>(
		"GameObject",

		sol::no_constructor,

		"IsValid",
		[](const FLuaGameObjectHandle& Handle) { return Handle.IsValid(); },

		"IsInvalid",
		[](const FLuaGameObjectHandle& Handle) { return !Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaGameObjectHandle& Self) { return Self.UUID; }),

		"Location",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				AActor* Actor = Self.Resolve();
				return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
			},
			[](const FLuaGameObjectHandle& Self, const FVector& Location)
			{
				AActor* Actor = Self.Resolve();
				if (!Actor)
				{
					UE_LOG("[Lua] Invalid GameObject.Location Access.");
					return;
				}
				Actor->SetActorLocation(Location);
			}
		),

		"Velocity",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				AActor* Actor = Self.Resolve();
				UProjectileMovementComponent* Movement = FindProjectileMovementComponent(Actor);
				return Movement ? Movement->GetVelocity() : FVector::ZeroVector;
			},
			[](const FLuaGameObjectHandle& Self, const FVector& Velocity)
			{
				AActor* Actor = Self.Resolve();

				if (!Actor)
				{
					UE_LOG("[Lua] Invalid GameObject.Velocity Access.");
					return;
				}

				UProjectileMovementComponent* Movement = EnsureProjectileMovementComponent(Actor);

				if (!Movement)
				{
					UE_LOG("[Lua] Failed to create ProjectileMoveComponent");
					return;
				}
				Movement->SetVelocity(Velocity);
			}
		),

		"Shape",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				FLuaShapeComponentHandle Handle;

				AActor* Actor = Self.Resolve();
				UStaticMeshComponent* Shape = FindShapeComponent(Actor);

				if (Shape)
				{
					Handle.UUID = Shape->GetUUID();
				}

				return Handle;
			}
		),

		"PrintLocation",
		[](const FLuaGameObjectHandle& Self)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.PrintLocation Access.");
				return;
			}

			const FVector Location = Actor->GetActorLocation();
			UE_LOG("[Lua] GameObject UUID = %u, Location=(%.3f, %.3f, %.3f)",
				Actor->GetUUID(), Location.X, Location.Y, Location.Z
			);
		}
	);

	Lua.set_function(
		"FindGameObjectByUUID",
		[](uint32 UUID, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);

			UObject* Object = UObjectManager::Get().FindByUUID(UUID);
			AActor* Actor = Cast<AActor>(Object);

			if (!Actor)
			{
				return sol::nil;
			}

			FLuaGameObjectHandle Handle;
			Handle.UUID = Actor->GetUUID();

			return sol::make_object(LuaView, Handle);
		}
	);
}

// Lua에 StaticMesh를 등록
// Handle을 통해 UUID로만 접근 가능
void RegisterShapeComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaShapeComponentHandle>(
		"ShapeComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaShapeComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaShapeComponentHandle& Self) { return Self.UUID; }),

		"Visible",
		sol::property(
			[](const FLuaShapeComponentHandle& Self)
			{
				UStaticMeshComponent* Shape = Self.Resolve();
				return Shape ? Shape->IsVisible() : false;
			},
			[](const FLuaShapeComponentHandle& Self, bool bVisible)
			{
				UStaticMeshComponent* Shape = Self.Resolve();
				if (!Shape)
				{
					UE_LOG("[Lua] Invalid ShapeComponent.Visible Access.");
					return;
				}

				Shape->SetVisibility(bVisible);
			}
		),

		"RelativeLocation",
		sol::property(
			[](const FLuaShapeComponentHandle& Self)
			{
				UStaticMeshComponent* Shape = Self.Resolve();
				return Shape ? Shape->GetRelativeLocation() : FVector::ZeroVector;
			},
			[](const FLuaShapeComponentHandle& Self, const FVector& Location)
			{
				UStaticMeshComponent* Shape = Self.Resolve();
				if (!Shape)
				{
					UE_LOG("[Lua] Invalid ShapeComponent.RelativeLocation Access.");
					return;
				}

				Shape->SetRelativeLocation(Location);
			}
		),
		
		"WorldLocation",
		sol::property(
			[](const FLuaShapeComponentHandle& Self)
			{
				UStaticMeshComponent* Shape = Self.Resolve();
				return Shape ? Shape->GetWorldLocation() : FVector::ZeroVector;
			},
			[](const FLuaShapeComponentHandle& Self, const FVector& Location)
			{
				UStaticMeshComponent* Shape = Self.Resolve();
				if (!Shape)
				{
					UE_LOG("[Lua] Invalid ShapeComponent.WorldLocation Access.");
					return;
				}
				
				Shape->SetWorldLocation(Location);
			}
		)
	);
}

// Lua에 Delegate 함수를 등록
void RegisterDelegateBinding(sol::state& Lua)
{
	Lua.set_function(
		"RegisterOnTick",
		[](const FLuaGameObjectHandle& Object, sol::protected_function Function)
		{
			AActor* Actor = Object.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] RegisterOnTick Failed: Invalid GameObject.");
				return false;
			}

			// TODO : Delegate 함수 등록
			// 예시: 
			// FLuaDelegateManager::Get().RegisterActorTick(Actor->GetUUID(), Function);

			UE_LOG("[Lua] RegisterOnTick called for Actor UUID = %u", Actor->GetUUID());
			return true;
		}
	);
}
