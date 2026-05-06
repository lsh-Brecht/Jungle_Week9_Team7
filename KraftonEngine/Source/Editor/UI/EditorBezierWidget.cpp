#define IMGUI_DEFINE_MATH_OPERATORS
#include "Editor/UI/EditorBezierWidget.h"

#include "Editor/Settings/EditorSettings.h"
#include "Core/Log.h"
#include "Math/MathUtils.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

// ImGui Bezier widget. @r-lyeh, public domain
// v1.02: add BezierValue(); comments; usage
// v1.01: out-of-bounds coord snapping; custom border width; spacing; cosmetics
// v1.00: initial version
//
// [ref] http://robnapier.net/faster-bezier
// [ref] http://easings.net/es#easeInSine

namespace
{
	template<int Steps>
	void BuildBezierTable(ImVec2 P[4], ImVec2 Results[Steps + 1])
	{
		static float Coefficients[(Steps + 1) * 4];
		static float* Table = nullptr;
		if (!Table)
		{
			Table = Coefficients;
			for (unsigned Step = 0; Step <= Steps; ++Step)
			{
				float T = static_cast<float>(Step) / static_cast<float>(Steps);
				Coefficients[Step * 4 + 0] = (1 - T) * (1 - T) * (1 - T);
				Coefficients[Step * 4 + 1] = 3 * (1 - T) * (1 - T) * T;
				Coefficients[Step * 4 + 2] = 3 * (1 - T) * T * T;
				Coefficients[Step * 4 + 3] = T * T * T;
			}
		}

		for (unsigned Step = 0; Step <= Steps; ++Step)
		{
			ImVec2 Point = {
				Table[Step * 4 + 0] * P[0].x + Table[Step * 4 + 1] * P[1].x + Table[Step * 4 + 2] * P[2].x + Table[Step * 4 + 3] * P[3].x,
				Table[Step * 4 + 0] * P[0].y + Table[Step * 4 + 1] * P[1].y + Table[Step * 4 + 2] * P[2].y + Table[Step * 4 + 3] * P[3].y
			};
			Results[Step] = Point;
		}
	}

	float EvalHermite(const FRichCurveKey& Key0, const FRichCurveKey& Key1, float Alpha)
	{
		const float T = FMath::Clamp01(Alpha);
		const float T2 = T * T;
		const float T3 = T2 * T;
		const float DT = Key1.Time - Key0.Time;

		const float H00 = 2.0f * T3 - 3.0f * T2 + 1.0f;
		const float H10 = T3 - 2.0f * T2 + T;
		const float H01 = -2.0f * T3 + 3.0f * T2;
		const float H11 = T3 - T2;

		return H00 * Key0.Value
			+ H10 * Key0.LeaveTangent * DT
			+ H01 * Key1.Value
			+ H11 * Key1.ArriveTangent * DT;
	}

	float EvaluateCachedKeys(const TArray<FRichCurveKey>& Keys, float InTime)
	{
		if (Keys.empty())
		{
			return 0.0f;
		}

		if (Keys.size() == 1 || InTime <= Keys.front().Time)
		{
			return Keys.front().Value;
		}

		if (InTime >= Keys.back().Time)
		{
			return Keys.back().Value;
		}

		auto Upper = std::upper_bound(Keys.begin(), Keys.end(), InTime,
			[](float Time, const FRichCurveKey& Key)
			{
				return Time < Key.Time;
			});

		const FRichCurveKey& Key1 = *Upper;
		const FRichCurveKey& Key0 = *(Upper - 1);
		const float DeltaTime = Key1.Time - Key0.Time;
		if (FMath::Abs(DeltaTime) <= FMath::Epsilon)
		{
			return Key1.Value;
		}

		const float Alpha = FMath::Clamp01((InTime - Key0.Time) / DeltaTime);
		switch (Key0.InterpMode)
		{
		case ERichCurveInterpMode::Constant:
			return Key0.Value;
		case ERichCurveInterpMode::Cubic:
			return EvalHermite(Key0, Key1, Alpha);
		case ERichCurveInterpMode::Linear:
		default:
			return FMath::Lerp(Key0.Value, Key1.Value, Alpha);
		}
	}

