#include "stdafx.h"
#include "Gui.h"

#include <windowsx.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>

#include "imgui/imgui_custom.h"
#include "../Util.h"

namespace render::gui
{
	using std::string;

	const GuiConstants guiConstantsBase;
	GuiConstants guiConstantsScaled;

	float currentScaleFactor = 1.f;

	float groupSpacing = 4.f;
	float groupPadding = 1.f;

	ImFont* currentFont;

	bool isVisible = true;
	bool resetPanelWidth = false;

	std::map<string, std::list<GuiComponent>> windowsToGuis;
	std::map<string, std::list<GuiComponent>> settingsGroupToGuis;
	std::map<string, std::list<GuiComponent>> infoGroupToGuis;

	GuiConstants& constants() {
		return guiConstantsScaled;
	}

	void onToggleVisible() {
		isVisible = !isVisible;
	}

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

	void init(HWND hWnd, D3d d3d)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX11_Init(d3d.device, d3d.deviceContext);

		onWindowDpiChange(hWnd);
	}

	void onWindowDpiChange(HWND hWnd) {
		float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hWnd);

		// scale font
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		currentFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Consola.ttf", 12.f * dpiScale);

		// scale constants used by other UI code
		guiConstantsScaled = guiConstantsBase.scale(dpiScale);

		// scale imgui style
		// this is bullshit but we have to reset style somehow (???) so we can re-apply scaling (because ScaleAllSizes does change baseline sizes)
		ImGuiContext* context = ImGui::GetCurrentContext();
		context->Style = ImGuiStyle();

		// re-apply imgui style changess
		ImGui::StyleColorsDark();// lookup default colors here

		ImGui::GetStyle().ScaleAllSizes(dpiScale);// scales non-font-dependent UI elements like scrollbars

		resetPanelWidth = true;
	}

	float drawWindowWithGroups(const string& windowName, std::map<string, std::list<GuiComponent>> groups, float posY, float sizeY, bool resetPanelWidth)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));

		ImGui::SetNextWindowPos({ 0.f, posY });
		if (resetPanelWidth) {
			ImGui::SetNextWindowSize({ constants().panelWidth, -1 });
		}
		ImGui::SetNextWindowSizeConstraints({ 0.f, (sizeY < 0 ? 0.f : sizeY) }, { FLT_MAX , (sizeY < 0 ? FLT_MAX : sizeY) });
		auto windowFlags = sizeY < 0 ?
			ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None; // fix window never getting auto resized

		if (ImGui::Begin(windowName.c_str(), 0, windowFlags)) {
			for (const auto& [groupName, commands] : groups) {
				// group text
				if (groups.size() > 1 && groupName != "") {
					ImGui::VerticalSpacing(groupSpacing * currentScaleFactor);
					ImGui::SetCursorPos({ ImGui::GetCursorPos().x + 8, ImGui::GetCursorPos().y });
					ImGui::Text((groupName).c_str());
				}
				// group elements
				// make roughly as dark as original ImGuiCol_WindowBg (when added on top of custom WindowBg)
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 0.80f));
				std::string groupId = groupName == "" ? "##empty" : groupName;
				ImGui::BeginChild(groupId.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
				ImGui::VerticalSpacing(groupPadding * currentScaleFactor);
				ImGui::SetCursorPos({ ImGui::GetCursorPos().x + constants().panelPadding, ImGui::GetCursorPos().y });
				ImGui::BeginGroup();
				for (const auto& command : commands) {
					command.buildGui();
				}
				ImGui::EndGroup();
				ImGui::VerticalSpacing(groupPadding * currentScaleFactor);
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
			ImGui::VerticalSpacing(2.f * currentScaleFactor);
		}
		float sizeX = ImGui::GetWindowSize().y;
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		return sizeX;
	}

	void draw()
	{
		if (!isVisible) {
			return;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::PushFont(currentFont);

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		auto size = viewport->Size;

		float currentPosY = 0;
		currentPosY += drawWindowWithGroups("Info", infoGroupToGuis, currentPosY, -1, resetPanelWidth);
		currentPosY += drawWindowWithGroups("Settings", settingsGroupToGuis, currentPosY, size.y - currentPosY, resetPanelWidth);

		currentPosY = 0;
		// TODO place everything else on right side
		for (auto& [windowName, commands] : windowsToGuis) {
			ImGui::SetNextWindowPos({ constants().panelWidth, currentPosY });
			ImGui::Begin(windowName.c_str());
			for (const auto& command : commands) {
				command.buildGui();
			}
			currentPosY += ImGui::GetWindowSize().y;
			ImGui::End();
		}

		if (resetPanelWidth) resetPanelWidth = false;

		ImGui::PopFont();
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void clean()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}