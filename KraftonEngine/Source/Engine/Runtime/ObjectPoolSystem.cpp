#include "Runtime/ObjectPoolSystem.h"

#include "Component/ActorComponent.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Runtime/PooledObjectInterface.h"

#include <algorithm>

namespace
{
	bool IsReusablePooledActor(AActor* Actor, UWorld* World)
	{
		return Actor && IsAliveObject(Actor) && Actor->GetWorld() == World;
	}

	int32 CountReusablePooledActors(const FPooledClassBucket& Bucket, UWorld* World)
	{
		int32 Count = 0;
		for (AActor* Actor : Bucket.InactiveActors)
		{
			if (IsReusablePooledActor(Actor, World))
			{
				++Count;
			}
		}
		return Count;
	}
}

void FObjectPoolSystem::Shutdown()
{
	TArray<AActor*> ActorsToDestroy;
	for (auto& Pair : PoolMap)
	{
		ActorsToDestroy.insert(
			ActorsToDestroy.end(),
			Pair.second.InactiveActors.begin(),
			Pair.second.InactiveActors.end()
		);
		Pair.second.InactiveActors.clear();
	}
	PoolMap.clear();

	for (AActor* Actor : ActorsToDestroy)
	{
		DestroyActor(Actor);
	}
}

AActor* FObjectPoolSystem::AcquireActor(UWorld* World, UClass* Class, const FVector& Location, const FRotator& Rotation)
{
	if (!World || !Class || !Class->IsA(AActor::StaticClass()))
	{
		return nullptr;
	}

	FPooledClassBucket& Bucket = GetOrCreateBucket(Class);
	AActor* Actor = nullptr;
	TArray<AActor*> SkippedActors;

	while (!Bucket.InactiveActors.empty())
	{
		AActor* Candidate = Bucket.InactiveActors.back();
		Bucket.InactiveActors.pop_back();

		if (IsReusablePooledActor(Candidate, World))
		{
			Actor = Candidate;
			break;
		}

		if (Candidate && IsAliveObject(Candidate))
		{
			SkippedActors.push_back(Candidate);
		}
	}

	Bucket.InactiveActors.insert(Bucket.InactiveActors.end(), SkippedActors.begin(), SkippedActors.end());

	if (!Actor)
	{
		Actor = SpawnActorForPool(World, Class, false);
		if (!Actor)
		{
			return nullptr;
		}
	}

	ActivateActor(Actor, Location, Rotation);
	return Actor;
}

bool FObjectPoolSystem::ReleaseActor(AActor* Actor)
{
	if (!Actor || Actor->IsPooledActorInactive() || !Actor->GetClass())
	{
		return false;
	}

	FPooledClassBucket& Bucket = GetOrCreateBucket(Actor->GetClass());
	DispatchReturnCallbacks(Actor);

	if (static_cast<int32>(Bucket.InactiveActors.size()) >= Bucket.MaxPoolSize)
	{
		DestroyActor(Actor);
		return true;
	}

	DeactivateActor(Actor);
	Bucket.InactiveActors.push_back(Actor);
	return true;
}

int32 FObjectPoolSystem::WarmUp(UWorld* World, UClass* Class, int32 Count)
{
	if (!World || !Class || !Class->IsA(AActor::StaticClass()) || Count <= 0)
	{
		return 0;
	}

	FPooledClassBucket& Bucket = GetOrCreateBucket(Class);
	const int32 ExistingWorldActors = CountReusablePooledActors(Bucket, World);
	const int32 ActorsToCreate = Count - ExistingWorldActors;

	if (ActorsToCreate <= 0)
	{
		return 0;
	}

	const int32 RequiredTotalCapacity = static_cast<int32>(Bucket.InactiveActors.size()) + ActorsToCreate;
	if (Bucket.MaxPoolSize < RequiredTotalCapacity)
	{
		Bucket.MaxPoolSize = RequiredTotalCapacity;
	}

	int32 CreatedCount = 0;
	while (CreatedCount < ActorsToCreate)
	{
		AActor* Actor = SpawnActorForPool(World, Class, true);
		if (!Actor)
		{
			break;
		}

		DeactivateActor(Actor);
		Bucket.InactiveActors.push_back(Actor);
		++CreatedCount;
	}

	return CreatedCount;
}

void FObjectPoolSystem::SetMaxPoolSize(UClass* Class, int32 MaxPoolSize)
{
	if (!Class)
	{
		return;
	}

	FPooledClassBucket& Bucket = GetOrCreateBucket(Class);
	Bucket.MaxPoolSize = (std::max)(0, MaxPoolSize);

	while (static_cast<int32>(Bucket.InactiveActors.size()) > Bucket.MaxPoolSize)
	{
		AActor* Actor = Bucket.InactiveActors.back();
		Bucket.InactiveActors.pop_back();
		DestroyActor(Actor);
	}
}

