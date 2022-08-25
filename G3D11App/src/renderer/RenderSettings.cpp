#include "stdafx.h"

#include "RenderSettings.h"
#include "Gui.h"
#include "imgui/imgui.h"

namespace renderer::gui::settings {

	const std::array<std::string, 5> filterSettingsItems = { "Trilinear", "AF  x2", "AF  x4", "AF  x8", "AF x16" };
	std::string filterSettingsSelected = filterSettingsItems[4];

	void init(RenderSettings& settings)
	{
		addSettings("Renderer", {
			[&]()  -> void {
				ImGui::Checkbox("Wireframe Mode", &settings.wireframe);
				ImGui::Checkbox("Reverse Z", &settings.reverseZ);
			}
		});

		addSettings("Textures", {
			[&]() -> void {
				ImGui::PushItemWidth(120);
				if (ImGui::BeginCombo("Filter", filterSettingsSelected.c_str()))
				{
					for (int n = 0; n < filterSettingsItems.size(); n++)
					{
						auto current = filterSettingsItems[n].c_str();
						bool isSelected = (filterSettingsSelected == current);
						if (ImGui::Selectable(current, isSelected)) {
							filterSettingsSelected = filterSettingsItems[n];
							if (filterSettingsSelected == filterSettingsItems[0]) {
								settings.anisotropicFilter = false;
							}
							else {
								settings.anisotropicFilter = true;
								if (filterSettingsSelected == filterSettingsItems[1]) {
									settings.anisotropicLevel = 2;
								}
								if (filterSettingsSelected == filterSettingsItems[2]) {
									settings.anisotropicLevel = 4;
								}
								if (filterSettingsSelected == filterSettingsItems[3]) {
									settings.anisotropicLevel = 8;
								}
								if (filterSettingsSelected == filterSettingsItems[4]) {
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