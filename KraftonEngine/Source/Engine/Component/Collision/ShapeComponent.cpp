#include "ShapeComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_LUA_QUERY_COMPONENT(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

void UShapeComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	DrawDebugShape(Scene, DebugShapeColor);
}

void UShapeComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << DebugShapeColor;
	Ar << bDrawDebugOnlyIfSelected;
}
