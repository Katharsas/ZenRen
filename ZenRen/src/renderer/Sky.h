#pragma once

#include "dx11.h"

namespace renderer
{
	const float defaultTime = 0.0f;

	D3DXCOLOR getSkyLightFromIntensity(float intensity, float currentTime = defaultTime);

	// TODO at some point we should maybe separate sky data and PipelineSky?
	void drawSky(D3d d3d, ID3D11RenderTargetView* targetRtv, float currentTime = defaultTime);
}

