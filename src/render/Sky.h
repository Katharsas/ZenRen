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
		UV uvSpeed = { 0, 0 };
		COLOR texlightColor = greyscale(1);
	};
	inline std::ostream& operator <<(std::ostream& os, const SkyTexState& that)
	{
		return os << "[TIME:" << that.timeKey << " TEX:" << that.tex.name << " ALPHA:" << that.alpha << "]";
	}

	std::array<SkyTexState, 2> getSkyLayers(float timeOfDay);
	bool getSwapLayers(float timeOfDay);
	COLOR getSkyLightFromIntensity(float intensity, float currentTime = defaultTime);
	COLOR getSkyColor(float currentTime = defaultTime);
}

