#pragma once

#include "Camera/CameraTypes.h"
#include "Component/ActorComponent.h"

class AActor;
class APlayerController;

class UCameraModeComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UCameraModeComponent, UActorComponent)

	UCameraModeComponent() = default;
	~UCameraModeComponent() override = default;

	virtual ECameraModeId GetCameraModeId() const { return ECameraModeId::ThirdPerson; }

	virtual bool CalcCameraView(
		APlayerController* Controller,
		AActor* ViewTarget,
		float DeltaTime,
		FCameraView& OutView)
	{
		(void)Controller;
		(void)ViewTarget;
		(void)DeltaTime;
		(void)OutView;
		return false;
	}
};
