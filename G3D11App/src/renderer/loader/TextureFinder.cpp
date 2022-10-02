#include "stdafx.h"
#include "TextureFinder.h"

#include "../../Util.h"

namespace fs = std::filesystem;

namespace renderer::loader {

	const std::filesystem::path DEFAULT_TEXTURE = "./default_texture.png";
	const std::filesystem::path NUMBER_TEXTURES_DIR = "../../DebugTextureGenerator/number-textures";

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

		// debug textures with numbers from 0-99
		textureNamesToPaths.insert(std::pair("tex_default.png", NUMBER_TEXTURES_DIR / "tex_default.png"));
		for (int32_t n = 0; n < 1000; n++) {
			auto filename = getNumberTexName(n);
			auto path = NUMBER_TEXTURES_DIR / filename;
			if (std::filesystem::is_regular_file(path)) {
				textureNamesToPaths.insert(std::pair(filename, path));
			}
		}
	}

	const std::filesystem::path& getTexturePathOrDefault(const std::string& filename)
	{
		std::string filenameLower = filename;
		util::asciiToLowercase(filenameLower);
		auto& it = textureNamesToPaths.find(filenameLower);
		if (it != textureNamesToPaths.end()) {
			return it->second;
		}
		else {
			return DEFAULT_TEXTURE;
		}
	}

	const std::string getNumberTexName(int32_t number) {
		if (number < 0 || number >= 1000) {
			return "tex_default.png";
		}
		else {
			std::stringstream ss;
			ss << "tex_" << std::setw(3) << std::setfill('0') << number << std::setw(0) << ".png";
			std::string name = ss.str();
			return name;
		}
	}
}
