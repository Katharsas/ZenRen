#pragma once

#include <filesystem>

namespace assets
{
	void initDebugTextures(std::unordered_map<std::string, const std::filesystem::path>* assetNamesToPaths);
}
