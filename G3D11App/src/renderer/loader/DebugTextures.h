#pragma once

#include <filesystem>

namespace renderer::loader {
	void initDebugTextures(std::unordered_map<std::string, const std::filesystem::path>* assetNamesToPaths);
}
