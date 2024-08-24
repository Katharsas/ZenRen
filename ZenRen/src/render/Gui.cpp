#include "stdafx.h"
#include "Gui.h"

#include <windowsx.h>

#include "imgui/imgui.h"
#include "imgui/imgui_custom.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "../Util.h"

namespace render
{
	using std::string;

	std::map<string, std::list<GuiComponent>> windowsToGuis;
	std::map<string, std::list<GuiComponent>> settingsGroupToGuis;
	std::map<string, std::list<GuiComponent>> infoGroupToGuis;

	void addWindow(const string& windowName, GuiComponent guiComponent)
	{
		auto& components = util::getOrCreateDefault(windowsToGuis, windowName);
		components.push_back(guiComponent);
	}

	void addSettings(const string& groupName, GuiComponent guiComponent)
	{
		auto& components = util::getOrCreateDefault(settingsGroupToGuis, groupName);
		components.push_back(guiComponent);
	}

	void addInfo(const string& groupName, GuiComponent guiComponent)
	{
		auto& components = util::getOrCreateDefault(infoGroupToGuis, groupName);
		components.push_back(guiComponent);
	}

	void initGui(HWND hWnd, D3d d3d)
	{
		
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

		ImGui::StyleColorsDark();// lookup default colors here

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX11_Init(d3d.device, d3d.deviceContext);

		// DPI scaling and font
		float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hWnd);
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Consola.ttf", 13.f * dpiScale);
	}

	float drawWindowWithGroups(const string& windowName, std::map<string, std::list<GuiComponent>> groups, float posY, float sizeY = -1)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));

		ImGui::SetNextWindowPos({ 0.f, posY });
		ImGui::SetNextWindowSize({ GUI_PANEL_WIDTH, -1 }, ImGuiCond_Once);
		ImGui::SetNextWindowSizeConstraints({ 0.f, (sizeY < 0 ? 0.f : sizeY) }, { FLT_MAX , (sizeY < 0 ? FLT_MAX : sizeY) });
		auto windowFlags = sizeY < 0 ?
			ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None; // fix window never getting auto resized

		if (ImGui::Begin(windowName.c_str(), 0, windowFlags)) {
			for (const auto& [groupName, commands] : groups) {
				// group text
				if (groups.size() > 1 && groupName != "") {
					if (groupName != "") {
						ImGui::Dummy(ImVec2(0.0f, 6.0f));
						ImGui::SetCursorPos({ ImGui::GetCursorPos().x + 8, ImGui::GetCursorPos().y });
						ImGui::Text((groupName).c_str());
					}
				}
				// group elements
				// make roughly as dark as original ImGuiCol_WindowBg (when added on top of custom WindowBg)
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 0.80f));
				ImGui::BeginChild(groupName.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
				ImGui::SetCursorPos({ ImGui::GetCursorPos().x + GUI_PANEL_PADDING, ImGui::GetCursorPos().y });
				ImGui::BeginGroup();
				for (const auto& command : commands) {
					command.buildGui();
				}
				ImGui::EndGroup();
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
			ImGui::Dummy(ImVec2(0.0f, 3.0f));
		}
		float sizeX = ImGui::GetWindowSize().y;
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		return sizeX;
	}

	void drawGui()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		auto size = viewport->Size;

		float currentPosY = 0;
		currentPosY += drawWindowWithGroups("Info", infoGroupToGuis, currentPosY, -1);
		currentPosY += drawWindowWithGroups("Settings", settingsGroupToGuis, currentPosY, size.y - currentPosY);

		currentPosY = 0;
		// TODO place everything else on right side
		for (auto& [windowName, commands] : windowsToGuis) {
			ImGui::SetNextWindowPos({ GUI_PANEL_WIDTH, currentPosY });
			ImGui::Begin(windowName.c_str());
			for (const auto& command : commands) {
				command.buildGui();
			}
			currentPosY += ImGui::GetWindowSize().y;
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