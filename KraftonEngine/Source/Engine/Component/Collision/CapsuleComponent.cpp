#include "CapsuleComponent.h"
#include <algorithm>
#include <cmath>

void UCapsuleComponent::GetSegmentPoints(FVector& OutP0, FVector& OutP1) const
{
	const FVector Scale = GetWorldScale();
	const float RadiusScale = std::max(std::abs(Scale.X), std::abs(Scale.Y));
	const float HalfHeightScale = std::abs(Scale.Z);

	const float ScaledRadius = CapsuleRadius * RadiusScale;
	const float ScaledHalfHeight = CapsuleHalfHeight * HalfHeightScale;
	const float SegmentHalfLength = std::max(ScaledHalfHeight - ScaledRadius, 0.0f);

	const FVector Center = GetWorldLocation();
	const FVector Axis = GetUpVector();

	OutP0 = Center - Axis * SegmentHalfLength;
	OutP1 = Center + Axis * SegmentHalfLength;
}

FBoundingBox UCapsuleComponent::GetWorldAABB() const
{
	const FVector Scale = GetWorldScale();
	const float RadiusScale = std::max(std::abs(Scale.X), std::abs(Scale.Y));
	const float Radius = CapsuleRadius * RadiusScale;

	FVector P0;
	FVector P1;
	GetSegmentPoints(P0, P1);

	FVector Min(
		std::min(P0.X, P1.X),
		std::min(P0.Y, P1.Y),
		std::min(P0.Z, P1.Z));
	FVector Max(
		std::max(P0.X, P1.X),
		std::max(P0.Y, P1.Y),
		std::max(P0.Z, P1.Z));

	const FVector Expand(Radius, Radius, Radius);
	return FBoundingBox(Min - Expand, Max + Expand);
}