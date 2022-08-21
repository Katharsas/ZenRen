#pragma once

#include "dx11.h"
#include "ShaderManager.h"

namespace renderer::world {
	void updateObjects();
	void initVertexIndexBuffers(D3d d3d, bool reverseZ);
	void initConstantBufferPerObject(D3d d3d);
	void draw(D3d d3d, ShaderManager* shaders);
	void clean();
}

