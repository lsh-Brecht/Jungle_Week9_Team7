#define IMGUI_DEFINE_MATH_OPERATORS
#include "Editor/UI/EditorBezierWidget.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

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

	int Bezier(const char* Label, float P[4])
	{
		static constexpr int Smoothness = 64;
		static constexpr float CurveWidth = 4.0f;
		static constexpr float LineWidth = 1.0f;
		static constexpr float GrabRadius = 6.0f;
		static constexpr float GrabBorder = 2.0f;

		const ImGuiStyle& Style = GetStyle();
		const ImGuiIO& IO = GetIO();
		ImDrawList* DrawList = GetWindowDrawList();
		ImGuiWindow* Window = GetCurrentWindow();
		if (Window->SkipItems)
		{
			return false;
		}

		int Changed = SliderFloat4(Label, P, 0.0f, 1.0f, "%.3f") ? 1 : 0;
		bool Hovered = IsItemActive() || IsItemHovered();
		Dummy(ImVec2(0.0f, 3.0f));

		const float AvailableWidth = GetContentRegionAvail().x;
		const float CanvasSize = ImMin(ImMax(AvailableWidth, 32.0f), 128.0f);
		const ImVec2 Canvas(CanvasSize, CanvasSize);

		PushID(Label);

		ImRect Bb(Window->DC.CursorPos, Window->DC.CursorPos + Canvas);
		ItemSize(Bb);
		const ImGuiID Id = Window->GetID("BezierCanvas");
		if (!ItemAdd(Bb, Id))
		{
			PopID();
			return Changed;
		}

		Hovered |= ItemHoverable(Bb, Id, ImGuiItemFlags_None);
		RenderFrame(Bb.Min, Bb.Max, GetColorU32(ImGuiCol_FrameBg, 1.0f), true, Style.FrameRounding);

		for (int i = 0; i <= 4; ++i)
		{
			const float X = Bb.Min.x + Canvas.x * (static_cast<float>(i) / 4.0f);
			DrawList->AddLine(
				ImVec2(X, Bb.Min.y),
				ImVec2(X, Bb.Max.y),
				GetColorU32(ImGuiCol_TextDisabled));
		}

		for (int i = 0; i <= 4; ++i)
		{
			const float Y = Bb.Min.y + Canvas.y * (static_cast<float>(i) / 4.0f);
			DrawList->AddLine(
				ImVec2(Bb.Min.x, Y),
				ImVec2(Bb.Max.x, Y),
				GetColorU32(ImGuiCol_TextDisabled));
		}

		ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
		ImVec2 Results[Smoothness + 1];
		BuildBezierTable<Smoothness>(Q, Results);

		for (int i = 0; i < 2; ++i)
		{
			const ImVec2 Pos = ImVec2(P[i * 2 + 0], 1.0f - P[i * 2 + 1]) * (Bb.Max - Bb.Min) + Bb.Min;
			SetCursorScreenPos(Pos - ImVec2(GrabRadius, GrabRadius));

			char ButtonId[16];
			std::snprintf(ButtonId, sizeof(ButtonId), "P%d", i);
			InvisibleButton(ButtonId, ImVec2(2.0f * GrabRadius, 2.0f * GrabRadius));

			if (IsItemActive() || IsItemHovered())
			{
				Hovered = true;
				SetTooltip("(%4.3f, %4.3f)", P[i * 2 + 0], P[i * 2 + 1]);
			}

			if (IsItemActive() && IsMouseDragging(0))
			{
				P[i * 2 + 0] += IO.MouseDelta.x / Canvas.x;
				P[i * 2 + 1] -= IO.MouseDelta.y / Canvas.y;
				Changed = true;
			}
		}

		const bool bRelaxClip = Hovered || Changed != 0;
		if (bRelaxClip)
		{
			DrawList->PushClipRectFullScreen();
		}

		ImColor CurveColor(GetStyle().Colors[ImGuiCol_PlotLines]);
		for (int i = 0; i < Smoothness; ++i)
		{
			ImVec2 P0 = { Results[i + 0].x, 1.0f - Results[i + 0].y };
			ImVec2 P1 = { Results[i + 1].x, 1.0f - Results[i + 1].y };
			ImVec2 R(P0.x * (Bb.Max.x - Bb.Min.x) + Bb.Min.x, P0.y * (Bb.Max.y - Bb.Min.y) + Bb.Min.y);
			ImVec2 S(P1.x * (Bb.Max.x - Bb.Min.x) + Bb.Min.x, P1.y * (Bb.Max.y - Bb.Min.y) + Bb.Min.y);
			DrawList->AddLine(R, S, CurveColor, CurveWidth);
		}

		const float Luma = Hovered ? 0.5f : 1.0f;
		ImVec4 Pink(1.00f, 0.00f, 0.75f, Luma);
		ImVec4 Cyan(0.00f, 0.75f, 1.00f, Luma);
		ImVec4 White(GetStyle().Colors[ImGuiCol_Text]);
		ImVec2 P1 = ImVec2(P[0], 1.0f - P[1]) * (Bb.Max - Bb.Min) + Bb.Min;
		ImVec2 P2 = ImVec2(P[2], 1.0f - P[3]) * (Bb.Max - Bb.Min) + Bb.Min;
		DrawList->AddLine(ImVec2(Bb.Min.x, Bb.Max.y), P1, ImColor(White), LineWidth);
		DrawList->AddLine(ImVec2(Bb.Max.x, Bb.Min.y), P2, ImColor(White), LineWidth);
		DrawList->AddCircleFilled(P1, GrabRadius, ImColor(White));
		DrawList->AddCircleFilled(P1, GrabRadius - GrabBorder, ImColor(Pink));
		DrawList->AddCircleFilled(P2, GrabRadius, ImColor(White));
		DrawList->AddCircleFilled(P2, GrabRadius - GrabBorder, ImColor(Cyan));

		if (bRelaxClip)
		{
			DrawList->PopClipRect();
		}

		SetCursorScreenPos(ImVec2(Bb.Min.x, Bb.Max.y + GrabRadius));
		PopID();

		return Changed;
	}

	void ShowBezierDemo()
	{
		{ static float V[] = { 0.000f, 0.000f, 1.000f, 1.000f }; Bezier("easeLinear", V); }
		{ static float V[] = { 0.470f, 0.000f, 0.745f, 0.715f }; Bezier("easeInSine", V); }
		{ static float V[] = { 0.390f, 0.575f, 0.565f, 1.000f }; Bezier("easeOutSine", V); }
		{ static float V[] = { 0.445f, 0.050f, 0.550f, 0.950f }; Bezier("easeInOutSine", V); }
		{ static float V[] = { 0.550f, 0.085f, 0.680f, 0.530f }; Bezier("easeInQuad", V); }
		{ static float V[] = { 0.250f, 0.460f, 0.450f, 0.940f }; Bezier("easeOutQuad", V); }
		{ static float V[] = { 0.455f, 0.030f, 0.515f, 0.955f }; Bezier("easeInOutQuad", V); }
		{ static float V[] = { 0.550f, 0.055f, 0.675f, 0.190f }; Bezier("easeInCubic", V); }
		{ static float V[] = { 0.215f, 0.610f, 0.355f, 1.000f }; Bezier("easeOutCubic", V); }
		{ static float V[] = { 0.645f, 0.045f, 0.355f, 1.000f }; Bezier("easeInOutCubic", V); }
		{ static float V[] = { 0.895f, 0.030f, 0.685f, 0.220f }; Bezier("easeInQuart", V); }
		{ static float V[] = { 0.165f, 0.840f, 0.440f, 1.000f }; Bezier("easeOutQuart", V); }
		{ static float V[] = { 0.770f, 0.000f, 0.175f, 1.000f }; Bezier("easeInOutQuart", V); }
		{ static float V[] = { 0.755f, 0.050f, 0.855f, 0.060f }; Bezier("easeInQuint", V); }
		{ static float V[] = { 0.230f, 1.000f, 0.320f, 1.000f }; Bezier("easeOutQuint", V); }
		{ static float V[] = { 0.860f, 0.000f, 0.070f, 1.000f }; Bezier("easeInOutQuint", V); }
		{ static float V[] = { 0.950f, 0.050f, 0.795f, 0.035f }; Bezier("easeInExpo", V); }
		{ static float V[] = { 0.190f, 1.000f, 0.220f, 1.000f }; Bezier("easeOutExpo", V); }
		{ static float V[] = { 1.000f, 0.000f, 0.000f, 1.000f }; Bezier("easeInOutExpo", V); }
		{ static float V[] = { 0.600f, 0.040f, 0.980f, 0.335f }; Bezier("easeInCirc", V); }
		{ static float V[] = { 0.075f, 0.820f, 0.165f, 1.000f }; Bezier("easeOutCirc", V); }
		{ static float V[] = { 0.785f, 0.135f, 0.150f, 0.860f }; Bezier("easeInOutCirc", V); }
		{ static float V[] = { 0.600f, -0.28f, 0.735f, 0.045f }; Bezier("easeInBack", V); }
		{ static float V[] = { 0.175f, 0.885f, 0.320f, 1.275f }; Bezier("easeOutBack", V); }
		{ static float V[] = { 0.680f, -0.55f, 0.265f, 1.550f }; Bezier("easeInOutBack", V); }
	}
}
