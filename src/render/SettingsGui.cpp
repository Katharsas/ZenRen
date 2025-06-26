#include "stdafx.h"
#include "SettingsGui.h"

#include "render/Gui.h"
#include "render/GuiHelper.h"

#include "imgui.h"
#include "imgui/imgui_custom.h"
#include "magic_enum.hpp"

namespace render::gui::settings {

	// TODO we should have checkbox toggles for each light type so we can add them up however we want

	auto shaderModeComboState = gui::comboStateFromEnum<ShaderMode>();
	auto filterSettingsComboState = gui::ComboState<5>({ "Trilinear", "AF  x2", "AF  x4", "AF  x8", "AF x16" }, 4);
	auto msaaSettingsComboState = gui::ComboState<4>({ "None", "x2", "x4", "x8" }, 2);

	std::function<void()> notifyGameSwitch;

	void init(
		RenderSettings& settings,
		const std::function<void()>& notifyGameSwitchParam)
	{
		notifyGameSwitch = notifyGameSwitchParam;

		addSettings("Renderer", {
			[&]() -> void {
				ImGui::PushItemWidth(constants().elementWidth);
				if (ImGui::Checkbox("Gothic 2", &settings.isG2)) {
					notifyGameSwitch();
				}

				ImGui::VerticalSpacing();
				ImGui::SliderFloat("##ViewDistance", &settings.viewDistance, 1, 2000, "%.0f View Distance");

				ImGui::VerticalSpacing();
				ImGui::Checkbox("Fog", &settings.distanceFog);
				ImGui::SliderFloat("##FogStart", &settings.distanceFogStart, 1, 200, "%.0f Fog Start");
				ImGui::SliderFloat("##FogEnd", &settings.distanceFogEnd, 1, 2000, "%.0f Fog End");
				ImGui::SliderFloat("##FogSky", &settings.distanceFogSkyFactor, 1, 10, "%.2f Fog Sky");

				ImGui::VerticalSpacing();
				ImGui::PushStyleColorDebugText();

				ImGui::PushItemWidth(120);
				gui::comboEnum("Shader Mode", shaderModeComboState, settings.shader.mode);
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
				ImGui::PushItemWidth(120);
				gui::combo("MSAA", msaaSettingsComboState, [&](uint32_t index) -> void {
					settings.multisampleCount = 1 << index; // 2^index
				});
				ImGui::PushStyleColorDebugText();
				ImGui::Checkbox("Transparency", &settings.multisampleTransparency);
				ImGui::Checkbox("Distant Alpha", &settings.distantAlphaDensityFix);
				ImGui::PopStyleColor();
				ImGui::PopItemWidth();
			}
		});

		addSettings("Textures", {
			[&]() -> void {
				ImGui::PushItemWidth(120);
				gui::combo("Filter", filterSettingsComboState, [&](uint32_t index) -> void {
					if (index == 0) {
						settings.anisotropicFilter = false;
					}
					else {
						settings.anisotropicFilter = true;
						settings.anisotropicLevel = 1 << index; // 2^index
					}
				});
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