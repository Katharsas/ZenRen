#include "stdafx.h"
#include "SettingsGui.h"

#include "Gui.h"

#include "imgui.h"
#include "imgui/imgui_custom.h"
#include "magic_enum.hpp"

namespace render::gui::settings {

	// TODO we should have checkbox toggles for each light type so we can add them up however we want
	constexpr auto shaderModeItems = magic_enum::enum_names<ShaderMode>();
	std::string_view shaderModeSelected = shaderModeItems[0];

	const std::array<std::string, 5> filterSettingsItems = { "Trilinear", "AF  x2", "AF  x4", "AF  x8", "AF x16" };
	std::string filterSettingsSelected = filterSettingsItems[4];

	const std::array<std::string, 4> msaaSettingsItems = { "None", "x2", "x4", "x8" };
	std::string msaaSettingsSelected = msaaSettingsItems[2];

	void init(RenderSettings& settings)
	{
		addSettings("Renderer", {
			[&]() -> void {
				ImGui::PushItemWidth(constants().elementWidth);
				ImGui::SliderFloat("##ViewDistance", &settings.viewDistance, 1, 2000, "%.0f View Distance");

				ImGui::VerticalSpacing();
				ImGui::Checkbox("Fog", &settings.distanceFog);
				ImGui::SliderFloat("##FogStart", &settings.distanceFogStart, 1, 200, "%.0f Fog Start");
				ImGui::SliderFloat("##FogEnd", &settings.distanceFogEnd, 1, 2000, "%.0f Fog End");
				ImGui::SliderFloat("##FogSky", &settings.distanceFogSkyFactor, 1, 10, "%.2f Fog Sky");

				ImGui::VerticalSpacing();
				ImGui::PushStyleColorDebugText();
				
				const auto& items = shaderModeItems;
				std::string_view& selected = shaderModeSelected;

				ImGui::PushItemWidth(120);// TODO whats up with this??
				if (ImGui::BeginCombo("Shader Mode", std::string(selected).c_str()))
				{
					for (uint32_t n = 0; n < items.size(); n++)
					{
						auto current = items[n];
						bool isSelected = (selected == current);
						if (ImGui::Selectable(std::string(current).c_str(), isSelected)) {
							selected = items[n];
							settings.shader.mode = static_cast<ShaderMode>(n);
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ImGui::Checkbox("Wireframe Mode", &settings.wireframe);
				ImGui::Checkbox("Enable Alpha", &settings.outputAlpha);

				ImGui::VerticalSpacing();
				//ImGui::Checkbox("Depth Prepass", &settings.depthPrepass);
				ImGui::Checkbox("Opaque Passes", &settings.passesOpaque);
				ImGui::Checkbox("Blend Passes", &settings.passesBlend);

				ImGui::VerticalSpacing();
				ImGui::SliderFloat("##Debug1", &settings.debugFloat1, -1.f, 1.f, "%.2f Debug 1");
				ImGui::SliderFloat("##Debug2", &settings.debugFloat2, -1.f, 1.f, "%.2f Debug 2");
				
				ImGui::VerticalSpacing();
				ImGui::Checkbox("Reverse Z", &settings.reverseZ);

				ImGui::PopStyleColor();
				ImGui::PopItemWidth();
			}
		});

		addSettings("Resolution", {
			[&]() -> void {
				ImGui::PushItemWidth(constants().inputFloatWidth);
				ImGui::InputFloat("Resolution Scaling", &settings.resolutionScaling, 0, 0, "%.2f");
				ImGui::PopItemWidth();
				ImGui::PushStyleColorDebugText();
				ImGui::Checkbox("Smooth Scaling", &settings.resolutionUpscaleSmooth);
				//ImGui::Checkbox("Downsampling", &settings.downsampling);
				ImGui::PopStyleColor();
			}
		});

		addSettings("Anti-Aliasing", {
			[&]() -> void {
				const auto& items = msaaSettingsItems;
				auto& selected = msaaSettingsSelected;

				ImGui::PushItemWidth(120);
				if (ImGui::BeginCombo("MSAA", selected.c_str()))
				{
					for (uint32_t n = 0; n < items.size(); n++)
					{
						auto current = items[n].c_str();
						bool isSelected = (selected == current);
						if (ImGui::Selectable(current, isSelected)) {
							selected = items[n];
							settings.multisampleCount = 1 << n; // 2^n
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PushStyleColorDebugText();
				ImGui::Checkbox("Transparency", &settings.multisampleTransparency);
				ImGui::Checkbox("Distant Alpha", &settings.distantAlphaDensityFix);
				ImGui::PopStyleColor();
				ImGui::PopItemWidth();
			}
		});

		addSettings("Textures", {
			[&]() -> void {
				const auto& items = filterSettingsItems;
				auto& selected = filterSettingsSelected;

				ImGui::PushItemWidth(120);
				if (ImGui::BeginCombo("Filter", selected.c_str()))
				{
					for (uint32_t n = 0; n < items.size(); n++)
					{
						auto current = items[n].c_str();
						bool isSelected = (selected == current);
						if (ImGui::Selectable(current, isSelected)) {
							selected = items[n];
							if (selected == items[0]) {
								settings.anisotropicFilter = false;
							}
							else {
								settings.anisotropicFilter = true;
								settings.anisotropicLevel = 1 << n; // 2^n
							}
						}
					}
					ImGui::EndCombo();
				}
				ImGui::Checkbox("Cloud Blur", &settings.skyTexBlur);
				ImGui::PopItemWidth();
			}
		});

		addSettings("Image", {
			[&]() -> void {
				ImGui::PushItemWidth(constants().elementWidth);
				ImGui::SliderFloat("##Gamma", &settings.gamma, 0.5, 1.5, "%.3f Gamma");
				float f = settings.brightness * 100;
				if (ImGui::SliderFloat("##Min", &f, -1, 1, "%.2f Min Brightness")) {
					settings.brightness = f / 100;
				}
				ImGui::SliderFloat("##Max", &settings.contrast, 0, 5, "%.2f Max Brightness");
				ImGui::PopItemWidth();
			}
		});
	}
}