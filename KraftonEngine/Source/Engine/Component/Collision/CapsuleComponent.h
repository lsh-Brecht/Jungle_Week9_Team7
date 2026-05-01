#pragma once
#include "ShapeComponent.h"
#include "CollisionTypes.h"

class UCapsuleComponent : public UShapeComponent
{
public:
	virtual ECollisionShapeType GetCollisionShapeType() const override
	{
		return ECollisionShapeType::Capsule;
	}

	virtual FBoundingBox GetWorldAABB() const override;

	float GetCapsuleRadius() const { return CapsuleRadius; }
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	void SetCapsuleRadius(float InRadius) { CapsuleRadius = InRadius; }
	void SetCapsuleHalfHeight(float InHalfHeight) { CapsuleHalfHeight = InHalfHeight; }

	void GetSegmentPoints(FVector& OutP0, FVector& OutP1) const;

private:
	float CapsuleHalfHeight = 0.0f;
	float CapsuleRadius = 0.0f;
};