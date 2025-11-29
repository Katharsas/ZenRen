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

	void advanced(RenderSettings& settings, const std::function<void()> gui)
	{
		ImGui::PushStyleColorDebugText();
		if (settings.showAdvancedSettings) {
			gui();
		}
		ImGui::PopStyleColor();
	}

	void init(
		RenderSettings& settings,
		const std::function<void()>& notifyGameSwitchParam)
	{
		notifyGameSwitch = notifyGameSwitchParam;

		addSettings("", {
			[&]() -> void {
				ImGui::Checkbox("Show Advanced", &settings.showAdvancedSettings);
			}
		});

		addSettings("Renderer", {
			[&]() -> void {
				if (ImGui::Checkbox("Gothic 2", &settings.isG2)) {
					notifyGameSwitch();
				}

				ImGui::VerticalSpacing();
				ImGui::SliderFloat("##ViewDistance", &settings.viewDistance, 1, 2000, "%.0f View Distance");

				ImGui::VerticalSpacing();
				ImGui::Checkbox("Fog", &settings.distanceFog);
				ImGui::SliderFloat("##FogStart", &settings.distanceFogStart, 1, 200, "%.0f Fog Start");
				ImGui::SliderFloat("##FogEnd", &settings.distanceFogEnd, 1, 2000, "%.0f Fog End");
				
				advanced(settings, [&]() -> void {
					ImGui::SliderFloat("##FogSky", &settings.distanceFogSkyFactor, 0, 20, "%.2f Fog Sky");
					ImGui::SliderFloat("##FogEaseOut", &settings.distanceFogEaseOut, -0.5, 2, "%.2f Fog EaseOut");
					ImGui::VerticalSpacing();

					ImGui::PushItemWidth(120);
					gui::comboEnum("Shader Mode", shaderModeComboState, settings.shader.mode);
					ImGui::PopItemWidth();
					ImGui::Checkbox("Wireframe Mode", &settings.wireframe);
					ImGui::Checkbox("Enable Alpha", &settings.outputAlpha);
					ImGui::SliderFloat("##AlphaCutoff", &settings.alphaCutoff, 0.01, 0.99, "%.2f Alpha Cutoff");

					ImGui::VerticalSpacing();
					//ImGui::Checkbox("Depth Prepass", &settings.depthPrepass);
					ImGui::Checkbox("Opaque Passes", &settings.passesOpaque);
					ImGui::Checkbox("Blend Passes", &settings.passesBlend);

					ImGui::VerticalSpacing();
					ImGui::SliderFloat("##Debug1", &settings.debugFloat1, -1.f, 1.f, "%.2f Debug 1");
					ImGui::SliderFloat("##Debug2", &settings.debugFloat2, -1.f, 1.f, "%.2f Debug 2");

					ImGui::VerticalSpacing();
					ImGui::Checkbox("Reverse Z", &settings.reverseZ);
				});
			}
		});

		addSettings("Resolution", {
			[&]() -> void {
				ImGui::PushItemWidth(constants().inputFloatWidth);
				ImGui::InputFloat("Resolution Scaling", &settings.resolutionScaling, 0, 0, "%.2f");
				ImGui::PopItemWidth();
				advanced(settings, [&]() -> void {
					ImGui::Checkbox("Smooth Scaling", &settings.resolutionUpscaleSmooth);
					//ImGui::Checkbox("Downsampling", &settings.downsampling);
				});
			}
		});

		addSettings("Anti-Aliasing", {
			[&]() -> void {
				ImGui::PushItemWidth(120);
				gui::combo("MSAA", msaaSettingsComboState, [&](uint32_t index) -> void {
					settings.multisampleCount = 1 << index; // 2^index
				});
				ImGui::PopItemWidth();
				advanced(settings, [&]() -> void {
					ImGui::Checkbox("Transparency", &settings.multisampleTransparency);
					ImGui::Checkbox("Distant Alpha", &settings.distantAlphaDensityFix);
				});
			}
		});

		addSettings("Textures", {
			[&]() -> void {
				ImGui::PushItemWidth(120);
				gui::combo("Filter", filterSettingsComboState, [&](uint32_t index) -> void {
					settings.anisotropicLevel = 1 << index; // 2^index
				});
				ImGui::PopItemWidth();
				ImGui::Checkbox("Cloud Blur", &settings.skyTexBlur);
			}
		});

		addSettings("Image", {
			[&]() -> void {
				ImGui::SliderFloat("##Gamma", &settings.gamma, 0.5, 1.5, "%.3f Gamma");
				float f = settings.brightness * 100;
				if (ImGui::SliderFloat("##Min", &f, -1, 1, "%.2f Min Brightness")) {
					settings.brightness = f / 100;
				}
				ImGui::SliderFloat("##Max", &settings.contrast, 0, 5, "%.2f Max Brightness");
			}
		});

		addSettings("Camera", {
			[&]() -> void {
				ImGui::SliderFloat("##FOV", &settings.fovVertical, 50, 100, "%.0f FOV (Vertical)");
			}
		});
	}
}