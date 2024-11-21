#pragma once

#include "Common.h"

namespace assets
{
	struct LoadDebugFlags {
		bool vobsTint = false;
		bool staticLights = false;
		bool staticLightRays = false;
		bool staticLightTintUnreached = false;
	};
}
namespace render
{
	struct VobLighting
	{
		DirectX::XMVECTOR direction;// pre-inverted
		COLOR color;
		bool receiveLightSun;
	};

	struct StaticInstance {
		std::string meshName;// TODO rename to modelName or visualAsset or visualFile
		DirectX::XMMATRIX transform;
		std::array<DirectX::XMVECTOR, 2> bbox;// pos_min, pos_max
		VobLighting lighting;
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