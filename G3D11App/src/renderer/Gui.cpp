#include "stdafx.h"
#include "Gui.h"

#include <windowsx.h>
#include <d3d11.h>

#include "imgui/imgui.h"
#include "imgui/imgui_custom.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

namespace renderer {

	std::map<std::string, std::list<GuiComponent>> windowsToGuis;
	std::map<std::string, std::list<GuiComponent>> settingsGroupToGuis;

	void addWindow(const std::string& windowName, GuiComponent guiComponent)
	{
		auto entryIt = windowsToGuis.find(windowName);
		if (entryIt == windowsToGuis.end()) {
			std::list<GuiComponent> guiComponents;
			guiComponents.push_back(guiComponent);
			windowsToGuis.insert(std::make_pair(windowName, guiComponents));
		}
		else {
			entryIt->second.push_back(guiComponent);
		}
	}

	void addSettings(const std::string& groupName, GuiComponent guiComponent)
	{
		auto entryIt = settingsGroupToGuis.find(groupName);
		if (entryIt == settingsGroupToGuis.end()) {
			std::list<GuiComponent> guiComponents;
			guiComponents.push_back(guiComponent);
			settingsGroupToGuis.insert(std::make_pair(groupName, guiComponents));
		}
		else {
			entryIt->second.push_back(guiComponent);
		}
	}

	void initGui(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
	{
		
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX11_Init(device, deviceContext);

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