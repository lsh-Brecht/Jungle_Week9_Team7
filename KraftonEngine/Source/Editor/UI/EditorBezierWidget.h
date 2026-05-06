#pragma once

#include "Editor/UI/EditorWidget.h"

namespace ImGui
{
	float BezierValue(float Dt01, const float P[4]);
	int Bezier(const char* Label, float P[4]);
	void ShowBezierDemo();
}

class FEditorBezierWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
};
