#include "ShapeComponent.h"

DEFINE_CLASS(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

void UShapeComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	DrawDebugShape(Scene, DebugShapeColor);
}
