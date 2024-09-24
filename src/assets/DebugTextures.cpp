#include "stdafx.h"
#include "DebugTextures.h"

#include "../Util.h"

namespace fs = std::filesystem;

namespace assets {

	
	const std::filesystem::path NUMBER_TEXTURES_DIR = "../../DebugTextureGenerator/number-textures";
	const bool numberTexturesEnabled = false;

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

	void initDebugTextures(std::unordered_map<std::string, const fs::path>* assetNamesToPaths)
	{
		const bool numberTexturesEnabled = false;
		if (numberTexturesEnabled) {
			// debug number textures from tex_000.png to tex_999.png
			assetNamesToPaths->insert(std::pair("tex_default.png", NUMBER_TEXTURES_DIR / "tex_default.png"));
			for (int32_t n = 0; n < 1000; n++) {
				auto filename = getNumberTexName(n);
				auto path = NUMBER_TEXTURES_DIR / filename;
				if (std::filesystem::is_regular_file(path)) {
					assetNamesToPaths->insert(std::pair(filename, path));
				}
			}
		}
	}
}
