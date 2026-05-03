#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Math/Vector.h"

class FArchive;

class UPawnMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UPawnMovementComponent, UMovementComponent)

	UPawnMovementComponent();
	~UPawnMovementComponent() override = default;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void Serialize(FArchive& Ar) override;

	void AddMovementInput(const FVector& WorldDirection, float Scale = 1.0f) override;
	FVector ConsumeMovementInputVector();
	FVector GetPendingMovementInputVector() const { return PendingMovementInput; }
	void ApplyPendingMovement(float DeltaTime);
	bool ApplyControllerMovementInput(const FControllerMovementInput& Input) override;

private:
	FVector PendingMovementInput = FVector::ZeroVector;
};
