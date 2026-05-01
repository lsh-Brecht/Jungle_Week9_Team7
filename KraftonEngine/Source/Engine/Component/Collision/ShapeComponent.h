#pragma once
#include "Component/PrimitiveComponent.h"

class UShapeComponent : public UPrimitiveComponent
{
public:
	const FColor& GetDebugShapeColor() const { return DebugShapeColor; }
	bool GetDrawDebugOnlyIfSelected() const { return bDrawDebugOnlyIfSelected; }

	void SetDebugShapeColor(const FColor& InColor) { DebugShapeColor = InColor; }
	void SetDrawDebugOnlyIfSelected(bool bInValue) { bDrawDebugOnlyIfSelected = bInValue; }

protected:
	FColor DebugShapeColor = FColor::Green(); //에디터 내에서 사용.
	bool bDrawDebugOnlyIfSelected = true;
};

