#pragma once

#include <d3d11.h>

namespace renderer {
	struct GuiComponent
	{
		void (*buildGui)();
	};

	void addGui(std::string windowName, GuiComponent guiComponent);
	void initGui(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void drawGui();
	void cleanGui();
}

