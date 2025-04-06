#pragma once

#include <magic_enum.hpp>
#include "Common.h"

namespace render
{
	const float defaultTime = 0.0f;

	struct SkyTex {
		std::string name;
		float uvScale = 20;
		bool blurDisabled = false;
	};

	struct SkyTexState {
		float timeKey;
		SkyTex tex;
		float alpha;
		Uv uvSpeed = { 0, 0 };
		Color texlightColor = greyscale(1);
	};
	inline std::ostream& operator <<(std::ostream& os, const SkyTexState& that)
	{
		return os << "[TIME:" << that.timeKey << " TEX:" << that.tex.name << " ALPHA:" << that.alpha << "]";
	}

	std::array<SkyTexState, 2> getSkyLayers(float timeOfDay);
	bool getSwapLayers(float timeOfDay);
	Color getSkyLightFromIntensity(float intensity, float currentTime = defaultTime);
	Color getSkyColor(float currentTime = defaultTime);
}

