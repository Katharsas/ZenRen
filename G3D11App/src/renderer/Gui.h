#pragma once

#include <d3d11.h>

namespace renderer {
	struct GuiComponent
	{
		void (*buildGui)();
	};

	void addWindow(const std::string& windowName, GuiComponent guiComponent);
	void addSettings(const std::string& groupName, GuiComponent guiComponent);
	void initGui(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void drawGui();
	void cleanGui();
}

