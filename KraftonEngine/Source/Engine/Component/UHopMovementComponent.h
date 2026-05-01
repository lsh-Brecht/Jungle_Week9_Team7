#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Math/Vector.h"

class FArchive;
class FScene;

class UHopMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UHopMovementComponent, UMovementComponent)

	UHopMovementComponent() = default;
	~UHopMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;
	void ContributeSelectedVisuals(FScene& Scene) const override;

	void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
	const FVector& GetVelocity() const { return Velocity; }

	void SetInitialSpeed(float InInitialSpeed) { InitialSpeed = InInitialSpeed; }
	float GetInitialSpeed() const { return InitialSpeed; }

	float GetMaxSpeed() const { return MaxSpeed; }
	void SetMaxSpeed(float InMaxSpeed) { MaxSpeed = InMaxSpeed; }

	void SetHopCoefficient(float InHopCoefficient) { HopCoefficient = InHopCoefficient; }
	float GetHopCoefficient() const { return HopCoefficient; }

	void SetHopHeight(float InHopHeight) { HopHeight = InHopHeight; }
	float GetHopHeight() const { return HopHeight; }

	FVector GetPreviewVelocity() const;
	void StopSimulating();

protected:
	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	float InitialSpeed = 10.0f;
	float MaxSpeed = 100.0f;
	float HopCoefficient = 1.0f;
	float HopHeight = 25.0f;

	FVector StartLocation = FVector(0.0f, 0.0f, 0.0f);
	float ElapsedTime = 0.0f;
	bool bSimulating = true;
	bool bHasStartLocation = false;
};