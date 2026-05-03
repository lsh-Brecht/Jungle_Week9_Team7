#include "GameFramework/PlayerCameraManager.h"

#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "Component/CameraModeComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

namespace
{
	constexpr int32 CameraModeCount = 6;
	constexpr int32 BlendFunctionCount = 4;
	constexpr int32 ProjectionSwitchModeCount = 3;

	float ExpAlpha(float Speed, float DeltaTime)
	{
		if (Speed <= 0.0f)
		{
			return 1.0f;
		}
		return 1.0f - std::exp(-Speed * DeltaTime);
	}

	float LerpFloat(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	int32 NormalizeCameraModeValue(int32 Value)
	{
		if (Value < 0 || Value >= CameraModeCount)
		{
			return static_cast<int32>(ECameraModeId::ThirdPerson);
		}
		return Value;
	}

	int32 NormalizeBlendFunctionValue(int32 Value)
	{
		if (Value < 0 || Value >= BlendFunctionCount)
		{
			return static_cast<int32>(ECameraBlendFunction::EaseInOut);
		}
		return Value;
	}

	int32 NormalizeProjectionSwitchModeValue(int32 Value)
	{
		if (Value < 0 || Value >= ProjectionSwitchModeCount)
		{
			return static_cast<int32>(ECameraProjectionSwitchMode::SwitchAtHalf);
		}
		return Value;
	}
}

void APlayerCameraManager::InitDefaultComponents()
{
	if (!OutputCameraComponent)
	{
		OutputCameraComponent = AddComponent<UCameraComponent>();
		SetRootComponent(OutputCameraComponent);
		OutputCameraComponent->SetHiddenInComponentTree(true);
	}

	SyncRuntimeToEditorValues();
}

void APlayerCameraManager::EndPlay()
{
	OwningController = nullptr;
	ViewTarget = nullptr;
	CurrentView.bValid = false;
	BlendFromView.bValid = false;
	AActor::EndPlay();
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
	FCameraBlendParams Params;
	Params.BlendTime = 0.0f;
	SetViewTargetWithBlend(NewTarget, Params);
}

void APlayerCameraManager::SetViewTargetWithBlend(AActor* NewTarget, const FCameraBlendParams& Params)
{
	ViewTarget = NewTarget;
	ActiveBlendParams = Params;
	BlendElapsedTime = 0.0f;

	if (!CurrentView.bValid && OutputCameraComponent)
	{
		CurrentView = OutputCameraComponent->GetCameraView();
	}

	BlendFromView = CurrentView;
	bIsBlending = Params.BlendTime > 0.0f && BlendFromView.bValid;

	if (!bIsBlending)
	{
		SnapToTarget();
	}
}

void APlayerCameraManager::SetCameraMode(ECameraModeId NewMode, const FCameraBlendParams& Params)
{
	CameraMode = NewMode;
	CameraModeValue = static_cast<int32>(NewMode);
	ActiveBlendParams = Params;
	BlendElapsedTime = 0.0f;

	if (!CurrentView.bValid && OutputCameraComponent)
	{
		CurrentView = OutputCameraComponent->GetCameraView();
	}

	BlendFromView = CurrentView;
	bIsBlending = Params.BlendTime > 0.0f && BlendFromView.bValid;

	if (!bIsBlending)
	{
		SnapToTarget();
	}
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	if (!OutputCameraComponent)
	{
		return;
	}

	FCameraView TargetView;
	if (!CalcDesiredCameraView(DeltaTime, TargetView))
	{
		return;
	}

	if (!CurrentView.bValid)
	{
		CurrentView = TargetView;
	}
	else if (bIsBlending)
	{
		BlendElapsedTime += DeltaTime;
		const float RawAlpha = ActiveBlendParams.BlendTime > 0.0f
			? Clamp(BlendElapsedTime / ActiveBlendParams.BlendTime, 0.0f, 1.0f)
			: 1.0f;

		const float Alpha = EvaluateBlendAlpha(RawAlpha, ActiveBlendParams.Function);
		CurrentView = BlendViews(BlendFromView, TargetView, Alpha, ActiveBlendParams);

		if (RawAlpha >= 1.0f)
		{
			bIsBlending = false;
		}
	}
	else if (bEnableContinuousSmoothing)
	{
		CurrentView = SmoothView(CurrentView, TargetView, DeltaTime);
	}
	else
	{
		CurrentView = TargetView;
	}

	OutputCameraComponent->ApplyCameraView(CurrentView);

	if (UWorld* World = GetWorld())
	{
		World->SetActiveCamera(OutputCameraComponent);
		World->SetViewCamera(OutputCameraComponent);
	}
}

void APlayerCameraManager::SnapToTarget()
{
	if (!OutputCameraComponent)
	{
		return;
	}

	FCameraView TargetView;
	if (CalcDesiredCameraView(0.0f, TargetView))
	{
		CurrentView = TargetView;
		OutputCameraComponent->ApplyCameraView(CurrentView);
	}

	if (UWorld* World = GetWorld())
	{
		World->SetActiveCamera(OutputCameraComponent);
		World->SetViewCamera(OutputCameraComponent);
	}
}

void APlayerCameraManager::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
	Ar << CameraModeValue;
	Ar << DefaultBlendParams.BlendTime;
	Ar << BlendFunctionValue;
	Ar << ProjectionSwitchModeValue;
	Ar << DefaultBlendParams.bBlendLocation;
	Ar << DefaultBlendParams.bBlendRotation;
	Ar << DefaultBlendParams.bBlendFOV;
	Ar << DefaultBlendParams.bBlendOrthoWidth;
	Ar << DefaultBlendParams.bBlendNearFar;
	Ar << bEnableContinuousSmoothing;
	Ar << LocationLagSpeed;
	Ar << RotationLagSpeed;
	Ar << FOVLagSpeed;
	Ar << OrthoWidthLagSpeed;

