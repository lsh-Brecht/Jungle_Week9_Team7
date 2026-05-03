#pragma once
#include "GameFramework/AActor.h"
#include "Math/Rotator.h"

class APawn;
class UCameraComponent;
class UControllerInputComponent;
class FArchive;

class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController() = default;
	~APlayerController() override = default;

	void Serialize(FArchive& Ar) override;
	void InitDefaultComponents() override;
	void EndPlay() override;

	void Possess(AActor* InActor);
	void UnPossess();
	AActor* GetPossessedActor() const;

	void SetViewTarget(AActor* InViewTarget);
	AActor* GetViewTarget() const;

	UCameraComponent* ResolveViewCamera() const;
	UControllerInputComponent* FindControllerInputComponent() const;

	FRotator GetControlRotation() const { return ControlRotation; }
	void SetControlRotation(const FRotator& InRotation) { ControlRotation = InRotation; }

private:
	AActor* PossessedActor = nullptr;
	AActor* ViewTarget = nullptr;
	FRotator ControlRotation;
};
