#pragma once

namespace renderer
{
	void initD3D(HWND hWnd);
	// Clean up DirectX and COM
	void cleanD3D();
	void updateObjects();
	void renderFrame(void);

}