	if (Ar.IsLoading())
	{
		SyncEditorValuesToRuntime(false);
		CurrentView.bValid = false;
		BlendFromView.bValid = false;
		BlendElapsedTime = 0.0f;
		bIsBlending = false;
	}
}

void APlayerCameraManager::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	static const char* CameraModeNames[] =
	{
		"First Person",
		"Third Person",
		"Orthographic Follow",
		"Perspective Follow",
		"External Camera",
		"Cutscene",
	};

	static const char* BlendFunctionNames[] =
	{
		"Linear",
		"Ease In",
		"Ease Out",
		"Ease In Out",
	};

	static const char* ProjectionSwitchModeNames[] =
	{
		"Switch At Start",
		"Switch At Half",
		"Switch At End",
	};

	OutProps.push_back({ "Active Mode", EPropertyType::Enum, &CameraModeValue, 0.0f, 0.0f, 0.1f, CameraModeNames, CameraModeCount });
	OutProps.push_back({ "Blend Time", EPropertyType::Float, &DefaultBlendParams.BlendTime, 0.0f, 30.0f, 0.01f });
	OutProps.push_back({ "Blend Function", EPropertyType::Enum, &BlendFunctionValue, 0.0f, 0.0f, 0.1f, BlendFunctionNames, BlendFunctionCount });
	OutProps.push_back({ "Projection Switch Mode", EPropertyType::Enum, &ProjectionSwitchModeValue, 0.0f, 0.0f, 0.1f, ProjectionSwitchModeNames, ProjectionSwitchModeCount });
	OutProps.push_back({ "Enable Continuous Smoothing", EPropertyType::Bool, &bEnableContinuousSmoothing });
	OutProps.push_back({ "Location Lag Speed", EPropertyType::Float, &LocationLagSpeed, 0.0f, 1000.0f, 0.1f });
	OutProps.push_back({ "Rotation Lag Speed", EPropertyType::Float, &RotationLagSpeed, 0.0f, 1000.0f, 0.1f });
	OutProps.push_back({ "FOV Lag Speed", EPropertyType::Float, &FOVLagSpeed, 0.0f, 1000.0f, 0.1f });
	OutProps.push_back({ "Ortho Width Lag Speed", EPropertyType::Float, &OrthoWidthLagSpeed, 0.0f, 1000.0f, 0.1f });
}

