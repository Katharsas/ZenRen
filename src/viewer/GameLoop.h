#pragma once
#include "Args.h"
#include "Win.h"

namespace viewer
{
	void init(HWND hWnd, Arguments args, uint16_t width, uint16_t height);
	void onWindowResized(uint16_t width, uint16_t height);
	void onWindowDpiChanged(float dpiScale);
	void execute();
	void cleanup();
};

