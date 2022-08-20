#pragma once
namespace game
{
	void init(HWND hWnd);
	void onWindowResized(uint32_t width, uint32_t height);
	void execute();
	void cleanup();
};