void APlayerCameraManager::PostEditProperty(const char* PropertyName)
{
	const bool bApplyMode =
		std::strcmp(PropertyName, "Active Mode") == 0 ||
		std::strcmp(PropertyName, "Blend Time") == 0 ||
		std::strcmp(PropertyName, "Blend Function") == 0 ||
		std::strcmp(PropertyName, "Projection Switch Mode") == 0;

	SyncEditorValuesToRuntime(bApplyMode);
}

bool APlayerCameraManager::CalcDesiredCameraView(float DeltaTime, FCameraView& OutView) const
{
	AActor* Target = (ViewTarget && IsAliveObject(ViewTarget)) ? ViewTarget : nullptr;
	if (!Target && OwningController)
	{
		Target = OwningController->GetViewTarget();
	}
	if (!Target && OwningController)
	{
		Target = OwningController->GetPossessedActor();
	}
	if (!Target)
	{
		return false;
	}

	for (UActorComponent* Component : Target->GetComponents())
	{
		UCameraModeComponent* ModeComponent = Cast<UCameraModeComponent>(Component);
		if (!ModeComponent || ModeComponent->GetCameraModeId() != CameraMode)
		{
			continue;
		}

		if (ModeComponent->CalcCameraView(OwningController, Target, DeltaTime, OutView))
		{
			return true;
		}
	}

	return CalcViewFromTargetCamera(DeltaTime, OutView);
}

bool APlayerCameraManager::CalcViewFromTargetCamera(float DeltaTime, FCameraView& OutView) const
{
	(void)DeltaTime;

	AActor* Target = (ViewTarget && IsAliveObject(ViewTarget)) ? ViewTarget : nullptr;
	if (!Target && OwningController)
	{
		Target = OwningController->GetViewTarget();
	}
	if (!Target && OwningController)
	{
		Target = OwningController->GetPossessedActor();
	}
	if (!Target)
	{
		return false;
	}

	for (UActorComponent* Component : Target->GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			OutView = Camera->GetCameraView();
			return OutView.bValid;
		}
	}

	OutView.Location = Target->GetActorLocation();
	OutView.Rotation = Target->GetActorRotation().ToQuaternion();
	OutView.Projection = FCameraProjectionSettings();
	OutView.bValid = true;
	return true;
}

FCameraView APlayerCameraManager::BlendViews(const FCameraView& From, const FCameraView& To, float Alpha, const FCameraBlendParams& Params) const
{
	if (!From.bValid)
	{
		return To;
	}

	FCameraView Out = To;
	Out.bValid = To.bValid;

	if (Params.bBlendLocation)
	{
		Out.Location = FVector::Lerp(From.Location, To.Location, Alpha);
	}

	if (Params.bBlendRotation)
	{
		Out.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, Alpha);
	}

	if (Params.bBlendFOV)
	{
		Out.Projection.FOV = LerpFloat(From.Projection.FOV, To.Projection.FOV, Alpha);
	}

	if (Params.bBlendOrthoWidth)
	{
		Out.Projection.OrthoWidth = LerpFloat(From.Projection.OrthoWidth, To.Projection.OrthoWidth, Alpha);
	}

	if (Params.bBlendNearFar)
	{
		Out.Projection.NearZ = LerpFloat(From.Projection.NearZ, To.Projection.NearZ, Alpha);
		Out.Projection.FarZ = LerpFloat(From.Projection.FarZ, To.Projection.FarZ, Alpha);
	}
	else
	{
		Out.Projection.NearZ = To.Projection.NearZ;
		Out.Projection.FarZ = To.Projection.FarZ;
	}

	switch (Params.ProjectionSwitchMode)
	{
	case ECameraProjectionSwitchMode::SwitchAtStart:
		Out.Projection.bIsOrthographic = To.Projection.bIsOrthographic;
		break;
	case ECameraProjectionSwitchMode::SwitchAtEnd:
		Out.Projection.bIsOrthographic = Alpha >= 1.0f ? To.Projection.bIsOrthographic : From.Projection.bIsOrthographic;
		break;
	case ECameraProjectionSwitchMode::SwitchAtHalf:
	default:
		Out.Projection.bIsOrthographic = Alpha >= 0.5f ? To.Projection.bIsOrthographic : From.Projection.bIsOrthographic;
		break;
	}

	return Out;
}

