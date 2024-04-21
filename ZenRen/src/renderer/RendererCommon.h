#pragma once

#include "dx11.h"

#include <string>
#include <ostream>
#include <vector>

namespace renderer
{
	inline std::ostream& operator <<(std::ostream& os, const D3DXCOLOR& that)
	{
		return os << "[R:" << that.r << " G:" << that.g << " B:" << that.b << " A:" << that.a << "]";
	}

	struct VEC3 {
		float x, y, z;
	};
	inline std::ostream& operator <<(std::ostream& os, const VEC3& that)
	{
		return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << "]";
	}

	struct VEC4 {
		float x, y, z, w;
	};
	inline std::ostream& operator <<(std::ostream& os, const VEC4& that)
	{
		return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << " W=" << that.w << "]";
	}

	struct UV {
		float u, v;
	};
	inline std::ostream& operator <<(std::ostream& os, const UV& that)
	{
		return os << "[U=" << that.u << " V=" << that.v << "]";
	}

	struct ARRAY_UV {
		float u, v;
		float i;
	};
	inline std::ostream& operator <<(std::ostream& os, const ARRAY_UV& that)
	{
		return os << "[U=" << that.u << " V=" << that.v << " I=" << that.i << "]";
	}

	struct POS_COLOR {
		VEC3 position;
		D3DXCOLOR color;
	};

	struct POS_UV {
		VEC3 pos;
		UV uv;
	};
	inline std::ostream& operator <<(std::ostream& os, const POS_UV& that)
	{
		return os << "[POS: " << that.pos << " UV:" << that.uv << "]";
	}

	struct NORMAL_UV_LUV {
		VEC3 normal;
		UV uvDiffuse;
		ARRAY_UV uvLightmap;
	};
	inline std::ostream& operator <<(std::ostream& os, const NORMAL_UV_LUV& that)
	{
		return os << "[NOR:" << that.normal << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
	}

	struct NORMAL_CL_UV_LUV_STATIC_LIGHT {
		VEC3 normal;
		UV uvDiffuse;
		ARRAY_UV uvLightmap;
		D3DXCOLOR colLight;
		VEC3 dirLight;
		float lightSun;
	};
	inline std::ostream& operator <<(std::ostream& os, const NORMAL_CL_UV_LUV_STATIC_LIGHT& that)
	{
		return os << "[NOR:" << that.normal << " COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
	}

	typedef VEC3 VERTEX_POS;
	typedef NORMAL_CL_UV_LUV_STATIC_LIGHT VERTEX_OTHER;

	struct VEC_VERTEX_DATA {
		std::vector<VERTEX_POS> vecPos;
		std::vector<VERTEX_OTHER> vecNormalUv;
	};

	struct Material {
		std::string texBaseColor;

		bool operator==(const Material& other) const
		{
			return (texBaseColor == other.texBaseColor);
		}
	};
}
namespace std
{
	using namespace renderer;
	template <>
	struct hash<Material>
	{
		size_t operator()(const Material& key) const
		{
			size_t res = 17;
			res = res * 31 + hash<string>()(key.texBaseColor);
			return res;
		}
	};
}
namespace renderer
{
	struct VertKey {
		const Material* mat;
		const uint32_t vertIndex;

		const VEC_VERTEX_DATA& get(const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData) const {
			return meshData.find(*this->mat)->second;
		}
		const std::array<VERTEX_POS, 3> getPos(const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData) const {
			auto& vertData = meshData.find(*this->mat)->second.vecPos;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
		const std::array<VERTEX_OTHER, 3> getOther(const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData) const {
			auto& vertData = meshData.find(*this->mat)->second.vecNormalUv;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
	};

	struct BufferSize {
		uint32_t width;
		uint32_t height;

		BufferSize operator*(const float scalar) const {
			return { (uint32_t)((width * scalar) + 0.5f), (uint32_t)((height * scalar) + 0.5f) };
		}
	};

	void release(IUnknown* dx11object);
	void release(const std::vector<IUnknown*>& dx11objects);
	void initViewport(BufferSize& size, D3D11_VIEWPORT* viewport);
}