#pragma once

#include "render/Dx.h"
#include "render/Settings.h"
#include "render/Renderer.h"
#include "render/ShaderManager.h"

#include "render/d3d/BlendState.h"

#include "render/pass/world/WorldSettings.h"

namespace render::pass::world
{
	LoadWorldResult loadWorld(D3d d3d, const std::string& level);
	void updateObjects(float deltaTime);
	void updatePrepareDraws(D3d d3d, const DirectX::BoundingFrustum& cameraFrustum, bool hasCameraMoved);
	WorldSettings& getWorldSettings();
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void init(D3d d3d);
	Color getBackgroundColor();
	void drawSky(D3d d3d, ShaderManager* shaders);
	void drawWorld(D3d d3d, ShaderManager* shaders, BlendType pass);
	void drawWireframe(D3d d3d, ShaderManager* shaders, BlendType pass);
	void clean();
}

