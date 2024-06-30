#pragma once
#include "GameArgs.h"

namespace game
{
	void init(HWND hWnd, Arguments args, uint32_t width, uint32_t height);
	void onWindowResized(uint32_t width, uint32_t height);
	void execute();
	void cleanup();
};

