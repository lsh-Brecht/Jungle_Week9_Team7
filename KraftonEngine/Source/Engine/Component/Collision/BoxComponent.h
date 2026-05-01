#pragma once
#include "ShapeComponent.h"
#include "Math/Vector.h"
#include "CollisionTypes.h"

class UBoxComponent : public UShapeComponent
{
public:
	virtual ECollisionShapeType GetCollisionShapeType() const override
	{
		return ECollisionShapeType::Box;
	}

	virtual FBoundingBox GetWorldAABB() const override;

	const FVector& GetBoxExtent() const { return BoxExtent; }
	void SetBoxExtent(const FVector& InExtent) { BoxExtent = InExtent; }

private:
	FVector BoxExtent = FVector::ZeroVector;
};