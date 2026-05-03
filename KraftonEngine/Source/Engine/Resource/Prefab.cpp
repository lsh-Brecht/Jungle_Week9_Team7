#include "Resource/Prefab.h"

#include "GameFramework/AActor.h"
#include "Object/UClass.h"
#include "Platform/Paths.h"

#include <filesystem>
#include <fstream>

namespace
{
	UClass* FindActorClassByName(const FString& ClassName)
	{
		for (UClass* Class : UClass::GetAllClasses())
		{
			if (Class && ClassName == Class->GetName() && Class->IsA(AActor::StaticClass()))
			{
				return Class;
			}
		}
		return nullptr;
	}

	std::filesystem::path ResolvePrefabFilePath(const FString& FilePath)
	{
		std::filesystem::path Path(FPaths::ToWide(FilePath));
		if (Path.is_absolute())
		{
			return Path;
		}
		return std::filesystem::path(FPaths::RootDir()) / Path;
	}
}

bool UPrefab::LoadFromFile(const FString& FilePath)
{
	std::ifstream File(ResolvePrefabFilePath(FilePath));
	if (!File.is_open())
	{
		return false;
	}

	FString Content(
		(std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>()
	);

	json::JSON LoadedData = json::JSON::Load(Content);
	FString ClassName = LoadedData["ClassName"].ToString();
	if (ClassName.empty())
	{
		return false;
	}

	UClass* ActorClass = FindActorClassByName(ClassName);
	if (!ActorClass)
	{
		return false;
	}

	PrefabPath = FilePath;
	BaseActorClass = ActorClass;
	PrefabData = LoadedData;
	return true;
}
