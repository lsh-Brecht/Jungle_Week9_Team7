#pragma once

namespace ImGui
{
	float BezierValue(float Dt01, const float P[4]);
	int Bezier(const char* Label, float P[4]);
	void ShowBezierDemo();
}
