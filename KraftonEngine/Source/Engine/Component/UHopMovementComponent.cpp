#include "UHopMovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cmath>

namespace
{
	FVector GetHopDirection(const UHopMovementComponent* MovementComponent)
	{
		FVector Direction = MovementComponent ? MovementComponent->GetVelocity() : FVector(0.0f, 0.0f, 0.0f);

		if (Direction.Length() <= FMath::Epsilon)
		{
			USceneComponent* SourceComponent = MovementComponent ? MovementComponent->GetUpdatedComponent() : nullptr;
			if (!SourceComponent && MovementComponent)
			{
				AActor* OwnerActor = MovementComponent->GetOwner();
				SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
			}

			if (SourceComponent)
			{
				Direction = SourceComponent->GetForwardVector();
			}
		}

		const FVector WorldUp(0.0f, 0.0f, 1.0f);
		Direction = Direction - WorldUp * Direction.Dot(WorldUp);

		if (Direction.Length() <= FMath::Epsilon)
		{
			Direction = FVector(1.0f, 0.0f, 0.0f);
		}

		return Direction.Normalized();
	}

	float GetEffectiveSpeed(float InitialSpeed, float MaxSpeed)
	{
		float EffectiveSpeed = InitialSpeed;
		if (EffectiveSpeed < 0.0f)
		{
			EffectiveSpeed = 0.0f;
		}

		if (MaxSpeed > 0.0f)
		{
			EffectiveSpeed = Clamp(EffectiveSpeed, 0.0f, MaxSpeed);
		}

		return EffectiveSpeed;
	}

	float GetHopPhase(float ElapsedTime)
	{
		const float CycleTime = std::fmod(ElapsedTime, 1.0f);
		return CycleTime >= 0.0f ? CycleTime : CycleTime + 1.0f;
	}
}

IMPLEMENT_CLASS(UHopMovementComponent, UMovementComponent)

void UHopMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	StartLocation = UpdatedSceneComponent->GetWorldLocation();
	ElapsedTime = 0.0f;
	bHasStartLocation = true;
	bSimulating = true;
}

void UHopMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSimulating)
	{
		return;
	}

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	if (!bHasStartLocation)
	{
		StartLocation = UpdatedSceneComponent->GetWorldLocation();
		bHasStartLocation = true;
	}

	if (DeltaTime > 0.0f)
	{
		ElapsedTime += DeltaTime;
	}

	const FVector Direction = GetHopDirection(this);
	const float EffectiveSpeed = GetEffectiveSpeed(InitialSpeed, MaxSpeed);
	const float HopScale = Clamp(HopCoefficient, 0.0f, 100.0f);
	const float HopDistancePerSecond = EffectiveSpeed * HopScale;
	const float HopHeightValue = Clamp(HopHeight, 0.0f, 100000.0f);

	const float TravelDistance = HopDistancePerSecond * ElapsedTime;
	const float HopPhase = GetHopPhase(ElapsedTime);
	const float VerticalOffset = std::sin(HopPhase * FMath::Pi) * HopHeightValue;

	const FVector NewLocation = StartLocation + (Direction * TravelDistance) + FVector(0.0f, 0.0f, VerticalOffset);
	UpdatedSceneComponent->SetWorldLocation(NewLocation);
}

void UHopMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, 0.0f, 10.0f });
	OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, 0.0f, 10.0f });
	OutProps.push_back({ "Hop Coefficient", EPropertyType::Float, &HopCoefficient, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Hop Height", EPropertyType::Float, &HopHeight, 0.0f, 0.0f, 1.0f });
}

void UHopMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Velocity;
	Ar << InitialSpeed;
	Ar << MaxSpeed;
	Ar << HopCoefficient;
	Ar << HopHeight;
	Ar << bSimulating;
}

void UHopMovementComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	(void)Scene;
}

void UHopMovementComponent::StopSimulating()
{
	bSimulating = false;
	Velocity = FVector(0.0f, 0.0f, 0.0f);
}

FVector UHopMovementComponent::GetPreviewVelocity() const
{
	const FVector Direction = GetHopDirection(this);
	const float EffectiveSpeed = GetEffectiveSpeed(InitialSpeed, MaxSpeed);
	const float HopScale = Clamp(HopCoefficient, 0.0f, 100.0f);
	return Direction * (EffectiveSpeed * HopScale);
}