#pragma once

#include "dx11.h"

#include <string>
#include <ostream>

namespace renderer
{
	struct VEC3 {
		float x, y, z;

		friend std::ostream& operator <<(std::ostream& os, const VEC3& that)
		{
			return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << "]";
		}
	};
	struct UV {
		float u, v;

		friend std::ostream& operator <<(std::ostream& os, const UV& that)
		{
			return os << "[U=" << that.u << " V=" << that.v << "]";
		}
	};
	struct ARRAY_UV {
		float u, v;
		float i;

		friend std::ostream& operator <<(std::ostream& os, const ARRAY_UV& that)
		{
			return os << "[U=" << that.u << " V=" << that.v << " I=" << that.i << "]";
		}
	};

	struct POS_COLOR {
		VEC3 position;
		D3DXCOLOR color;
	};
	struct POS_UV {
		VEC3 pos;
		UV uv;

		friend std::ostream& operator <<(std::ostream& os, const POS_UV& that)
		{
			return os << "[POS: " << that.pos << " UV:" << that.uv << "]";
		}
	};
	struct POS_NORMAL_UV {
		VEC3 pos;
		VEC3 normal;
		UV uvDiffuse;
		ARRAY_UV uvLightmap;

		friend std::ostream& operator <<(std::ostream& os, const POS_NORMAL_UV& that)
		{
			return os << "[POS:" << that.pos << " NOR:" << that.normal << " UV_DIFF:" << that.uvDiffuse << "]";
		}
	};
	struct POS_NORMAL_UV_COL {
		VEC3 pos;
		VEC3 normal;
		UV uv;
		D3DXCOLOR color;

		friend std::ostream& operator <<(std::ostream& os, const POS_NORMAL_UV_COL& that)
		{
			return os << "[POS:" << that.pos << " NOR:" << that.normal << " UV:" << that.uv << " LIGHT:" << that.color << "]";
		}
	};
	struct WORLD_VERTEX {
		VEC3 pos;
		VEC3 normal;
		UV uvDiffuse;
		ARRAY_UV uvLightmap;
		//D3DXCOLOR colorLightmap;

		friend std::ostream& operator <<(std::ostream& os, const WORLD_VERTEX& that)
		{
			return os << "[POS:" << that.pos << " NOR:" << that.normal << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
		}
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