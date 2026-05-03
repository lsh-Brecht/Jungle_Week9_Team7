#include "HopMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>

namespace
{
	constexpr float KInputTolerance = 1.0e-4f;
	constexpr float KVelocityStopTolerance = 1.0e-3f;
	constexpr float KMaxReasonableSpeed = 100000.0f;
	constexpr float KMaxReasonableAcceleration = 1000000.0f;
	constexpr float KMaxReasonableHopHeight = 100000.0f;
	constexpr float KMaxReasonableHopFrequency = 240.0f;

	FVector FlattenOnWorldUp(const FVector& Vector)
	{
		return FVector(Vector.X, Vector.Y, 0.0f);
	}

	FVector ClampInputMagnitude(const FVector& Input)
	{
		FVector FlatInput = FlattenOnWorldUp(Input);
		const float Length = FlatInput.Length();
		if (Length <= KInputTolerance)
		{
			return FVector::ZeroVector;
		}

		if (Length > 1.0f)
		{
			FlatInput /= Length;
		}
		return FlatInput;
	}

	FVector MoveTowards(const FVector& Current, const FVector& Target, float MaxDelta)
	{
		if (MaxDelta <= 0.0f)
		{
			return Target;
		}

		const FVector Delta = Target - Current;
		const float DeltaLength = Delta.Length();
		if (DeltaLength <= MaxDelta || DeltaLength <= KVelocityStopTolerance)
		{
			return Target;
		}

		return Current + (Delta / DeltaLength) * MaxDelta;
	}

	float GetHopPhase(float ElapsedTime, float Frequency)
	{
		const float CycleTime = std::fmod(ElapsedTime * Frequency, 1.0f);
		return CycleTime >= 0.0f ? CycleTime : CycleTime + 1.0f;
	}
}

IMPLEMENT_CLASS(UHopMovementComponent, UMovementComponent)

UHopMovementComponent::UHopMovementComponent()
{
	bReceiveControllerInput = true;
	ControllerInputPriority = 10;
}

void UHopMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	HopElapsedTime = 0.0f;
	AppliedHopOffset = 0.0f;
	PendingMovementInput = FVector::ZeroVector;
	LastMovementInput = FVector::ZeroVector;
	Velocity = FlattenOnWorldUp(Velocity);
	bSimulating = true;
}

void UHopMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// TODO: Split hop bobbing from root/collision movement. The Pawn root should stay as
	// the gameplay/camera target, while a visual component handles the vertical bob.
	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!bSimulating || !UpdatedSceneComponent || DeltaTime <= 0.0f)
	{
		PendingMovementInput = FVector::ZeroVector;
		return;
	}

	const FVector RawInput = ConsumeFrameMovementInput();
	const FVector MoveInput = ClampInputMagnitude(RawInput);
	const bool bHasMoveInput = MoveInput.Length() > KInputTolerance;

	const float TargetSpeed = bHasMoveInput ? GetEffectiveMoveSpeed() : 0.0f;
	const FVector TargetVelocity = MoveInput * TargetSpeed;
	const float AccelRate = bHasMoveInput ? Acceleration : BrakingDeceleration;
	const float ClampedAccelRate = Clamp(AccelRate, 0.0f, KMaxReasonableAcceleration);

	Velocity = MoveTowards(FlattenOnWorldUp(Velocity), TargetVelocity, ClampedAccelRate * DeltaTime);
	if (Velocity.Length() <= KVelocityStopTolerance)
	{
		Velocity = FVector::ZeroVector;
	}

	const bool bIsHorizontallyMoving = Velocity.Length() > KVelocityStopTolerance;
	const bool bShouldApplyHop = (HopHeight > 0.0f && HopFrequency > 0.0f)
		&& (!bHopOnlyWhenMoving || bHasMoveInput || bIsHorizontallyMoving);

	float NewHopOffset = 0.0f;
	if (bShouldApplyHop)
	{
		HopElapsedTime += DeltaTime;
		const float Phase = GetHopPhase(HopElapsedTime, Clamp(HopFrequency, 0.0f, KMaxReasonableHopFrequency));
		const float ClampedHopHeight = Clamp(HopHeight, 0.0f, KMaxReasonableHopHeight);
		NewHopOffset = std::sin(Phase * FMath::Pi) * ClampedHopHeight;
	}
	else if (bResetHopWhenIdle)
	{
		HopElapsedTime = 0.0f;
	}

	const float HopDelta = NewHopOffset - AppliedHopOffset;
	AppliedHopOffset = NewHopOffset;

	const FVector HorizontalDelta = Velocity * DeltaTime;
	const FVector VerticalDelta(0.0f, 0.0f, HopDelta);
	UpdatedSceneComponent->SetWorldLocation(UpdatedSceneComponent->GetWorldLocation() + HorizontalDelta + VerticalDelta);
}

void UHopMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, KMaxReasonableSpeed, 1.0f });
	OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, KMaxReasonableSpeed, 1.0f });
	OutProps.push_back({ "Hop Coefficient", EPropertyType::Float, &HopCoefficient, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Acceleration", EPropertyType::Float, &Acceleration, 0.0f, KMaxReasonableAcceleration, 10.0f });
	OutProps.push_back({ "Braking Deceleration", EPropertyType::Float, &BrakingDeceleration, 0.0f, KMaxReasonableAcceleration, 10.0f });
	OutProps.push_back({ "Hop Height", EPropertyType::Float, &HopHeight, 0.0f, KMaxReasonableHopHeight, 1.0f });
	OutProps.push_back({ "Hop Frequency", EPropertyType::Float, &HopFrequency, 0.0f, KMaxReasonableHopFrequency, 0.1f });
	OutProps.push_back({ "Hop Only When Moving", EPropertyType::Bool, &bHopOnlyWhenMoving });
	OutProps.push_back({ "Reset Hop When Idle", EPropertyType::Bool, &bResetHopWhenIdle });
	OutProps.push_back({ "Simulating", EPropertyType::Bool, &bSimulating });
}

void UHopMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);

	// Keep the old field order first for compatibility with existing binary archives.
	Ar << Velocity;
	Ar << InitialSpeed;
	Ar << MaxSpeed;
	Ar << HopCoefficient;
	Ar << HopHeight;
	Ar << bSimulating;

	// New movement/hop configuration. Runtime input vectors are intentionally transient.
	Ar << Acceleration;
	Ar << BrakingDeceleration;
	Ar << HopFrequency;
	Ar << bHopOnlyWhenMoving;
	Ar << bResetHopWhenIdle;

	if (Ar.IsLoading())
	{
		MovementInput = FVector::ZeroVector;
		PendingMovementInput = FVector::ZeroVector;
		LastMovementInput = FVector::ZeroVector;
		HopElapsedTime = 0.0f;
		AppliedHopOffset = 0.0f;
		Velocity = FlattenOnWorldUp(Velocity);
	}
}

void UHopMovementComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	(void)Scene;
}

void UHopMovementComponent::SetMovementInput(const FVector& InWorldInput)
{
	MovementInput = ClampInputMagnitude(InWorldInput);
}

void UHopMovementComponent::ClearMovementInput()
{
	MovementInput = FVector::ZeroVector;
	PendingMovementInput = FVector::ZeroVector;
	LastMovementInput = FVector::ZeroVector;
}

void UHopMovementComponent::AddMovementInput(const FVector& WorldDirection, float Scale)
{
	if (Scale == 0.0f)
	{
		return;
	}

	const FVector Direction = ClampInputMagnitude(WorldDirection);
	if (Direction.Length() <= KInputTolerance)
	{
		return;
	}

	PendingMovementInput += Direction * Scale;
}

void UHopMovementComponent::SetLocalMovementInput(const FVector& InLocalInput)
{
	SetMovementInput(BuildWorldInputFromLocal(InLocalInput));
}

void UHopMovementComponent::AddLocalMovementInput(const FVector& InLocalDirection, float Scale)
{
	AddMovementInput(BuildWorldInputFromLocal(InLocalDirection), Scale);
}

bool UHopMovementComponent::ApplyControllerMovementInput(const FControllerMovementInput& Input)
{
	if (Input.WorldDirection.IsNearlyZero())
	{
		return false;
	}

	AddMovementInput(Input.WorldDirection, 1.0f);
	return true;
}

FVector UHopMovementComponent::BuildWorldInputFromLocal(const FVector& InLocalInput) const
{
	USceneComponent* BasisComponent = GetUpdatedComponent();
	if (!BasisComponent)
	{
		return ClampInputMagnitude(InLocalInput);
	}

	const FVector Forward = ClampInputMagnitude(BasisComponent->GetForwardVector());
	const FVector Right = ClampInputMagnitude(BasisComponent->GetRightVector());
	return ClampInputMagnitude((Forward * InLocalInput.X) + (Right * InLocalInput.Y));
}

FVector UHopMovementComponent::ConsumeFrameMovementInput()
{
	const FVector CombinedInput = MovementInput + PendingMovementInput;
	PendingMovementInput = FVector::ZeroVector;
	LastMovementInput = ClampInputMagnitude(CombinedInput);
	return LastMovementInput;
}

float UHopMovementComponent::GetEffectiveMoveSpeed() const
{
	const float BaseSpeed = Clamp(InitialSpeed, 0.0f, KMaxReasonableSpeed);
	const float SpeedScale = Clamp(HopCoefficient, 0.0f, 100.0f);
	const float SpeedCap = Clamp(MaxSpeed, 0.0f, KMaxReasonableSpeed);
	const float ScaledSpeed = BaseSpeed * SpeedScale;
	return SpeedCap > 0.0f ? Clamp(ScaledSpeed, 0.0f, SpeedCap) : ScaledSpeed;
}

void UHopMovementComponent::RemoveAppliedHopOffset()
{
	if (std::fabs(AppliedHopOffset) <= KVelocityStopTolerance)
	{
		AppliedHopOffset = 0.0f;
		return;
	}

	if (USceneComponent* UpdatedSceneComponent = GetUpdatedComponent())
	{
		UpdatedSceneComponent->SetWorldLocation(
			UpdatedSceneComponent->GetWorldLocation() + FVector(0.0f, 0.0f, -AppliedHopOffset));
	}
	AppliedHopOffset = 0.0f;
}

void UHopMovementComponent::StopMovementImmediately()
{
	ClearMovementInput();
	Velocity = FVector::ZeroVector;
	HopElapsedTime = 0.0f;
	RemoveAppliedHopOffset();
}

void UHopMovementComponent::StopSimulating()
{
	bSimulating = false;
	StopMovementImmediately();
}

FVector UHopMovementComponent::GetPreviewVelocity() const
{
	const FVector Input = ClampInputMagnitude(MovementInput + PendingMovementInput);
	if (Input.Length() <= KInputTolerance)
	{
		return FVector::ZeroVector;
	}
	return Input * GetEffectiveMoveSpeed();
}
