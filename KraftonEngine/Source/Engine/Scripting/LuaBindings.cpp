#include "LuaBindings.h"

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUAJIT 1

#include "LuaBindingHelper.h"
#include "SolInclude.h"

#include "LuaHandles.h"

#include "Core/Log.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Component/ActorComponent.h"
#include "Component/Collision/BoxComponent.h"
#include "Component/Collision/CapsuleComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Component/Collision/SphereComponent.h"
#include "Component/Movement/PendulumMovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"

namespace
{
	template<typename TComponent>
	TComponent* FindComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (TComponent* TypeComponent = Cast<TComponent>(Component))
			{
				return TypeComponent;
			}
		}

		return nullptr;
	}

	template<typename TComponent>
	TComponent* EnsureComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (TComponent* Existing = FindComponent<TComponent>(Actor))
		{
			return Existing;
		}

		return Actor->AddComponent<TComponent>();
	}
}

// Lua에 함수를 바인딩함
void RegisterLuaBindings(sol::state& Lua)
{
	RegisterFVectorBinding(Lua);
	RegisterFRotatorBinding(Lua);
	
	RegisterShapeComponentBinding(Lua);
	RegisterSphereComponentBinding(Lua);
	RegisterBoxComponentBinding(Lua);
	RegisterCapsuleComponentBinding(Lua);

	RegisterMovementComponentBinding(Lua);
	RegisterProjectileMovementComponentBinding(Lua);
	RegisterInterpToMovementComponentBinding(Lua);
	RegisterPendulumMovementComponentBinding(Lua);
	RegisterRotatingMovementComponentBinding(Lua);
	
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

void RegisterFRotatorBinding(sol::state& Lua)
{
	Lua.new_usertype<FRotator>(
		"FRotator",
		sol::constructors<FRotator(), FRotator(float, float, float)>(),

		"pitch",
		sol::property(
			[](const FRotator& R) { return R.Pitch; }
		),

		"yaw",
		sol::property(
			[](const FRotator& R) { return R.Yaw; }
		),

		"roll",
		sol::property(
			[](const FRotator& R) { return R.Roll; }
		),

		"Pitch", &FRotator::Pitch,
		"Yaw", &FRotator::Yaw,
		"Roll", &FRotator::Roll
	);
}

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

void RegisterMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaMovementComponentHandle>(
		"MovementComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaMovementComponentHandle),

		LUA_COMPONENT_RO_PROPERTY(
			"MovementComponent",
			"HasValidUpdatedComponent",
			FLuaMovementComponentHandle,
			UMovementComponent,
			bool,
			false,
			HasValidUpdatedComponent()
		)
	);
}

void RegisterProjectileMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaProjectileMovementComponentHandle>(
		"ProjectileMovementComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaProjectileMovementComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"ProjectileMovementComponent",
			"Velocity",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent,
			FVector,
			FVector::ZeroVector,
			GetVelocity(),
			SetVelocity(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"ProjectileMovementComponent",
			"InitialSpeed",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent,
			float,
			0.0f,
			GetInitialSpeed(),
			SetInitialSpeed(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"ProjectileMovementComponent",
			"MaxSpeed",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent,
			float,
			0.0f,
			GetMaxSpeed(),
			SetMaxSpeed(Value)
		),

		LUA_COMPONENT_RO_PROPERTY(
			"ProjectileMovementComponent",
			"PreviewVelocity",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent,
			FVector,
			FVector::ZeroVector,
			GetPreviewVelocity()
		),

		LUA_COMPONENT_METHOD(
			"ProjectileMovementComponent",
			"StopSimulating",
			FLuaProjectileMovementComponentHandle,
			UProjectileMovementComponent,
			StopSimulating()
		)
	);
}

void RegisterInterpToMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaInterpToMoveComponentHandle>(
		"InterpToMovementComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaInterpToMoveComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"InterpToMovementComponent",
			"Duration",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent,
			float,
			0.0f,
			GetInterpDuration(),
			SetInterpDuration(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"InterpToMovementComponent",
			"AutoActivate",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent,
			bool,
			false,
			IsAutoActivating(),
			ShouldAutoActivate(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"InterpToMovementComponent",
			"FaceTargetDir",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent,
			bool,
			false,
			IsFacingTargetDir(),
			ShouldFaceTargetDir(Value)
		),

		LUA_COMPONENT_METHOD(
			"InterpToMovementComponent",
			"Initiate",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent,
			Initiate()
		),

		LUA_COMPONENT_METHOD(
			"InterpToMovementComponent",
			"Reset",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent,
			Reset()
		),

		LUA_COMPONENT_METHOD(
			"InterpToMovementComponent",
			"ResetAndHalt",
			FLuaInterpToMoveComponentHandle,
			UInterpToMovementComponent,
			ResetAndHalt()
		)
	);
}

void RegisterPendulumMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaPendulumMovementComponentHandle>(
		"PendulumMovementComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaPendulumMovementComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"PendulumMovementComponent",
			"Axis",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent,
			FVector,
			FVector::UpVector,
			GetAxis(),
			SetAxis(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"PendulumMovementComponent",
			"Amplitude",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent,
			float,
			0.0f,
			GetAmplitude(),
			SetAmplitude(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"PendulumMovementComponent",
			"Frequency",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent,
			float,
			0.0f,
			GetFrequency(),
			SetFrequency(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"PendulumMovementComponent",
			"Phase",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent,
			float,
			0.0f,
			GetPhase(),
			SetPhase(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"PendulumMovementComponent",
			"AngleOffset",
			FLuaPendulumMovementComponentHandle,
			UPendulumMovementComponent,
			float,
			0.0f,
			GetAngleOffset(),
			SetAngleOffset(Value)
		)
	);
}

void RegisterRotatingMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaRotatingMovementComponentHandle>(
		"RotatingMovementComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaRotatingMovementComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"RotatingMovementComponent",
			"RotationRate",
			FLuaRotatingMovementComponentHandle,
			URotatingMovementComponent,
			FRotator,
			FRotator(),
			GetRotationRate(),
			SetRotationRate(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"RotatingMovementComponent",
			"RotationInLocalSpace",
			FLuaRotatingMovementComponentHandle,
			URotatingMovementComponent,
			bool,
			false,
			IsRotationInLocalSpace(),
			SetRotationInLocalSpace(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"RotatingMovementComponent",
			"PivotTranslation",
			FLuaRotatingMovementComponentHandle,
			URotatingMovementComponent,
			FVector,
			FVector::ZeroVector,
			GetPivotTranslation(),
			SetPivotTranslation(Value)
		),

		LUA_COMPONENT_METHOD(
			"RotatingMovementComponent",
			"ResetWorldPivotCache",
			FLuaRotatingMovementComponentHandle,
			URotatingMovementComponent,
			ResetWorldPivotCache()
		)
	);
}

// Lua에 충돌용 Shape를 등록
// Handle을 통해 UUID로만 접근 가능
void RegisterShapeComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaShapeComponentHandle>(
		"ShapeComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaShapeComponentHandle),

		LUA_COMPONENT_RO_PROPERTY_EXPR(
			"ShapeComponent",
			"ShapeType",
			FLuaShapeComponentHandle,
			UShapeComponent,
			int,
			static_cast<int>(ECollisionShapeType::None),
			static_cast<int>(Component->GetCollisionShapeType())
		),

		LUA_HANDLE_DOWNCAST_METHOD(
			"AsBox",
			FLuaShapeComponentHandle,
			UShapeComponent,
			FLuaBoxComponentHandle,
			UBoxComponent
		),

		LUA_HANDLE_DOWNCAST_METHOD(
			"AsSphere",
			FLuaShapeComponentHandle,
			UShapeComponent,
			FLuaSphereComponentHandle,
			USphereComponent
		),

		LUA_HANDLE_DOWNCAST_METHOD(
			"AsCapsule",
			FLuaShapeComponentHandle,
			UShapeComponent,
			FLuaCapsuleComponentHandle,
			UCapsuleComponent
		)
	);

	Lua["ShapeType_None"] = static_cast<int>(ECollisionShapeType::None);
	Lua["ShapeType_Box"] = static_cast<int>(ECollisionShapeType::Box);
	Lua["ShapeType_Sphere"] = static_cast<int>(ECollisionShapeType::Sphere);
	Lua["ShapeType_Capsule"] = static_cast<int>(ECollisionShapeType::Capsule);
}

void RegisterBoxComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaBoxComponentHandle>(
		"BoxComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaBoxComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"BoxComponent",
			"Extent",
			FLuaBoxComponentHandle,
			UBoxComponent,
			FVector,
			FVector::ZeroVector,
			GetBoxExtent(),
			SetBoxExtent(Value)
		)
	);
}

void RegisterSphereComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaSphereComponentHandle>(
		"SphereComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaSphereComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"SphereComponent",
			"Radius",
			FLuaSphereComponentHandle,
			USphereComponent,
			float,
			0.0f,
			GetSphereRadius(),
			SetSphereRadius(Value)
		)
	);
}

void RegisterCapsuleComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaCapsuleComponentHandle>(
		"CapsuleComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaCapsuleComponentHandle),

		LUA_COMPONENT_RW_PROPERTY(
			"CapsuleComponent",
			"Radius",
			FLuaCapsuleComponentHandle,
			UCapsuleComponent,
			float,
			0.0f,
			GetCapsuleRadius(),
			SetCapsuleRadius(Value)
		),

		LUA_COMPONENT_RW_PROPERTY(
			"CapsuleComponent",
			"HalfHeight",
			FLuaCapsuleComponentHandle,
			UCapsuleComponent,
			float,
			0.0f,
			GetCapsuleHalfHeight(),
			SetCapsuleHalfHeight(Value)
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
