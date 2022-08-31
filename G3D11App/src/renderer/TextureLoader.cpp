#include "stdafx.h"
#include "TextureLoader.h"

#include "../Util.h"

namespace fs = std::filesystem;

namespace renderer::loader {

	const std::filesystem::path DEFAULT_TEXTURE = "./default_texture.png";

	std::unordered_map<std::string, const std::filesystem::path> textureNamesToPaths;

	void scanDirForTextures(std::filesystem::path& rootDir)
	{
		textureNamesToPaths.clear();

		LOG(INFO) << "Scanning dir for TGA textures: " << rootDir.u8string();

		for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(rootDir)) {
			if (!std::filesystem::is_directory(dirEntry)) {
				const auto& path = dirEntry.path();
				auto& filename = path.filename().u8string();
				util::asciiToLowercase(filename);
				if (util::endsWith(filename, ".tga")) {
					textureNamesToPaths.insert(std::pair(filename, path));
				}
			}
		}

		LOG(INFO) << "Number of textures found by scan: " << textureNamesToPaths.size();
	}

	const std::filesystem::path& getTexturePathOrDefault(const std::string& filename)
	{
		auto& it = textureNamesToPaths.find(filename);
		if (it != textureNamesToPaths.end()) {
			return it->second;
		}
		else {
			return DEFAULT_TEXTURE;
		}
	}
}
