#include "stdafx.h"
#include "GameLoop.h"
#include "TimerPrecision.h"
#include "renderer/D3D11Renderer.h"

namespace game
{
	void init(HWND hWnd)
	{
		enablePreciseTimerResolution();
		renderer::initD3D(hWnd);
	}

	void execute()
	{
		renderer::renderFrame();
		Sleep(10);
	}

	void cleanup()
	{
		renderer::cleanD3D();
		disablePreciseTimerResolution();
	}
}