	const char* GetInterpModeName(ERichCurveInterpMode Mode)
	{
		switch (Mode)
		{
		case ERichCurveInterpMode::Constant: return "Constant";
		case ERichCurveInterpMode::Cubic: return "Cubic";
		case ERichCurveInterpMode::Linear:
		default: return "Linear";
		}
	}

	ERichCurveInterpMode GetInterpModeFromIndex(int32 Index)
	{
		switch (Index)
		{
		case 0: return ERichCurveInterpMode::Constant;
		case 2: return ERichCurveInterpMode::Cubic;
		case 1:
		default: return ERichCurveInterpMode::Linear;
		}
	}

	int32 GetInterpModeIndex(ERichCurveInterpMode Mode)
	{
		switch (Mode)
		{
		case ERichCurveInterpMode::Constant: return 0;
		case ERichCurveInterpMode::Cubic: return 2;
		case ERichCurveInterpMode::Linear:
		default: return 1;
		}
	}
}

void FEditorBezierWidget::SetCurveAsset(UCurveFloat* InCurveAsset)
{
	CurrentCurveAsset = InCurveAsset;
	bIsOpen = CurrentCurveAsset != nullptr;
	FEditorSettings::Get().UI.bBezier = bIsOpen;

	if (CurrentCurveAsset)
	{
		SyncDataFromAsset();
	}
	else
	{
		CachedKeys.clear();
		CachedCurvePoints.clear();
		SelectedKeyIndex = -1;
	}
}

void FEditorBezierWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	FEditorSettings& Settings = FEditorSettings::Get();
	bool bWindowOpen = Settings.UI.bBezier || bIsOpen;
	if (!bWindowOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(640.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Curve Editor", &bWindowOpen))
	{
		if (!CurrentCurveAsset)
		{
			ImGui::TextDisabled("No UCurveFloat asset selected.");
		}
		else
		{
			bool bModified = RenderToolbar();
			ImGui::Separator();
			bModified |= RenderCurveCanvas();
			ImGui::Separator();
			bModified |= RenderKeyTable();

			if (bModified)
			{
				SyncDataToAsset();
			}
		}
	}
	ImGui::End();

	bIsOpen = bWindowOpen;
	Settings.UI.bBezier = bWindowOpen;
}

void FEditorBezierWidget::SyncDataFromAsset()
{
	CachedKeys.clear();
	CachedCurvePoints.clear();
	SelectedKeyIndex = -1;

	if (!CurrentCurveAsset)
	{
		return;
	}

	CachedKeys = CurrentCurveAsset->GetKeys();
	RebuildCachedPoints();
}

void FEditorBezierWidget::SyncDataToAsset()
{
	if (!CurrentCurveAsset)
	{
		return;
	}

	float SelectedTime = 0.0f;
	const bool bHasSelectedKey = SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(CachedKeys.size());
	if (bHasSelectedKey)
	{
		SelectedTime = CachedKeys[SelectedKeyIndex].Time;
	}

	CurrentCurveAsset->SetKeys(CachedKeys);
	CachedKeys = CurrentCurveAsset->GetKeys();
	RebuildCachedPoints();

	if (bHasSelectedKey)
	{
		SelectedKeyIndex = CurrentCurveAsset->FindKeyIndex(SelectedTime);
	}
}

bool FEditorBezierWidget::RenderToolbar()
{
	bool bModified = false;

	const FString& CurveName = CurrentCurveAsset->GetCurveName();
	const FString& SourcePath = CurrentCurveAsset->GetSourceFilePath();
	ImGui::Text("Asset: %s", CurveName.empty() ? "(Unnamed Curve)" : CurveName.c_str());
	ImGui::TextDisabled("%s", SourcePath.empty() ? "(No source file)" : SourcePath.c_str());

	if (SourcePath.empty())
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Save"))
	{
		SyncDataToAsset();
		if (!CurrentCurveAsset->SaveToFile(SourcePath))
		{
			UE_LOG("Failed to save curve asset: %s", SourcePath.c_str());
		}
	}
	if (SourcePath.empty())
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		SyncDataFromAsset();
	}

	ImGui::SameLine();
	if (ImGui::Button("Add Key"))
	{
		const float NewTime = CachedKeys.empty() ? 0.0f : CachedKeys.back().Time + 1.0f;
		const float NewValue = CachedKeys.empty() ? 0.0f : CachedKeys.back().Value;
		CachedKeys.emplace_back(NewTime, NewValue, ERichCurveInterpMode::Linear);
		SelectedKeyIndex = static_cast<int32>(CachedKeys.size()) - 1;
		RebuildCachedPoints();
		bModified = true;
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset Linear"))
	{
		CachedKeys.clear();
		CachedKeys.emplace_back(0.0f, 0.0f, ERichCurveInterpMode::Linear);
		CachedKeys.emplace_back(1.0f, 1.0f, ERichCurveInterpMode::Linear);
		SelectedKeyIndex = 0;
		RebuildCachedPoints();
		bModified = true;
	}

	return bModified;
}

bool FEditorBezierWidget::RenderCurveCanvas()
{
	const ImVec2 CanvasSize(ImGui::GetContentRegionAvail().x, 260.0f);
	if (CanvasSize.x <= 0.0f)
	{
		return false;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);

	ImGui::InvisibleButton("##CurveCanvas", CanvasSize);
	const bool bCanvasHovered = ImGui::IsItemHovered();
	const bool bCanvasActive = ImGui::IsItemActive();

	DrawList->AddRectFilled(CanvasMin, CanvasMax, ImGui::GetColorU32(ImGuiCol_FrameBg));
	DrawList->AddRect(CanvasMin, CanvasMax, ImGui::GetColorU32(ImGuiCol_Border));

	if (CachedKeys.empty())
	{
		DrawList->AddText(ImVec2(CanvasMin.x + 12.0f, CanvasMin.y + 12.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "No keys");
		return false;
	}

	float MinTime = CachedKeys.front().Time;
	float MaxTime = CachedKeys.back().Time;
	float MinValue = CachedKeys.front().Value;
	float MaxValue = CachedKeys.front().Value;
	for (const FRichCurveKey& Key : CachedKeys)
	{
		MinTime = FMath::Min(MinTime, Key.Time);
		MaxTime = FMath::Max(MaxTime, Key.Time);
		MinValue = FMath::Min(MinValue, Key.Value);
		MaxValue = FMath::Max(MaxValue, Key.Value);
	}

	if (FMath::Abs(MaxTime - MinTime) <= FMath::Epsilon)
	{
		MinTime -= 0.5f;
		MaxTime += 0.5f;
	}
	if (FMath::Abs(MaxValue - MinValue) <= FMath::Epsilon)
	{
		MinValue -= 0.5f;
		MaxValue += 0.5f;
	}

	const float ValuePadding = (MaxValue - MinValue) * 0.15f;
	MinValue -= ValuePadding;
	MaxValue += ValuePadding;

	auto ToScreen = [&](float Time, float Value)
	{
		const float X = (Time - MinTime) / (MaxTime - MinTime);
		const float Y = (Value - MinValue) / (MaxValue - MinValue);
		return ImVec2(
			CanvasMin.x + X * CanvasSize.x,
			CanvasMax.y - Y * CanvasSize.y);
	};

	auto ToCurve = [&](const ImVec2& Screen)
	{
		const float X = FMath::Clamp01((Screen.x - CanvasMin.x) / CanvasSize.x);
		const float Y = FMath::Clamp01((CanvasMax.y - Screen.y) / CanvasSize.y);
		return ImVec2(
			MinTime + X * (MaxTime - MinTime),
			MinValue + Y * (MaxValue - MinValue));
	};

	const ImU32 GridColor = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.35f);
	for (int32 i = 1; i < 4; ++i)
	{
		const float X = CanvasMin.x + CanvasSize.x * (static_cast<float>(i) / 4.0f);
		const float Y = CanvasMin.y + CanvasSize.y * (static_cast<float>(i) / 4.0f);
		DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), GridColor);
		DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), GridColor);
	}

	const int32 SampleCount = 96;
	ImVec2 Previous = ToScreen(MinTime, EvaluateCachedKeys(CachedKeys, MinTime));
	for (int32 i = 1; i <= SampleCount; ++i)
	{
		const float Alpha = static_cast<float>(i) / static_cast<float>(SampleCount);
		const float Time = FMath::Lerp(MinTime, MaxTime, Alpha);
		const ImVec2 Current = ToScreen(Time, EvaluateCachedKeys(CachedKeys, Time));
		DrawList->AddLine(Previous, Current, ImGui::GetColorU32(ImGuiCol_PlotLines), 2.0f);
		Previous = Current;
	}

	const float GrabRadius = 6.0f;
	int32 HoveredKeyIndex = -1;
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	for (int32 Index = 0; Index < static_cast<int32>(CachedKeys.size()); ++Index)
	{
		const ImVec2 Point = ToScreen(CachedKeys[Index].Time, CachedKeys[Index].Value);
		const float Dx = Point.x - MousePos.x;
		const float Dy = Point.y - MousePos.y;
		if (Dx * Dx + Dy * Dy <= GrabRadius * GrabRadius * 4.0f)
		{
			HoveredKeyIndex = Index;
		}

		const bool bSelected = Index == SelectedKeyIndex;
		DrawList->AddCircleFilled(Point, bSelected ? GrabRadius + 2.0f : GrabRadius,
			bSelected ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
		DrawList->AddCircle(Point, GrabRadius + 2.0f, ImGui::GetColorU32(ImGuiCol_Text));
	}

	if (bCanvasHovered && HoveredKeyIndex >= 0)
	{
		ImGui::SetTooltip("Key %d: %.3f, %.3f", HoveredKeyIndex, CachedKeys[HoveredKeyIndex].Time, CachedKeys[HoveredKeyIndex].Value);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedKeyIndex = HoveredKeyIndex;
		}
	}

	bool bModified = false;
	if (bCanvasActive && SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(CachedKeys.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		ImVec2 CurvePoint = ToCurve(MousePos);
		if (SelectedKeyIndex > 0)
		{
			CurvePoint.x = FMath::Max(CurvePoint.x, CachedKeys[SelectedKeyIndex - 1].Time + 1.0e-3f);
		}
		if (SelectedKeyIndex + 1 < static_cast<int32>(CachedKeys.size()))
		{
			CurvePoint.x = FMath::Min(CurvePoint.x, CachedKeys[SelectedKeyIndex + 1].Time - 1.0e-3f);
		}

		CachedKeys[SelectedKeyIndex].Time = CurvePoint.x;
		CachedKeys[SelectedKeyIndex].Value = CurvePoint.y;
		RebuildCachedPoints();
		bModified = true;
	}

	return bModified;
}

