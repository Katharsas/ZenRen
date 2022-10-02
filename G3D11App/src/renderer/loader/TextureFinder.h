#pragma once

#include <filesystem>

namespace renderer::loader {
	void scanDirForTextures(std::filesystem::path& rootDir);
	const std::filesystem::path& getTexturePathOrDefault(const std::string& filename);
	const std::string getNumberTexName(int32_t number);
}
