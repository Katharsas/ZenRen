#pragma once

#include <filesystem>

namespace game
{
	struct Arguments {
		std::optional<std::filesystem::path> vdfFilesRoot;
		std::optional<std::filesystem::path> assetFilesRoot;
		std::optional<std::string> level;
	};

	void init(HWND hWnd, Arguments args);
	void onWindowResized(uint32_t width, uint32_t height);
	void execute();
	void cleanup();
};

