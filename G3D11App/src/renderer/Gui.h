#pragma once

#include "dx11.h";

namespace renderer {
	struct GuiComponent
	{
		void (*buildGui)();
	};

	void addWindow(const std::string& windowName, GuiComponent guiComponent);
	void addSettings(const std::string& groupName, GuiComponent guiComponent);
	void initGui(HWND hWnd, D3d d3d);
	void drawGui();
	void cleanGui();
}

