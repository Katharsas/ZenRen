#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"

namespace ImGui {
	IMGUI_API void BeginGroupPanel(const char* name, const ImVec2& size);
	IMGUI_API void EndGroupPanel();
}

