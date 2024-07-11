#pragma once

#include "../dx11.h"
#include "../Settings.h"
#include "../Renderer.h"
#include "../ShaderManager.h"

namespace render::pass::world
{
	struct WorldSettings {
		float timeOfDay = 0.f;// range 0 to 1 (midday to midday)
		float timeOfDayChangeSpeed = 0.f;// good showcase value: 0.03

		bool drawWorld = true;
		bool drawStaticObjects = true;
	};

	void loadLevel(D3d d3d, std::string& level);
	void updateObjects(float deltaTime);
	const WorldSettings& getWorldSettings();
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void init(D3d d3d);
	D3DXCOLOR getBackgroundColor();
	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawPrepass(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void clean();
}

