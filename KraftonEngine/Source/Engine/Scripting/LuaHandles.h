#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "GameFramework/StaticMeshActor.h"

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

// Lua가 Static Mesh에 직접 접근할 수 없도록 감쌈
// nullptr일 경우를 대비
struct FLuaShapeComponentHandle
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
