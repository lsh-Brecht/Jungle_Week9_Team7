#pragma once
#include "ShapeComponent.h"
#include "CollisionTypes.h"

class USphereComponent : public UShapeComponent
{
public:
	virtual ECollisionShapeType GetCollisionShapeType() const override
	{
		return ECollisionShapeType::Sphere;
	}

	virtual FBoundingBox GetWorldAABB() const override;

	float GetSphereRadius() const { return SphereRadius; }
	void SetSphereRadius(float InRadius) { SphereRadius = InRadius; }

private:
	float SphereRadius = 0.0f;
};