#include "LuaWorldLibrary.h"

#include "Core/Log.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Runtime/Engine.h"

#include "GameFramework/World.h"
#include "GameFramework/StaticMeshActor.h"

#include "Component/SceneComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/StaticMeshComponent.h"

#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <d3d11.h>

UWorld* FLuaWorldLibrary::GetActiveWorld()
{
	if (!GEngine)
	{
		UE_LOG("[Lua] Active World lookup failed: GEngine is null.");
		return nullptr;
	}

	UWorld* World = GEngine->GetWorld();

	if (!World)
	{
		UE_LOG("[Lua] Active World lookup failed: World is null.");
		return nullptr;
	}

	return World;
}
AActor* FLuaWorldLibrary::SpawnActorByClassName(const FString& ClassName, const FVector& Location)
{
	UWorld* World = GetActiveWorld();

	if (!World)
	{
		return nullptr;
	}

	UObject* Object = FObjectFactory::Get().Create(ClassName, World);

	if (!Object)
	{
		UE_LOG("[Lua] SpawnActor failed: unknown class name = %s", ClassName.c_str());
		return nullptr;
	}

	AActor* Actor = Cast<AActor>(Object);

	if (!Actor)
	{
		UE_LOG("[Lua] SpawnActor failed: class is not AActor. ClassName = %s", ClassName.c_str());
		UObjectManager::Get().DestroyObject(Object);
		return nullptr;
	}

	EnsureRootComponent(Actor);
	Actor->SetActorLocation(Location);
	World->AddActor(Actor);

	return Actor;
}

