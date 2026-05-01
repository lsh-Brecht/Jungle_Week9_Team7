#include "SphereComponent.h"
#include <algorithm>
#include <cmath>

FBoundingBox USphereComponent::GetWorldAABB() const
{
	const FVector Center = GetWorldLocation();
	const FVector Scale = GetWorldScale();

	const float MaxScale = (std::max)({ std::abs(Scale.X), std::abs(Scale.Y), std::abs(Scale.Z) });
	const float Radius = SphereRadius * MaxScale;
	const FVector Extent(Radius, Radius, Radius);

	return FBoundingBox(Center - Extent, Center + Extent);
}