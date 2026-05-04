#pragma once
#include "Math/Vector.h"
#include "Runtime/Delegate.h"
#include "ActorComponent.h"

class USceneComponent;

class UParryComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UParryComponent, UActorComponent)
	DECLARE_DELEGATE(ParryDelegate)

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction& ThisTickFunction) override;

	void Parry();
	bool IsParrying() const { return bIsParrying; }

private:
	void DeflectNearbyProjectiles();

	bool bIsParrying = false;
	float ParryDuration = 0.3f;
	float CurrentParryTime = 0.0f;
	float ParryRadius = 10.0f;
	float ParryLaunchMultiplier = 10.0f;
	FVector OriginalScale = { 1.0f , 1.0f , 1.0f };
	USceneComponent* ScaleTarget = nullptr;
};
