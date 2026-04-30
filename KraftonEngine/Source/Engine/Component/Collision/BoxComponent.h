#pragma once
#include "ShapeComponent.h"
#include "Math/Vector.h"

class UBoxComponent : public UShapeComponent
{
	FVector BoxExtent;
};