#pragma once

#include "Component/StaticMeshComponent.h"
#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Collision/BoxComponent.h"
#include "Component/Collision/CapsuleComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Component/Collision/SphereComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Movement/PendulumMovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Core/CoreTypes.h"
#include "Object/Object.h"
#include "GameFramework/AActor.h"

// Lua가 Object에 직접 접근할 수 없도록 감쌈
// nullptr일 경우를 대비
struct FLuaGameObjectHandle
{
	uint32 UUID = 0;

	AActor* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<AActor>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaActorComponentHandle
{
	uint32 UUID = 0;

	UActorComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UActorComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaSceneComponentHandle
{
	uint32 UUID = 0;

	USceneComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<USceneComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaPrimitiveComponentHandle
{
	uint32 UUID = 0;

	UPrimitiveComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UPrimitiveComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaMovementComponentHandle
{
	uint32 UUID = 0;

	UMovementComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UMovementComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaProjectileMovementComponentHandle
{
	uint32 UUID = 0;

	UProjectileMovementComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UProjectileMovementComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaInterpToMoveComponentHandle
{
	uint32 UUID = 0;

	UInterpToMovementComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UInterpToMovementComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaPendulumMovementComponentHandle
{
	uint32 UUID = 0;

	UPendulumMovementComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UPendulumMovementComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaRotatingMovementComponentHandle
{
	uint32 UUID = 0;

	URotatingMovementComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<URotatingMovementComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

// Lua가 Shape에 직접 접근할 수 없도록 감쌈
// nullptr일 경우를 대비
struct FLuaShapeComponentHandle
{
	uint32 UUID = 0;

	UShapeComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UShapeComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaBoxComponentHandle
{
	uint32 UUID = 0;

	UBoxComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UBoxComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaSphereComponentHandle
{
	uint32 UUID = 0;

	USphereComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<USphereComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaCapsuleComponentHandle
{
	uint32 UUID = 0;

	UCapsuleComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UCapsuleComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};

struct FLuaStaticMeshComponentHandle
{
	uint32 UUID = 0;

	UStaticMeshComponent* Resolve() const
	{
		UObject* Object = UObjectManager::Get().FindByUUID(UUID);
		return Cast<UStaticMeshComponent>(Object);
	}

	bool IsValid() const
	{
		return Resolve() != nullptr;
	}
};