AActor* FLuaWorldLibrary::SpawnStaticMeshActor(const FString& StaticMeshPath, const FVector& Location)
{
	UWorld* World = GetActiveWorld();

	if (!World)
	{
		return nullptr;
	}

	AStaticMeshActor* Actor = UObjectManager::Get().CreateObject<AStaticMeshActor>(World);

	if (!Actor)
	{
		UE_LOG("[Lua] SpawnStaticMeshActor failed.");
		return nullptr;
	}

	Actor->InitDefaultComponents(StaticMeshPath);
	Actor->SetActorLocation(Location);
	World->AddActor(Actor);

	return Actor;
}
bool FLuaWorldLibrary::DestroyActor(AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	UWorld* World = Actor->GetWorld();

	if (!World)
	{
		UE_LOG("[Lua] DestroyActor failed: Actor has no World.");
		return false;
	}

	const uint32 ActorUUID = Actor->GetUUID();

	// TODO: FLuaDelegateManager::Get().ClearActor(ActorUUID);
	(void)ActorUUID;

	World->DestroyActor(Actor);

	return true;
}
bool FLuaWorldLibrary::RemoveComponent(AActor* Actor, UActorComponent* Component)
{
	if (!Actor || !Component)
	{
		return false;
	}

	if (Actor->GetRootComponent() == Component)
	{
		UE_LOG("[Lua] RemoveComponent failed: cannot remove RootComponent from Lua.");
		return false;
	}

	Actor->RemoveComponent(Component);
	return true;
}
bool FLuaWorldLibrary::RemoveShape(AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	bool bRemoved = false;

	TArray<UActorComponent*> Components = Actor->GetComponents();

	for (UActorComponent* Component : Components)
	{
		if (UShapeComponent* Shape = Cast<UShapeComponent>(Component))
		{
			if (RemoveComponent(Actor, Shape))
			{
				bRemoved = true;
			}
		}
	}

	return bRemoved;
}
UStaticMeshComponent* FLuaWorldLibrary::GetOrAddStaticMeshComponent(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UStaticMeshComponent* Existing = FindComponent<UStaticMeshComponent>(Actor))
	{
		return Existing;
	}

	EnsureRootComponent(Actor);

	UStaticMeshComponent* MeshComponent = Actor->AddComponent<UStaticMeshComponent>();

	if (!MeshComponent)
	{
		UE_LOG("[Lua] Failed to add StaticMeshComponent.");
		return nullptr;
	}

	MeshComponent->AttachToComponent(Actor->GetRootComponent());

	PostComponentAdded(Actor, MeshComponent);

	return MeshComponent;
}
bool FLuaWorldLibrary::SetStaticMesh(UStaticMeshComponent* MeshComponent, const FString& StaticMeshPath)
{
	if (!MeshComponent)
	{
		UE_LOG("[Lua] SetStaticMesh failed: invalid StaticMeshComponent.");
		return false;
	}

	UWorld* World = MeshComponent->GetWorld();

	if (!World)
	{
		UE_LOG("[Lua] SetStaticMesh failed: StaticMeshComponent has no World.");
		return false;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();

	if (!Device)
	{
		UE_LOG("[Lua] SetStaticMesh failed: Failed to get D3D11 Device.");
		return false;
	}

	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(StaticMeshPath, Device);

	if (!Mesh)
	{
		UE_LOG("[Lua] SetStaticMesh failed: cannot load static mesh = %s", StaticMeshPath.c_str());
		return false;
	}

	MeshComponent->SetStaticMesh(Mesh);
	return true;
}

bool FLuaWorldLibrary::SetMaterial(UStaticMeshComponent* MeshComponent, int32 ElementIndex, const FString& MaterialPath)
{
	if (!MeshComponent)
	{
		UE_LOG("[Lua] SetMaterial failed: invalid StaticMeshComponent.");
		return false;
	}

	UMaterial* Material = nullptr;

	if (!MaterialPath.empty() && MaterialPath != "None")
	{
		Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
	}

	MeshComponent->SetMaterial(ElementIndex, Material);
	return true;
}

bool FLuaWorldLibrary::SetMaterialScalarParameter(UStaticMeshComponent* MeshComponent, int32 ElementIndex, const FString& ParameterName, float Value)
{
	if (!MeshComponent)
	{
		return false;
	}

	UMaterial* Material = MeshComponent->GetMaterial(ElementIndex);

	if (!Material)
	{
		UE_LOG("[Lua] SetMaterialScalarParameter failed: invalid Material.");
		return false;
	}

	if (!Material->SetScalarParameter(ParameterName, Value))
	{
		UE_LOG("[Lua] SetMaterialScalarParameter failed: Material does not have parameter = %s", ParameterName.c_str());
		return false;
	}

	MeshComponent->SetMaterial(ElementIndex, Material);
	return true;
}

bool FLuaWorldLibrary::SetMaterialVectorParameter(UStaticMeshComponent* MeshComponent, int32 ElementIndex, const FString& ParameterName, const FVector& Value)
{
	if (!MeshComponent)
	{
		return false;
	}

	UMaterial* Material = MeshComponent->GetMaterial(ElementIndex);

	if (!Material)
	{
		UE_LOG("[Lua] SetMaterialVectorParameter failed: invalid Material.");
		return false;
	}

	if (!Material->SetVector3Parameter(ParameterName, Value))
	{
		UE_LOG("[Lua] SetMaterialVectorParameter failed: Material does not have parameter = %s", ParameterName.c_str());
		return false;
	}

	MeshComponent->SetMaterial(ElementIndex, Material);
	return true;
}

bool FLuaWorldLibrary::SetMaterialVector4Parameter(UStaticMeshComponent* MeshComponent, int32 ElementIndex, const FString& ParameterName, const FVector4& Value)
{
	if (!MeshComponent)
	{
		return false;
	}

	UMaterial* Material = MeshComponent->GetMaterial(ElementIndex);

	if (!Material)
	{
		UE_LOG("[Lua] SetMaterialVector4Parameter failed: invalid Material.");
		return false;
	}

	if (!Material->SetVector4Parameter(ParameterName, Value))
	{
		UE_LOG("[Lua] SetMaterialVector4Parameter failed: Material does not have parameter = %s", ParameterName.c_str());
		return false;
	}

	MeshComponent->SetMaterial(ElementIndex, Material);
	return true;
}

void FLuaWorldLibrary::PostComponentAdded(AActor* Actor, UActorComponent* Component)
{
	if (!Actor || !Component)
	{
		return;
	}

	if (Actor->HasActorBegunPlay())
	{
		Component->BeginPlay();
	}

	if (UMovementComponent* Movement = Cast<UMovementComponent>(Component))
	{
		Movement->ResolveUpdatedComponent();
	}
}

void FLuaWorldLibrary::EnsureRootComponent(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (Actor->GetRootComponent())
	{
		return;
	}

	USceneComponent* Root = Actor->AddComponent<USceneComponent>();

	if (!Root)
	{
		UE_LOG("[Lua] EnsureRootComponent failed.");
		return;
	}

	Actor->SetRootComponent(Root);
}
