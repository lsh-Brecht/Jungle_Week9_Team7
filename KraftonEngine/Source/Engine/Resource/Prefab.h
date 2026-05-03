#pragma once

#include "Core/CoreTypes.h"
#include "SimpleJSON/json.hpp"

class UClass;

class UPrefab
{
public:
	bool LoadFromFile(const FString& FilePath);

	const FString& GetPrefabPath() const { return PrefabPath; }
	UClass* GetBaseActorClass() const { return BaseActorClass; }
	json::JSON& GetPrefabData() { return PrefabData; }
	const json::JSON& GetPrefabData() const { return PrefabData; }

private:
	FString PrefabPath;
	UClass* BaseActorClass = nullptr;
	json::JSON PrefabData;
};
