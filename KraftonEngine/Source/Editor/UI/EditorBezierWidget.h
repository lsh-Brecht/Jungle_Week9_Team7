#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Curves/CurveFloat.h"
#include "ImGui/imgui.h"

namespace ImGui
{
	float BezierValue(float Dt01, const float P[4]);
	int Bezier(const char* Label, float P[4]);
	void ShowBezierDemo();
}

class FEditorBezierWidget : public FEditorWidget
{
public:
	void SetCurveAsset(UCurveFloat* InCurveAsset);
	virtual void Render(float DeltaTime) override;

private:
	void SyncDataFromAsset();
	void SyncDataToAsset();
	bool RenderToolbar();
	bool RenderCurveCanvas();
	bool RenderKeyTable();
	void RebuildCachedPoints();

	UCurveFloat* CurrentCurveAsset = nullptr;
	TArray<FRichCurveKey> CachedKeys;
	TArray<ImVec2> CachedCurvePoints;
	int32 SelectedKeyIndex = -1;
	bool bIsOpen = false;
};
