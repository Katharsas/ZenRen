#include "stdafx.h"

#include "RenderSettings.h"
#include "Gui.h"
#include "imgui/imgui.h"

namespace renderer::gui::settings {

	// TODO we should have checkbox toggles for each light type so we can add them up however we want
	const std::array<std::string, 5> shaderModeItems = { "Full", "Solid Only", "Diffuse Only", "Normals Only", "Light Static" };
	std::string shaderModeSelected = shaderModeItems[0];
	const std::array<std::string, 5> filterSettingsItems = { "Trilinear", "AF  x2", "AF  x4", "AF  x8", "AF x16" };
	std::string filterSettingsSelected = filterSettingsItems[4];

	const std::array<std::string, 4> msaaSettingsItems = { "None", "x2", "x4", "x8" };
	std::string msaaSettingsSelected = msaaSettingsItems[2];

	void init(RenderSettings& settings)
	{
		addSettings("Renderer", {
			[&]()  -> void {
				ImGui::Checkbox("Wireframe Mode", &settings.wireframe);
				ImGui::Checkbox("Reverse Z", &settings.reverseZ);

				const auto& items = shaderModeItems;
				auto& selected = shaderModeSelected;

				ImGui::PushItemWidth(120);
				if (ImGui::BeginCombo("Shader Mode", selected.c_str()))
				{
					for (int n = 0; n < items.size(); n++)
					{
						auto current = items[n].c_str();
						bool isSelected = (selected == current);
						if (ImGui::Selectable(current, isSelected)) {
							selected = items[n];
							if (selected == items[0]) {
								settings.shader.mode = ShaderMode::Default;
							}
							if (selected == items[1]) {
								settings.shader.mode = ShaderMode::Solid;
							}
							if (selected == items[2]) {
								settings.shader.mode = ShaderMode::Diffuse;
							}
							if (selected == items[3]) {
								settings.shader.mode = ShaderMode::Normals;
							}
							if (selected == items[4]) {
								settings.shader.mode = ShaderMode::Light_Static;
							}
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();

				ImGui::Checkbox("Depth Prepass", &settings.depthPrepass);
			}
		});

		addSettings("Resolution", {
			[&]()  -> void {
				ImGui::PushItemWidth(60);
				ImGui::InputFloat("Resolution Scaling", &settings.resolutionScaling, 0);
				ImGui::PopItemWidth();
				ImGui::PushItemWidth(120);
				ImGui::Checkbox("Smooth Scaling", &settings.resolutionUpscaleSmooth);
				ImGui::Checkbox("Downsampling", &settings.downsampling);
				ImGui::PopItemWidth();
			}
		});

		addSettings("Anti-Aliasing", {
			[&]() -> void {
				const auto& items = msaaSettingsItems;
				auto& selected = msaaSettingsSelected;

				ImGui::PushItemWidth(120);
				if (ImGui::BeginCombo("MSAA", selected.c_str()))
				{
					for (int n = 0; n < items.size(); n++)
					{
						auto current = items[n].c_str();
						bool isSelected = (selected == current);
						if (ImGui::Selectable(current, isSelected)) {
							selected = items[n];
							if (selected == items[0]) {
								settings.multisampleCount = 1;
							}
							if (selected == items[1]) {
								settings.multisampleCount = 2;
							}
							if (selected == items[2]) {
								settings.multisampleCount = 4;
							}
							if (selected == items[3]) {
								settings.multisampleCount = 8;
							}
						}
					}
					ImGui::EndCombo();
				}
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
					for (int n = 0; n < items.size(); n++)
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
								if (selected == items[1]) {
									settings.anisotropicLevel = 2;
								}
								if (selected == items[2]) {
									settings.anisotropicLevel = 4;
								}
								if (selected == items[3]) {
									settings.anisotropicLevel = 8;
								}
								if (selected == items[4]) {
									settings.anisotropicLevel = 16;
								}
							}
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
			}
		});
	}
}