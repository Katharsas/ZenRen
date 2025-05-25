#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace ImGui {

	IMGUI_API void VerticalSpacing(float height = 4.f);
	IMGUI_API void BeginGroupPanel(const char* name, const ImVec2& size);
	IMGUI_API void EndGroupPanel();
	IMGUI_API void PushStyleColorDebugText();


	ImGuiDataType_ dataType(uint8_t __);
	ImGuiDataType_ dataType(int8_t __);
	ImGuiDataType_ dataType(uint16_t __);
	ImGuiDataType_ dataType(int16_t __);
	ImGuiDataType_ dataType(uint32_t __);
	ImGuiDataType_ dataType(int32_t __);
	ImGuiDataType_ dataType(float __);
	ImGuiDataType_ dataType(double __);
}

