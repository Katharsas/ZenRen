#pragma once

#include "dx11.h"

#include "RendererCommon.h"
#include "magic_enum/magic_enum.hpp"

namespace renderer
{
	const float defaultTime = 0.0f;

	enum class SkyTexType {
		DAY, NIGHT
	};

	struct SkyTexState {
		float timeKey;
		SkyTexType type;
		float alpha;
		D3DXCOLOR texlightColor = greyscale(1);
	};
	inline std::ostream& operator <<(std::ostream& os, const SkyTexState& that)
	{
		return os << "[TIME:" << that.timeKey << " TEX:" << magic_enum::enum_name(that.type) << " ALPHA:" << that.alpha << "]";
	}

	std::array<SkyTexState, 2> getSkyLayers(float timeOfDay);
	D3DXCOLOR getSkyLightFromIntensity(float intensity, float currentTime = defaultTime);
	D3DXCOLOR getSkyColor(float currentTime = defaultTime);
}

