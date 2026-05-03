#pragma once

#include "Core/CoreTypes.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

struct FCameraProjectionSettings
{
	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearZ = 0.1f;
	float FarZ = 1000.0f;
	float OrthoWidth = 10.0f;
	bool bIsOrthographic = false;
};

struct FCameraView
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FCameraProjectionSettings Projection;
	bool bValid = false;
};

enum class ECameraBlendFunction : int32
{
	Linear = 0,
	EaseIn = 1,
	EaseOut = 2,
	EaseInOut = 3,
};

enum class ECameraProjectionSwitchMode : int32
{
	SwitchAtStart = 0,
	SwitchAtHalf = 1,
	SwitchAtEnd = 2,
};

struct FCameraBlendParams
{
	float BlendTime = 0.35f;
	ECameraBlendFunction Function = ECameraBlendFunction::EaseInOut;
	ECameraProjectionSwitchMode ProjectionSwitchMode = ECameraProjectionSwitchMode::SwitchAtHalf;

	bool bBlendLocation = true;
	bool bBlendRotation = true;
	bool bBlendFOV = true;
	bool bBlendOrthoWidth = true;
	bool bBlendNearFar = false;
};

enum class ECameraModeId : int32
{
	FirstPerson = 0,
	ThirdPerson = 1,
	OrthographicFollow = 2,
	PerspectiveFollow = 3,
	ExternalCamera = 4,
	Cutscene = 5,
};
