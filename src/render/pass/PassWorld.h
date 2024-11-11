#pragma once

#include "../WinDx.h"
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
		bool drawSky = true;

		bool enableFrustumCulling = true;
		bool updateFrustumCulling = true;

		bool debugWorldShaderEnabled = false;
		float debugDrawVertAmount = 1.f;

		bool debugSingleDrawEnabled = false;
		int32_t debugSingleDrawIndex = 0;
	};

	LoadWorldResult loadWorld(D3d d3d, const std::string& level);
	void updateObjects(float deltaTime);
	void updateCameraFrustum(const DirectX::BoundingFrustum& cameraFrustum);
	const WorldSettings& getWorldSettings();
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void init(D3d d3d);
	COLOR getBackgroundColor();
	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawPrepass(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void clean();
}