int32 FObjectPoolSystem::GetMaxPoolSize(UClass* Class) const
{
	const FPooledClassBucket* Bucket = FindBucket(Class);
	return Bucket ? Bucket->MaxPoolSize : 0;
}

int32 FObjectPoolSystem::GetInactiveCount(UClass* Class) const
{
	const FPooledClassBucket* Bucket = FindBucket(Class);
	return Bucket ? static_cast<int32>(Bucket->InactiveActors.size()) : 0;
}

void FObjectPoolSystem::ClearWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	for (auto& Pair : PoolMap)
	{
		TArray<AActor*>& InactiveActors = Pair.second.InactiveActors;
		InactiveActors.erase(
			std::remove_if(
				InactiveActors.begin(),
				InactiveActors.end(),
				[World](AActor* Actor)
				{
					if (!Actor || !IsAliveObject(Actor) || Actor->GetWorld() == World)
					{
						if (Actor && IsAliveObject(Actor))
						{
							Actor->SetPooledActorState(false, false);
						}
						return true;
					}
					return false;
				}
			),
			InactiveActors.end()
		);
	}
}

void FObjectPoolSystem::ForgetActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (auto& Pair : PoolMap)
	{
		TArray<AActor*>& InactiveActors = Pair.second.InactiveActors;
		InactiveActors.erase(
			std::remove(InactiveActors.begin(), InactiveActors.end(), Actor),
			InactiveActors.end()
		);
	}
}

AActor* FObjectPoolSystem::SpawnActorForPool(UWorld* World, UClass* Class, bool bStartInactive)
{
	UObject* Object = FObjectFactory::Get().Create(Class->GetName(), World);
	if (!Object)
	{
		return nullptr;
	}

	AActor* Actor = Cast<AActor>(Object);
	if (!Actor)
	{
		UObjectManager::Get().DestroyObject(Object);
		return nullptr;
	}

	if (bStartInactive)
	{
		Actor->SetPooledActorState(true, true);
	}

	World->AddActor(Actor);
	return Actor;
}

void FObjectPoolSystem::ActivateActor(AActor* Actor, const FVector& Location, const FRotator& Rotation)
{
	if (!Actor)
	{
		return;
	}

	Actor->SetPooledActorState(true, false);
	Actor->SetActorLocation(Location);
	Actor->SetActorRotation(Rotation);
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
	Actor->SetActorTickEnabled(true);

	if (UWorld* World = Actor->GetWorld())
	{
		if (World->HasBegunPlay() && !Actor->HasActorBegunPlay())
		{
			Actor->BeginPlay();
		}
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component)
		{
			Component->Activate();
		}
	}

	DispatchSpawnCallbacks(Actor);
}

void FObjectPoolSystem::DeactivateActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	Actor->SetPooledActorState(true, true);
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component)
		{
			Component->Deactivate();
		}
	}
}

void FObjectPoolSystem::DispatchSpawnCallbacks(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (IPooledObjectInterface* PooledActor = dynamic_cast<IPooledObjectInterface*>(Actor))
	{
		PooledActor->OnSpawnFromPool();
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (IPooledObjectInterface* PooledComponent = dynamic_cast<IPooledObjectInterface*>(Component))
		{
			PooledComponent->OnSpawnFromPool();
		}
	}
}

void FObjectPoolSystem::DispatchReturnCallbacks(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (IPooledObjectInterface* PooledActor = dynamic_cast<IPooledObjectInterface*>(Actor))
	{
		PooledActor->OnReturnToPool();
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (IPooledObjectInterface* PooledComponent = dynamic_cast<IPooledObjectInterface*>(Component))
		{
			PooledComponent->OnReturnToPool();
		}
	}
}

void FObjectPoolSystem::DestroyActor(AActor* Actor)
{
	if (!Actor || !IsAliveObject(Actor))
	{
		return;
	}

	ForgetActor(Actor);
	Actor->SetPooledActorState(false, false);

	if (UWorld* World = Actor->GetWorld())
	{
		World->DestroyActor(Actor);
	}
	else
	{
		UObjectManager::Get().DestroyObject(Actor);
	}
}

FPooledClassBucket* FObjectPoolSystem::FindBucket(UClass* Class)
{
	auto It = PoolMap.find(Class);
	return It != PoolMap.end() ? &It->second : nullptr;
}

const FPooledClassBucket* FObjectPoolSystem::FindBucket(UClass* Class) const
{
	auto It = PoolMap.find(Class);
	return It != PoolMap.end() ? &It->second : nullptr;
}

FPooledClassBucket& FObjectPoolSystem::GetOrCreateBucket(UClass* Class)
{
	return PoolMap[Class];
}
