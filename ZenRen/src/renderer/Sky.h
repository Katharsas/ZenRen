#pragma once

#include "dx11.h"

namespace renderer
{
	const float defaultTime = 0.0f;

	D3DXCOLOR getSkyLightFromIntensity(float intensity, float currentTime = defaultTime);
	D3DXCOLOR getSkyColor(float currentTime = defaultTime);
}

