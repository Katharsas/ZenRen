#include "stdafx.h"
#include "Gui.h"

#include <windowsx.h>

#include "imgui/imgui.h"
#include "imgui/imgui_custom.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "../Util.h"

namespace renderer {

	std::map<std::string, std::list<GuiComponent>> windowsToGuis;
	std::map<std::string, std::list<GuiComponent>> settingsGroupToGuis;

	void addWindow(const std::string& windowName, GuiComponent guiComponent)
	{
		auto components = util::getOrCreate(windowsToGuis, windowName);
		components.push_back(guiComponent);
	}

	void addSettings(const std::string& groupName, GuiComponent guiComponent)
	{
		auto& components = util::getOrCreate(settingsGroupToGuis, groupName);
		components.push_back(guiComponent);
	}

	void initGui(HWND hWnd, D3d d3d)
	{
		
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX11_Init(d3d.device, d3d.deviceContext);

		// DPI scaling and font
		float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hWnd);
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Consola.ttf", (int)((13 * dpiScale) + 0.5f));
	}

	void drawGui()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Settings");
		for (const auto& [groupName, commands] : settingsGroupToGuis) {
			ImGui::SameLine();
			ImGui::BeginGroupPanel(groupName.c_str(), ImVec2(0, 0));
			for (const auto& command : commands) {
				command.buildGui();
			}
			ImGui::EndGroupPanel();
		}
		ImGui::End();

		for (auto& [windowName, commands] : windowsToGuis) {
			ImGui::Begin(windowName.c_str());
			for (const auto& command : commands) {
				command.buildGui();
			}
			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void cleanGui()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}