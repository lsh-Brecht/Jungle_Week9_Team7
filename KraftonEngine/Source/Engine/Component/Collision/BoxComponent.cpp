#include "BoxComponent.h"
#include "Math/Matrix.h"
#include <cmath>

FBoundingBox UBoxComponent::GetWorldAABB() const
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FVector Center = GetWorldLocation();

	const float Ex = std::abs(WorldMatrix.M[0][0]) * BoxExtent.X
		+ std::abs(WorldMatrix.M[1][0]) * BoxExtent.Y
		+ std::abs(WorldMatrix.M[2][0]) * BoxExtent.Z;
	const float Ey = std::abs(WorldMatrix.M[0][1]) * BoxExtent.X
		+ std::abs(WorldMatrix.M[1][1]) * BoxExtent.Y
		+ std::abs(WorldMatrix.M[2][1]) * BoxExtent.Z;
	const float Ez = std::abs(WorldMatrix.M[0][2]) * BoxExtent.X
		+ std::abs(WorldMatrix.M[1][2]) * BoxExtent.Y
		+ std::abs(WorldMatrix.M[2][2]) * BoxExtent.Z;

	return FBoundingBox(
		Center - FVector(Ex, Ey, Ez),
		Center + FVector(Ex, Ey, Ez));
}