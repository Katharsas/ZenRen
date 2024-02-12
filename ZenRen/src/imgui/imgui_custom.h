#pragma once

#include "imgui/imgui.h"

namespace ImGui {
	IMGUI_API void BeginGroupPanel(const char* name, const ImVec2& size);
	IMGUI_API void EndGroupPanel();
}

