#pragma once

#include "dx11.h"
#include "Renderer.h"
#include "ShaderManager.h"

namespace renderer::world
{
	struct WorldSettings {
		float timeOfDay = 0.f;// range 0 to 1 (midday to midday)
		float timeOfDayChangeSpeed = 0.f;// good showcase value: 0.03
	};

	void loadLevel(D3d d3d, std::string& level);
	void updateObjects(float deltaTime);
	void updateShaderSettings(D3d d3d, const RenderSettings& settings);
	const WorldSettings& getWorldSettings();
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void init(D3d d3d);
	void initConstantBufferPerObject(D3d d3d);
	void drawPrepass(D3d d3d, ShaderManager* shaders);
	void drawWorld(D3d d3d, ShaderManager* shaders, ID3D11RenderTargetView* targetRtv);
	void drawWireframe(D3d d3d, ShaderManager* shaders);
	void clean();
}

