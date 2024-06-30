#pragma once

#include "dx11.h"

namespace render
{
	const float GUI_PANEL_WIDTH = 240;
	const float GUI_PANEL_PADDING = 8;
	const float GUI_ELEMENT_WIDTH = GUI_PANEL_WIDTH - (2 * GUI_PANEL_PADDING);
	const float INPUT_FLOAT_WIDTH = 50;

	struct GuiComponent
	{
		const std::function<void()> buildGui;
	};

	void addWindow(const std::string& windowName, GuiComponent guiComponent);
	void addSettings(const std::string& groupName, GuiComponent guiComponent);
	void addInfo(const std::string& groupName, GuiComponent guiComponent);
	void initGui(HWND hWnd, D3d d3d);
	void drawGui();
	void cleanGui();
}

