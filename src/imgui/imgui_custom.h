#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace ImGui {

	IMGUI_API void VerticalSpacing(float height = 4.f);
	IMGUI_API void BeginGroupPanel(const char* name, const ImVec2& size);
	IMGUI_API void EndGroupPanel();
	IMGUI_API void PushStyleColorDebugText();
}

