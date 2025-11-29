#include "stdafx.h"
#include "WorldSettingsGui.h"

#include "render/pass/world/WorldGrid.h"

#include "render/Gui.h"
#include "render/GuiHelper.h"

#include "imgui/imgui_custom.h"

#include <imgui.h>
#include <magic_enum.hpp>

namespace render::pass::world::gui
{
	auto lodModeComboState = render::gui::comboStateFromEnum<LodMode>();

	std::function<void()> notifyTimeOfDayChange;
	std::function<std::pair<GridPos, GridPos>()> getGridMinMaxPos;

	void advanced(WorldSettings& settings, const std::function<void()> gui)
	{
		ImGui::PushStyleColorDebugText();
		if (settings.showAdvancedSettings) {
			gui();
		}
		ImGui::PopStyleColor();
	}

	void init(
		WorldSettings& worldSettings,
		uint32_t& maxDrawCalls,
		const std::function<void()>& notifyTimeOfDayChangeParam,
		const std::function<std::pair<GridPos, GridPos>()>& getGridMinMaxPosParam)
	{
		notifyTimeOfDayChange = notifyTimeOfDayChangeParam;
		getGridMinMaxPos = getGridMinMaxPosParam;

		render::gui::addSettings("World", {
			[&]() -> void {
				// Lables starting with ## are hidden
				bool changed = ImGui::DragFloat("##TimeOfDay", &worldSettings.timeOfDay, .002f, 0, 0, "%.3f TimeOfDay");
				if (changed) {
					notifyTimeOfDayChange();
				}
				ImGui::SliderFloat("##TimeOfDayChangeSpeed", &worldSettings.timeOfDayChangeSpeed, 0, 1, "%.3f Time Speed",
					ImGuiSliderFlags_Logarithmic);			

				advanced(worldSettings, [&]() -> void {
					ImGui::VerticalSpacing();
					ImGui::Checkbox("Draw World", &worldSettings.drawWorld);
					ImGui::Checkbox("Draw VOBs/MOBs", &worldSettings.drawStaticObjects);
					ImGui::Checkbox("Draw Sky", &worldSettings.drawSky);
					ImGui::VerticalSpacing();
					ImGui::Checkbox("Chunked Rendering", &worldSettings.chunkedRendering);

					ImGui::VerticalSpacing();
					ImGui::BeginDisabled(!worldSettings.chunkedRendering);

					ImGui::Checkbox("Close chunks first", &worldSettings.renderCloseFirst);
					ImGui::SliderFloat("##CloseRadius", &worldSettings.renderCloseRadius, 0, 400, "%.0f Close Radius",
						ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
				});

				ImGui::VerticalSpacing();
				ImGui::Checkbox("LOD Enabled", &worldSettings.enableLod);
				ImGui::BeginDisabled(!worldSettings.enableLod);
				ImGui::SliderFloat("##LodCrossover", &worldSettings.lodRadius, 0, 1000, "%.0f LOD Radius", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);

				advanced(worldSettings, [&]() -> void {
					ImGui::PushItemWidth(120);
					render::gui::comboEnum("LOD Display", lodModeComboState, worldSettings.lodDisplayMode);
					ImGui::PopItemWidth();

					ImGui::Checkbox("LOD Per Pixel", &worldSettings.enablePerPixelLod);
					ImGui::BeginDisabled(!worldSettings.enablePerPixelLod);
					ImGui::Checkbox("LOD Dithering", &worldSettings.enableLodDithering);
					ImGui::BeginDisabled(!worldSettings.enableLodDithering);
					ImGui::SliderFloat("##LodCrossoverWidth", &worldSettings.lodRadiusDitherWidth, 0, 100, "%.0f LOD Fade Range");
					ImGui::EndDisabled();
					ImGui::EndDisabled();
					ImGui::EndDisabled();

					ImGui::VerticalSpacing();
					ImGui::Checkbox("Enable Frustum Culling", &worldSettings.enableFrustumCulling);
					ImGui::Checkbox("Update Frustum Culling", &worldSettings.updateFrustumCulling);
					ImGui::EndDisabled();

					ImGui::VerticalSpacing();
					ImGui::Checkbox("Debug Shader", &worldSettings.debugWorldShaderEnabled);
					ImGui::Checkbox("Debug Single Draw", &worldSettings.debugSingleDrawEnabled);
					ImGui::BeginDisabled(!worldSettings.debugSingleDrawEnabled);
					ImGui::SliderInt("Debug Single Draw Index", &worldSettings.debugSingleDrawIndex, 0, maxDrawCalls - 1);
				});
				ImGui::EndDisabled();
			}
		});

		render::gui::addSettings("World Draw Filter", {
			[&]() -> void {
				advanced(worldSettings, [&]() -> void {
					ImGui::BeginDisabled(!worldSettings.chunkedRendering);

					ImGui::Checkbox("Filter Chunks X", &worldSettings.chunkFilterXEnabled);
					ImGui::Checkbox("Filter Chunks Y", &worldSettings.chunkFilterYEnabled);

					auto [min, max] = getGridMinMaxPos();

					ImGui::BeginDisabled(!worldSettings.chunkFilterXEnabled);
					ImGui::SliderScalar("ChunkIndex X", ImGui::dataType(min.x), &worldSettings.chunkFilterX, &min.x, &max.x);
					ImGui::EndDisabled();
					ImGui::BeginDisabled(!worldSettings.chunkFilterYEnabled);
					ImGui::SliderScalar("ChunkIndex Y", ImGui::dataType(min.y), &worldSettings.chunkFilterY, &min.y, &max.y);
					ImGui::EndDisabled();

					ImGui::EndDisabled();
				});
			}
		});
	}
}