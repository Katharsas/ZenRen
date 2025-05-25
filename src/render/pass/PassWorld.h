#pragma once

#include "../Dx.h"
#include "../Settings.h"
#include "../Renderer.h"
#include "../ShaderManager.h"

#include "render/d3d/BlendState.h"

namespace render::pass::world
{
	struct WorldSettings {
		float timeOfDay = 0.f;// range 0 to 1 (midday to midday)
		float timeOfDayChangeSpeed = 0.f;// good showcase value: 0.03

		bool drawWorld = true;
		bool drawStaticObjects = true;
		bool drawSky = true;

		bool chunkedRendering = true;

		bool lowLodOnly = false;
		bool enableLod = true;
		float lodRadius = 300;

		bool enableFrustumCulling = true;
		bool updateFrustumCulling = true;

		bool debugWorldShaderEnabled = false;
		float debugDrawVertAmount = 1.f;

		bool debugSingleDrawEnabled = false;
		int32_t debugSingleDrawIndex = 0;

		bool chunkFilterXEnabled = false;
		bool chunkFilterYEnabled = false;
		int16_t chunkFilterX = 0;
		int16_t chunkFilterY = 0;
	};

	LoadWorldResult loadWorld(D3d d3d, const std::string& level);
	void updateObjects(float deltaTime);
	void updateCameraFrustum(const DirectX::BoundingFrustum& cameraFrustum, bool hasCameraMoved);
	WorldSettings& getWorldSettings();
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void init(D3d d3d);
	Color getBackgroundColor();
	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs);
	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, BlendType pass);
	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, BlendType pass);
	void clean();
}

