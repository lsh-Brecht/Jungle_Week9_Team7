#pragma once
#include "GameFramework/AActor.h"

class APawn;
class UCameraComponent;

class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController() = default;
	~APlayerController() override = default;

	void EndPlay() override;

	void Possess(APawn* InPawn);
	void UnPossess();

	APawn* GetPawn() const;

	void SetViewTarget(AActor* InViewTarget);
	AActor* GetViewTarget() const;

	UCameraComponent* ResolveViewCamera() const;

	FRotator GetControlRotation() const { return ControlRotation; }
	void SetControlRotation(const FRotator& InRotation) { ControlRotation = InRotation; }

private:
	APawn* Pawn = nullptr;
	AActor* ViewTarget = nullptr;
	FRotator ControlRotation;
};
