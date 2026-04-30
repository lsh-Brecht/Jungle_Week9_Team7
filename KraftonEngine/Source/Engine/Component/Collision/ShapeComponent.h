#pragma once
#include "Component/PrimitiveComponent.h"

class UShapeComponent : public UPrimitiveComponent
{
	FColor ShapeColor;
	bool bDrawOnlyIfSelected;
};