FCameraView APlayerCameraManager::SmoothView(const FCameraView& From, const FCameraView& To, float DeltaTime) const
{
	if (!From.bValid)
	{
		return To;
	}

	FCameraView Out = To;
	Out.Location = FVector::Lerp(From.Location, To.Location, ExpAlpha(LocationLagSpeed, DeltaTime));
	Out.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, ExpAlpha(RotationLagSpeed, DeltaTime));
	Out.Projection.FOV = LerpFloat(From.Projection.FOV, To.Projection.FOV, ExpAlpha(FOVLagSpeed, DeltaTime));
	Out.Projection.OrthoWidth = LerpFloat(From.Projection.OrthoWidth, To.Projection.OrthoWidth, ExpAlpha(OrthoWidthLagSpeed, DeltaTime));
	Out.Projection.NearZ = To.Projection.NearZ;
	Out.Projection.FarZ = To.Projection.FarZ;
	Out.Projection.AspectRatio = To.Projection.AspectRatio;
	Out.Projection.bIsOrthographic = To.Projection.bIsOrthographic;
	Out.bValid = To.bValid;
	return Out;
}

float APlayerCameraManager::EvaluateBlendAlpha(float RawAlpha, ECameraBlendFunction Function) const
{
	const float T = Clamp(RawAlpha, 0.0f, 1.0f);
	switch (Function)
	{
	case ECameraBlendFunction::EaseIn:
		return T * T;
	case ECameraBlendFunction::EaseOut:
		return 1.0f - (1.0f - T) * (1.0f - T);
	case ECameraBlendFunction::EaseInOut:
		return T < 0.5f ? 2.0f * T * T : 1.0f - std::pow(-2.0f * T + 2.0f, 2.0f) * 0.5f;
	case ECameraBlendFunction::Linear:
	default:
		return T;
	}
}

void APlayerCameraManager::SyncEditorValuesToRuntime(bool bApplyMode)
{
	CameraModeValue = NormalizeCameraModeValue(CameraModeValue);
	BlendFunctionValue = NormalizeBlendFunctionValue(BlendFunctionValue);
	ProjectionSwitchModeValue = NormalizeProjectionSwitchModeValue(ProjectionSwitchModeValue);

	DefaultBlendParams.BlendTime = DefaultBlendParams.BlendTime < 0.0f ? 0.0f : DefaultBlendParams.BlendTime;
	DefaultBlendParams.Function = static_cast<ECameraBlendFunction>(BlendFunctionValue);
	DefaultBlendParams.ProjectionSwitchMode = static_cast<ECameraProjectionSwitchMode>(ProjectionSwitchModeValue);

	LocationLagSpeed = LocationLagSpeed < 0.0f ? 0.0f : LocationLagSpeed;
	RotationLagSpeed = RotationLagSpeed < 0.0f ? 0.0f : RotationLagSpeed;
	FOVLagSpeed = FOVLagSpeed < 0.0f ? 0.0f : FOVLagSpeed;
	OrthoWidthLagSpeed = OrthoWidthLagSpeed < 0.0f ? 0.0f : OrthoWidthLagSpeed;

	if (bApplyMode)
	{
		SetCameraMode(static_cast<ECameraModeId>(CameraModeValue), DefaultBlendParams);
	}
	else
	{
		CameraMode = static_cast<ECameraModeId>(CameraModeValue);
		ActiveBlendParams = DefaultBlendParams;
	}
}

void APlayerCameraManager::SyncRuntimeToEditorValues()
{
	CameraModeValue = static_cast<int32>(CameraMode);
	BlendFunctionValue = static_cast<int32>(DefaultBlendParams.Function);
	ProjectionSwitchModeValue = static_cast<int32>(DefaultBlendParams.ProjectionSwitchMode);
}
