#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "GameFramework/AActor.h"

#include <type_traits>

class UClass;
class UWorld;

struct FPooledClassBucket
{
	TArray<AActor*> InactiveActors;
	int32 MaxPoolSize = 100;
};

class FObjectPoolSystem : public TSingleton<FObjectPoolSystem>
{
	friend class TSingleton<FObjectPoolSystem>;

public:
	void Initialize() {}
	void Shutdown();

	template<typename T>
	T* AcquireActor(UWorld* World, UClass* Class, const FVector& Location, const FRotator& Rotation)
	{
		static_assert(std::is_base_of_v<AActor, T>, "AcquireActor<T>: T must derive from AActor");
		return Cast<T>(AcquireActor(World, Class ? Class : T::StaticClass(), Location, Rotation));
	}

	template<typename T>
	T* AcquireActor(UWorld* World, const FVector& Location, const FRotator& Rotation)
	{
		return AcquireActor<T>(World, T::StaticClass(), Location, Rotation);
	}

	AActor* AcquireActor(UWorld* World, UClass* Class, const FVector& Location, const FRotator& Rotation);
	bool ReleaseActor(AActor* Actor);
	int32 WarmUp(UWorld* World, UClass* Class, int32 Count);

	void SetMaxPoolSize(UClass* Class, int32 MaxPoolSize);
	int32 GetMaxPoolSize(UClass* Class) const;
	int32 GetInactiveCount(UClass* Class) const;

	void ClearWorld(UWorld* World);
	void ForgetActor(AActor* Actor);

private:
	FObjectPoolSystem() = default;

	AActor* SpawnActorForPool(UWorld* World, UClass* Class, bool bStartInactive);
	void ActivateActor(AActor* Actor, const FVector& Location, const FRotator& Rotation);
	void DeactivateActor(AActor* Actor);
	void DispatchSpawnCallbacks(AActor* Actor);
	void DispatchReturnCallbacks(AActor* Actor);
	void DestroyActor(AActor* Actor);

	FPooledClassBucket* FindBucket(UClass* Class);
	const FPooledClassBucket* FindBucket(UClass* Class) const;
	FPooledClassBucket& GetOrCreateBucket(UClass* Class);

	TMap<UClass*, FPooledClassBucket> PoolMap;
};
