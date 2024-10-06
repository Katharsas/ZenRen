#pragma once

#include "Common.h"

namespace render
{
	struct StaticInstance {
		std::string meshName;
		DirectX::XMMATRIX transform;
		std::array<VEC3, 2> bbox;// pos_min, pos_max
		bool receiveLightSun;
		COLOR colLightStatic;
		DirectX::XMVECTOR dirLightStatic;// pre-inverted
	};

	struct Light {
		// TODO falloff and type surely play a role
		VEC3 pos;
		bool isStatic;
		COLOR color;
		float range;
	};

	struct InMemoryTexFile {
		int32_t width;
		int32_t height;
		std::vector<uint8_t> ddsRaw;
	};

	struct RenderData {
		bool isOutdoorLevel = false;
		VERT_CHUNKS_BY_MAT worldMesh;
		VERT_CHUNKS_BY_MAT staticMeshes;
		VERTEX_DATA_BY_MAT dynamicMeshes;
		std::vector<InMemoryTexFile> worldMeshLightmaps;
	};
}