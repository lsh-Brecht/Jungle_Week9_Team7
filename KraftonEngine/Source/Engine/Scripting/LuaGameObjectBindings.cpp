// LuaGameObjectBindings.cpp

#include "LuaBindings.h"
#include "SolInclude.h"

#include "LuaBindingHelper.h"
#include "LuaHandles.h"
#include "LuaWorldLibrary.h"

#include "Core/Log.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

#include "Object/Object.h"
#include "GameFramework/AActor.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ActorComponent.h"

#include "Component/Collision/BoxComponent.h"
#include "Component/Collision/CapsuleComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Component/Collision/SphereComponent.h"

#include "Component/Movement/PendulumMovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"

// LuaGameObjectBindings.cpp

#include "LuaBindings.h"
#include "SolInclude.h"

#include "LuaBindingHelper.h"
#include "LuaHandles.h"
#include "LuaWorldLibrary.h"

#include "Core/Log.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

#include "Object/Object.h"
#include "GameFramework/AActor.h"

#include "Component/StaticMeshComponent.h"
#include "Component/ActorComponent.h"

#include "Component/Collision/BoxComponent.h"
#include "Component/Collision/CapsuleComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Component/Collision/SphereComponent.h"

#include "Component/Movement/PendulumMovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"

// Lua에 GameObject를 등록
// Handle을 통해 UUID로만 접근 가능
void RegisterGameObjectBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaGameObjectHandle>(
		"GameObject",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaGameObjectHandle),

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

		"Rotation",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				AActor* Actor = Self.Resolve();
				return Actor ? Actor->GetActorRotation() : FRotator();
			},
			[](const FLuaGameObjectHandle& Self, const FRotator& Rotation)
			{
				AActor* Actor = Self.Resolve();

				if (!Actor)
				{
					UE_LOG("[Lua] Invalid GameObject.Rotation Access.");
					return;
				}

				Actor->SetActorRotation(Rotation);
			}
		),

		"Scale",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				AActor* Actor = Self.Resolve();
				return Actor ? Actor->GetActorScale() : FVector::OneVector;
			},
			[](const FLuaGameObjectHandle& Self, const FVector& Scale)
			{
				AActor* Actor = Self.Resolve();

				if (!Actor)
				{
					UE_LOG("[Lua] Invalid GameObject.Scale Access.");
					return;
				}

				Actor->SetActorScale(Scale);
			}
		),

		"Visible",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				AActor* Actor = Self.Resolve();
				return Actor ? Actor->IsVisible() : false;
			},
			[](const FLuaGameObjectHandle& Self, bool bVisible)
			{
				AActor* Actor = Self.Resolve();
				if (!Actor)
				{
					UE_LOG("[Lua] Invalid GameObject.Visible Access.");
					return;
				}
				Actor->SetVisible(bVisible);
			}
		),

		"Destroy",
		[](const FLuaGameObjectHandle& Self)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.Destroy Call.");
				return false;
			}

			return FLuaWorldLibrary::DestroyActor(Actor);
		},

		LUA_GAMEOBJECT_COMPONENT_PROPERTY(
			"Shape",
			FLuaShapeComponentHandle,
			UShapeComponent
		),

		LUA_GAMEOBJECT_COMPONENT_PROPERTY(
			"ProjectileMovement",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent
		),

		LUA_GAMEOBJECT_COMPONENT_PROPERTY(
			"InterpMovement",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent
		),

		LUA_GAMEOBJECT_COMPONENT_PROPERTY(
			"PendulumMovement",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent
		),

		LUA_GAMEOBJECT_COMPONENT_PROPERTY(
			"RotatingMovement",
			FLuaRotatingMovementComponentHandle,
			URotatingMovementComponent
		),

		LUA_GAMEOBJECT_GET_OR_ADD_COMPONENT_METHOD(
			"GetOrAddProjectileMovement",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent
		),

		LUA_GAMEOBJECT_GET_OR_ADD_COMPONENT_METHOD(
			"GetOrAddInterpMovement",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent
		),

		LUA_GAMEOBJECT_GET_OR_ADD_COMPONENT_METHOD(
			"GetOrAddPendulumMovement",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent
		),

		LUA_GAMEOBJECT_GET_OR_ADD_COMPONENT_METHOD(
			"GetOrAddRotatingMovement",
			FLuaRotatingMovementComponentHandle,
			URotatingMovementComponent
		),

		LUA_GAMEOBJECT_REMOVE_COMPONENT_METHOD(
			"RemoveProjectileMovement",
			UProjectileMovementComponent
		),

		LUA_GAMEOBJECT_REMOVE_COMPONENT_METHOD(
			"RemoveInterpMovement",
			UInterpToMovementComponent
		),

		LUA_GAMEOBJECT_REMOVE_COMPONENT_METHOD(
			"RemovePendulumMovement",
			UPendulumMovementComponent
		),

		LUA_GAMEOBJECT_REMOVE_COMPONENT_METHOD(
			"RemoveRotatingMovement",
			URotatingMovementComponent
		),

		LUA_GAMEOBJECT_SET_SHAPE_METHOD(
			"SetBoxShape",
			FLuaBoxComponentHandle,
			UBoxComponent
		),

		LUA_GAMEOBJECT_SET_SHAPE_METHOD(
			"SetSphereShape",
			FLuaSphereComponentHandle,
			USphereComponent
		),

		LUA_GAMEOBJECT_SET_SHAPE_METHOD(
			"SetCapsuleShape",
			FLuaCapsuleComponentHandle,
			UCapsuleComponent
		),

		LUA_GAMEOBJECT_COMPONENT_PROPERTY(
			"StaticMesh",
			FLuaStaticMeshComponentHandle,
			UStaticMeshComponent
		),

		"GetOrAddStaticMesh",
		[](const FLuaGameObjectHandle& Self)
		{
			FLuaStaticMeshComponentHandle Handle;

			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.GetOrAddStaticMesh Call.");
				return Handle;
			}

			UStaticMeshComponent* Mesh =
			FLuaWorldLibrary::GetOrAddStaticMeshComponent(Actor);

			if (Mesh)
			{
				Handle.UUID = Mesh->GetUUID();
			}

			return Handle;
		},

		"SetStaticMesh",
		[](const FLuaGameObjectHandle& Self, const FString& ObjPath)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.SetStaticMesh Call.");
				return false;
			}

			UStaticMeshComponent* Mesh =
			FLuaWorldLibrary::GetOrAddStaticMeshComponent(Actor);

			return FLuaWorldLibrary::SetStaticMesh(Mesh, ObjPath);
		},


		"SetMaterial",
		[](const FLuaGameObjectHandle& Self, int32 ElementIndex, const FString& MaterialPath)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.SetMaterial Call.");
				return false;
			}

			UStaticMeshComponent* Mesh =
			FLuaWorldLibrary::GetOrAddStaticMeshComponent(Actor);

			return FLuaWorldLibrary::SetMaterial(Mesh, ElementIndex, MaterialPath);
		},

		"SetMaterialColor",
		[](const FLuaGameObjectHandle& Self, int32 ElementIndex, const FString& ParamName, const FVector4& Color)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.SetMaterialColor Call.");
				return false;
			}

			UStaticMeshComponent* Mesh =
			FLuaWorldLibrary::GetOrAddStaticMeshComponent(Actor);

			return FLuaWorldLibrary::SetMaterialVector4Parameter(
				Mesh,
				ElementIndex,
				ParamName,
				Color
			);
		},

		"GetComponent",
		[](const FLuaGameObjectHandle& Self, const FString& TypeName)
		{
			FLuaActorComponentHandle Handle;

			AActor* Actor = Self.Resolve();
			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.GetComponent Call.");
				return Handle;
			}

			UActorComponent* Component = FLuaWorldLibrary::FindComponentByTypeName(Actor, TypeName);
			if (Component)
			{
				Handle.UUID = Component->GetUUID();
			}

			return Handle;
		},

		"GetOrAddComponent",
		[](const FLuaGameObjectHandle& Self, const FString& TypeName)
		{
			FLuaActorComponentHandle Handle;

			AActor* Actor = Self.Resolve();
			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.GetOrAddComponent Call.");
				return Handle;
			}

			UActorComponent* Component = FLuaWorldLibrary::GetOrAddComponentByTypeName(Actor, TypeName);
			if (Component)
			{
				Handle.UUID = Component->GetUUID();
			}

			return Handle;
		},

		"RemoveComponent",
		[](const FLuaGameObjectHandle& Self, const FString& TypeName)
		{
			AActor* Actor = Self.Resolve();
			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.RemoveComponent Call.");
				return false;
			}

			return FLuaWorldLibrary::RemoveComponentByTypeName(Actor, TypeName);
		},

		"ListComponents",
		[](const FLuaGameObjectHandle& Self, sol::this_state State)
		{
			sol::state_view LuaView(State);
			sol::table Result = LuaView.create_table();

			AActor* Actor = Self.Resolve();
			if (!Actor)
			{
				return Result;
			}

			int LuaIndex = 1;
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component)
				{
					continue;
				}

				FLuaActorComponentHandle Handle;
				Handle.UUID = Component->GetUUID();
				Result[LuaIndex++] = Handle;
			}

			return Result;
		},

		"RemoveShape",
		[](const FLuaGameObjectHandle& Self)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.RemoveShape Call.");
				return false;
			}

			return FLuaWorldLibrary::RemoveShape(Actor);
		},

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

			UE_LOG(
				"[Lua] GameObject UUID=%u, Location=(%.3f, %.3f, %.3f)",
				Actor->GetUUID(),
				Location.X,
				Location.Y,
				Location.Z
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
