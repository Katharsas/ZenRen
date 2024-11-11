#pragma once
#include "Args.h"
#include "Win.h"

namespace viewer
{
	void init(HWND hWnd, Arguments args, uint32_t width, uint32_t height);
	void onWindowResized(uint32_t width, uint32_t height);
	void onWindowDpiChanged(float dpiScale);
	void execute();
	void cleanup();
};