bool FEditorBezierWidget::RenderKeyTable()
{
	if (CachedKeys.empty())
	{
		return false;
	}

	bool bModified = false;
	bool bDeleteSelected = false;

	if (ImGui::BeginTable("CurveKeys", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 48.0f);
		ImGui::TableSetupColumn("Time");
		ImGui::TableSetupColumn("Value");
		ImGui::TableSetupColumn("Arrive");
		ImGui::TableSetupColumn("Leave");
		ImGui::TableSetupColumn("Mode");
		ImGui::TableHeadersRow();

		for (int32 Index = 0; Index < static_cast<int32>(CachedKeys.size()); ++Index)
		{
			FRichCurveKey& Key = CachedKeys[Index];
			ImGui::PushID(Index);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			const bool bSelected = SelectedKeyIndex == Index;
			char Label[32];
			std::snprintf(Label, sizeof(Label), "%d", Index);
			if (ImGui::Selectable(Label, bSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectedKeyIndex = Index;
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Time", &Key.Time, 0.01f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				SelectedKeyIndex = Index;
				bModified = true;
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Value", &Key.Value, 0.01f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				SelectedKeyIndex = Index;
				bModified = true;
			}

			ImGui::TableSetColumnIndex(3);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Arrive", &Key.ArriveTangent, 0.01f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				SelectedKeyIndex = Index;
				bModified = true;
			}

			ImGui::TableSetColumnIndex(4);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Leave", &Key.LeaveTangent, 0.01f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				SelectedKeyIndex = Index;
				bModified = true;
			}

			ImGui::TableSetColumnIndex(5);
			int32 ModeIndex = GetInterpModeIndex(Key.InterpMode);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::Combo("##Mode", &ModeIndex, "Constant\0Linear\0Cubic\0"))
			{
				Key.InterpMode = GetInterpModeFromIndex(ModeIndex);
				SelectedKeyIndex = Index;
				bModified = true;
			}

			if (ImGui::BeginPopupContextItem("KeyContext"))
			{
				if (ImGui::MenuItem("Delete Key"))
				{
					SelectedKeyIndex = Index;
					bDeleteSelected = true;
				}
				ImGui::TextDisabled("%s", GetInterpModeName(Key.InterpMode));
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (SelectedKeyIndex >= 0 && ImGui::Button("Delete Selected Key"))
	{
		bDeleteSelected = true;
	}

	if (bDeleteSelected && SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(CachedKeys.size()))
	{
		CachedKeys.erase(CachedKeys.begin() + SelectedKeyIndex);
		SelectedKeyIndex = FMath::Min(SelectedKeyIndex, static_cast<int32>(CachedKeys.size()) - 1);
		bModified = true;
	}

	if (bModified)
	{
		std::stable_sort(CachedKeys.begin(), CachedKeys.end(),
			[](const FRichCurveKey& A, const FRichCurveKey& B)
			{
				return A.Time < B.Time;
			});
		RebuildCachedPoints();
	}

	return bModified;
}

void FEditorBezierWidget::RebuildCachedPoints()
{
	CachedCurvePoints.clear();
	for (const FRichCurveKey& Key : CachedKeys)
	{
		CachedCurvePoints.emplace_back(Key.Time, Key.Value);
	}
}

namespace ImGui
{
	float BezierValue(float Dt01, const float P[4])
	{
		static constexpr int Steps = 256;
		ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
		ImVec2 Results[Steps + 1];
		BuildBezierTable<Steps>(Q, Results);

		const float ClampedDt = Dt01 < 0.0f ? 0.0f : (Dt01 > 1.0f ? 1.0f : Dt01);
		return Results[static_cast<int>(ClampedDt * static_cast<float>(Steps))].y;
	}

	int Bezier(const char* label, float P[5]) {
		// visuals
		enum { SMOOTHNESS = 64 }; // curve smoothness: the higher number of segments, the smoother curve
		enum { CURVE_WIDTH = 4 }; // main curved line width
		enum { LINE_WIDTH = 1 }; // handlers: small lines width
		enum { GRAB_RADIUS = 8 }; // handlers: circle radius
		enum { GRAB_BORDER = 2 }; // handlers: circle border width
		enum { AREA_CONSTRAINED = true }; // should grabbers be constrained to grid area?
		enum { AREA_WIDTH = 128 }; // area width in pixels. 0 for adaptive size (will use max avail width)

		// curve presets
		static struct { const char* name; float points[4]; } presets[] = {
			{ "Linear", 0.000f, 0.000f, 1.000f, 1.000f },

			{ "In Sine", 0.470f, 0.000f, 0.745f, 0.715f },
			{ "In Quad", 0.550f, 0.085f, 0.680f, 0.530f },
			{ "In Cubic", 0.550f, 0.055f, 0.675f, 0.190f },
			{ "In Quart", 0.895f, 0.030f, 0.685f, 0.220f },
			{ "In Quint", 0.755f, 0.050f, 0.855f, 0.060f },
			{ "In Expo", 0.950f, 0.050f, 0.795f, 0.035f },
			{ "In Circ", 0.600f, 0.040f, 0.980f, 0.335f },
			{ "In Back", 0.600f, -0.28f, 0.735f, 0.045f },

			{ "Out Sine", 0.390f, 0.575f, 0.565f, 1.000f },
			{ "Out Quad", 0.250f, 0.460f, 0.450f, 0.940f },
			{ "Out Cubic", 0.215f, 0.610f, 0.355f, 1.000f },
			{ "Out Quart", 0.165f, 0.840f, 0.440f, 1.000f },
			{ "Out Quint", 0.230f, 1.000f, 0.320f, 1.000f },
			{ "Out Expo", 0.190f, 1.000f, 0.220f, 1.000f },
			{ "Out Circ", 0.075f, 0.820f, 0.165f, 1.000f },
			{ "Out Back", 0.175f, 0.885f, 0.320f, 1.275f },

			{ "InOut Sine", 0.445f, 0.050f, 0.550f, 0.950f },
			{ "InOut Quad", 0.455f, 0.030f, 0.515f, 0.955f },
			{ "InOut Cubic", 0.645f, 0.045f, 0.355f, 1.000f },
			{ "InOut Quart", 0.770f, 0.000f, 0.175f, 1.000f },
			{ "InOut Quint", 0.860f, 0.000f, 0.070f, 1.000f },
			{ "InOut Expo", 1.000f, 0.000f, 0.000f, 1.000f },
			{ "InOut Circ", 0.785f, 0.135f, 0.150f, 0.860f },
			{ "InOut Back", 0.680f, -0.55f, 0.265f, 1.550f },

			// easeInElastic: not a bezier
			// easeOutElastic: not a bezier
			// easeInOutElastic: not a bezier
			// easeInBounce: not a bezier
			// easeOutBounce: not a bezier
			// easeInOutBounce: not a bezier
		};


		// preset selector

		bool reload = 0;
		ImGui::PushID(label);
		if (ImGui::ArrowButton("##lt", ImGuiDir_Left)) { // ImGui::ArrowButton(ImGui::GetCurrentWindow()->GetID("##lt"), ImGuiDir_Left, ImVec2(0, 0), 0)
			if (--P[4] >= 0) reload = 1; else ++P[4];
		}
		ImGui::SameLine();

		if (ImGui::Button("Presets")) {
			ImGui::OpenPopup("!Presets");
		}
		if (ImGui::BeginPopup("!Presets")) {
			for (int i = 0; i < IM_ARRAYSIZE(presets); ++i) {
				if (i == 1 || i == 9 || i == 17) ImGui::Separator();
				if (ImGui::MenuItem(presets[i].name, NULL, P[4] == i)) {
					P[4] = static_cast<float>(i);
					reload = 1;
				}
			}
			ImGui::EndPopup();
		}
		ImGui::SameLine();

		if (ImGui::ArrowButton("##rt", ImGuiDir_Right)) { // ImGui::ArrowButton(ImGui::GetCurrentWindow()->GetID("##rt"), ImGuiDir_Right, ImVec2(0, 0), 0)
			if (++P[4] < IM_ARRAYSIZE(presets)) reload = 1; else --P[4];
		}
		ImGui::SameLine();
		ImGui::PopID();

		if (reload) {
			memcpy(P, presets[(int)P[4]].points, sizeof(float) * 4);
		}

		// bezier widget

		const ImGuiStyle& Style = GetStyle();
		const ImGuiIO& IO = GetIO();
		ImDrawList* DrawList = GetWindowDrawList();
		ImGuiWindow* Window = GetCurrentWindow();
		if (Window->SkipItems)
			return false;

		// header and spacing
		int changed = SliderFloat4(label, P, 0.0f, 1.0f, "%.3f");
		int hovered = IsItemActive() || IsItemHovered(); // IsItemDragged() ?
		Dummy(ImVec2(0, 3));

		// prepare canvas
		const float avail = GetContentRegionAvail().x;
		const float dim = AREA_WIDTH > 0 ? AREA_WIDTH : avail;
		ImVec2 Canvas(dim, dim);

		ImRect bb(Window->DC.CursorPos, Window->DC.CursorPos + Canvas);
		ItemSize(bb);
		if (!ItemAdd(bb, NULL))
			return changed;

		const ImGuiID id = Window->GetID(label);
		hovered |= 0 != ItemHoverable(ImRect(bb.Min, bb.Min + ImVec2(avail, dim)), id, 0);

		RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, Style.FrameRounding);

		// background grid
		for (int i = 0; i <= 4; ++i) {
			const float X = Canvas.x * (static_cast<float>(i) / 4.0f);
			DrawList->AddLine(
				ImVec2(bb.Min.x + X, bb.Min.y),
				ImVec2(bb.Min.x + X, bb.Max.y),
				GetColorU32(ImGuiCol_TextDisabled));
		}
		for (int i = 0; i <= 4; ++i) {
			const float Y = Canvas.y * (static_cast<float>(i) / 4.0f);
			DrawList->AddLine(
				ImVec2(bb.Min.x, bb.Min.y + Y),
				ImVec2(bb.Max.x, bb.Min.y + Y),
				GetColorU32(ImGuiCol_TextDisabled));
		}

		// eval curve
		ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
		ImVec2 results[SMOOTHNESS + 1];
		BuildBezierTable<SMOOTHNESS>(Q, results);

		// control points: 2 lines and 2 circles
		{
			// handle grabbers
			ImVec2 mouse = GetIO().MousePos, pos[2];
			float distance[2];

			for (int i = 0; i < 2; ++i) {
				pos[i] = ImVec2(P[i * 2 + 0], 1 - P[i * 2 + 1]) * (bb.Max - bb.Min) + bb.Min;
				distance[i] = (pos[i].x - mouse.x) * (pos[i].x - mouse.x) + (pos[i].y - mouse.y) * (pos[i].y - mouse.y);
			}

			int selected = distance[0] < distance[1] ? 0 : 1;
			if (distance[selected] < (4 * GRAB_RADIUS * 4 * GRAB_RADIUS))
			{
				SetTooltip("(%4.3f, %4.3f)", P[selected * 2 + 0], P[selected * 2 + 1]);

				if (/*hovered &&*/ (IsMouseClicked(0) || IsMouseDragging(0))) {
					float& px = (P[selected * 2 + 0] += GetIO().MouseDelta.x / Canvas.x);
					float& py = (P[selected * 2 + 1] -= GetIO().MouseDelta.y / Canvas.y);

					if (AREA_CONSTRAINED) {
						px = (px < 0 ? 0 : (px > 1 ? 1 : px));
						py = (py < 0 ? 0 : (py > 1 ? 1 : py));
					}

					changed = true;
				}
			}
		}

		// if (hovered || changed) DrawList->PushClipRectFullScreen();

		// draw curve
		{
			ImColor color(GetStyle().Colors[ImGuiCol_PlotLines]);
			for (int i = 0; i < SMOOTHNESS; ++i) {
				ImVec2 p = { results[i + 0].x, 1 - results[i + 0].y };
				ImVec2 q = { results[i + 1].x, 1 - results[i + 1].y };
				ImVec2 r(p.x * (bb.Max.x - bb.Min.x) + bb.Min.x, p.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
				ImVec2 s(q.x * (bb.Max.x - bb.Min.x) + bb.Min.x, q.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
				DrawList->AddLine(r, s, color, CURVE_WIDTH);
			}
		}

		// draw preview (cycles every 1s)
		static clock_t epoch = clock();
		ImVec4 white(GetStyle().Colors[ImGuiCol_Text]);
		for (int i = 0; i < 3; ++i) {
			double now = ((clock() - epoch) / (double)CLOCKS_PER_SEC);
			float delta = ((int)(now * 1000) % 1000) / 1000.f; delta += i / 3.f; if (delta > 1) delta -= 1;
			int idx = static_cast<int>(delta * static_cast<float>(SMOOTHNESS));
			float evalx = results[idx].x; // 
			float evaly = results[idx].y; // ImGui::BezierValue( delta, P );
			ImVec2 p0 = ImVec2(evalx, 1 - 0) * (bb.Max - bb.Min) + bb.Min;
			ImVec2 p1 = ImVec2(0, 1 - evaly) * (bb.Max - bb.Min) + bb.Min;
			ImVec2 p2 = ImVec2(evalx, 1 - evaly) * (bb.Max - bb.Min) + bb.Min;
			DrawList->AddCircleFilled(p0, GRAB_RADIUS / 2, ImColor(white));
			DrawList->AddCircleFilled(p1, GRAB_RADIUS / 2, ImColor(white));
			DrawList->AddCircleFilled(p2, GRAB_RADIUS / 2, ImColor(white));
		}

		// draw lines and grabbers
		float luma = IsItemActive() || IsItemHovered() ? 0.5f : 1.0f;
		ImVec4 pink(1.00f, 0.00f, 0.75f, luma), cyan(0.00f, 0.75f, 1.00f, luma);
		ImVec2 p1 = ImVec2(P[0], 1 - P[1]) * (bb.Max - bb.Min) + bb.Min;
		ImVec2 p2 = ImVec2(P[2], 1 - P[3]) * (bb.Max - bb.Min) + bb.Min;
		DrawList->AddLine(ImVec2(bb.Min.x, bb.Max.y), p1, ImColor(white), LINE_WIDTH);
		DrawList->AddLine(ImVec2(bb.Max.x, bb.Min.y), p2, ImColor(white), LINE_WIDTH);
		DrawList->AddCircleFilled(p1, GRAB_RADIUS, ImColor(white));
		DrawList->AddCircleFilled(p1, GRAB_RADIUS - GRAB_BORDER, ImColor(pink));
		DrawList->AddCircleFilled(p2, GRAB_RADIUS, ImColor(white));
		DrawList->AddCircleFilled(p2, GRAB_RADIUS - GRAB_BORDER, ImColor(cyan));

		// if (hovered || changed) DrawList->PopClipRect();

		return changed;
	}

	void ShowBezierDemo()
	{
		{ static float V[] = { 0.000f, 0.000f, 1.000f, 1.000f }; Bezier("easeLinear", V); }
		//{ static float V[] = { 0.470f, 0.000f, 0.745f, 0.715f }; Bezier("easeInSine", V); }
		//{ static float V[] = { 0.390f, 0.575f, 0.565f, 1.000f }; Bezier("easeOutSine", V); }
		//{ static float V[] = { 0.445f, 0.050f, 0.550f, 0.950f }; Bezier("easeInOutSine", V); }
		//{ static float V[] = { 0.550f, 0.085f, 0.680f, 0.530f }; Bezier("easeInQuad", V); }
		//{ static float V[] = { 0.250f, 0.460f, 0.450f, 0.940f }; Bezier("easeOutQuad", V); }
		//{ static float V[] = { 0.455f, 0.030f, 0.515f, 0.955f }; Bezier("easeInOutQuad", V); }
		//{ static float V[] = { 0.550f, 0.055f, 0.675f, 0.190f }; Bezier("easeInCubic", V); }
		//{ static float V[] = { 0.215f, 0.610f, 0.355f, 1.000f }; Bezier("easeOutCubic", V); }
		//{ static float V[] = { 0.645f, 0.045f, 0.355f, 1.000f }; Bezier("easeInOutCubic", V); }
		//{ static float V[] = { 0.895f, 0.030f, 0.685f, 0.220f }; Bezier("easeInQuart", V); }
		//{ static float V[] = { 0.165f, 0.840f, 0.440f, 1.000f }; Bezier("easeOutQuart", V); }
		//{ static float V[] = { 0.770f, 0.000f, 0.175f, 1.000f }; Bezier("easeInOutQuart", V); }
		//{ static float V[] = { 0.755f, 0.050f, 0.855f, 0.060f }; Bezier("easeInQuint", V); }
		//{ static float V[] = { 0.230f, 1.000f, 0.320f, 1.000f }; Bezier("easeOutQuint", V); }
		//{ static float V[] = { 0.860f, 0.000f, 0.070f, 1.000f }; Bezier("easeInOutQuint", V); }
		//{ static float V[] = { 0.950f, 0.050f, 0.795f, 0.035f }; Bezier("easeInExpo", V); }
		//{ static float V[] = { 0.190f, 1.000f, 0.220f, 1.000f }; Bezier("easeOutExpo", V); }
		//{ static float V[] = { 1.000f, 0.000f, 0.000f, 1.000f }; Bezier("easeInOutExpo", V); }
		//{ static float V[] = { 0.600f, 0.040f, 0.980f, 0.335f }; Bezier("easeInCirc", V); }
		//{ static float V[] = { 0.075f, 0.820f, 0.165f, 1.000f }; Bezier("easeOutCirc", V); }
		//{ static float V[] = { 0.785f, 0.135f, 0.150f, 0.860f }; Bezier("easeInOutCirc", V); }
		//{ static float V[] = { 0.600f, -0.28f, 0.735f, 0.045f }; Bezier("easeInBack", V); }
		//{ static float V[] = { 0.175f, 0.885f, 0.320f, 1.275f }; Bezier("easeOutBack", V); }
		//{ static float V[] = { 0.680f, -0.55f, 0.265f, 1.550f }; Bezier("easeInOutBack", V); }
	}
}
