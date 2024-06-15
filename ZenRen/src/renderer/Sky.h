#pragma once

#include "dx11.h"

#include "RendererCommon.h"
#include "magic_enum/magic_enum.hpp"

namespace renderer
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
		D3DXCOLOR texlightColor = greyscale(1);
	};
	inline std::ostream& operator <<(std::ostream& os, const SkyTexState& that)
	{
		return os << "[TIME:" << that.timeKey << " TEX:" << that.tex.name << " ALPHA:" << that.alpha << "]";
	}

	std::array<SkyTexState, 2> getSkyLayers(float timeOfDay);
	boolean getSwapLayers(float timeOfDay);
	D3DXCOLOR getSkyLightFromIntensity(float intensity, float currentTime = defaultTime);
	D3DXCOLOR getSkyColor(float currentTime = defaultTime);
}

