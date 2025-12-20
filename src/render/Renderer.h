#pragma once

#include "Dx.h"
#include "render/basic/Common.h"
#include "render/d3d/Shader.h";
#include "render/d3d/GeometryBuffer.h"
#include "Texture.h"

namespace render
{
	// TODO rename to MeshBatchBuffers?
	struct MeshBatch {
		// we could wrap this in a normal Texture object to make it more similar to unbatched meshes
		ID3D11ShaderResourceView* texColorArray = nullptr;

		uint32_t drawCount = 0;
		uint32_t drawLodCount = 0;
		std::vector<ChunkVertCluster> vertClusters;
		std::vector<ChunkVertCluster> vertClustersLod;

		bool useIndices = false;
		VertexBuffer vbIndices = { sizeof(VertexIndex) };

		VertexBuffer vbPos = { sizeof(VertexPos) };
		VertexBuffer vbNormalUv = { sizeof(VertexNorUv) };
		VertexBuffer vbTexIndices = { sizeof(TexIndex) };// indices into texture array (base color textures)
		VertexBuffer vbOther = { sizeof(VertexBasic) };

		void release()
		{
			render::release(texColorArray);
			render::release(vbIndices.buffer);
			render::release(vbPos.buffer);
			render::release(vbNormalUv.buffer);
			render::release(vbTexIndices.buffer);
			render::release(vbOther.buffer);
		}
	};

	using GetVertexBuffers = std::vector<VertexBuffer> (*) (const MeshBatch&);

	struct ShaderContext {
		d3d::Shader shader;
		GetVertexBuffers getVertexBuffers = nullptr;

		static ShaderContext init(D3d d3d, const std::string& shaderName, const std::initializer_list<VertexAttributes>& attributes, GetVertexBuffers getVertexBuffers)
		{
			ShaderContext result;
			result.getVertexBuffers = getVertexBuffers;
			d3d::createShader(d3d, result.shader, shaderName, d3d::buildInputLayoutDesc(attributes));
			return result;
		}

		void release() {
			shader.release();
		}
	};

	struct LoadWorldResult {
		bool loaded = false;
		bool isG2 = false;
		bool isOutdoorLevel = false;
	};

	void initD3D(WindowHandle hWnd, const BufferSize& changedSize);
	bool loadLevel(const std::optional<std::string>& level, bool defaultSky = true);
	void onWindowResize(const BufferSize& changedSize);
	void onWindowDpiChange(float dpiScale);
	// Clean up DirectX and COM
	void cleanD3D();
	void update(float deltaTime);
	void renderFrame();
	void presentFrameBlocking();
	void reloadShaders();
}