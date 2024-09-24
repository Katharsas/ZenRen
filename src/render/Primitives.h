#pragma once

#include "dx11.h"
#include <ostream>

inline std::ostream& operator <<(std::ostream& os, const D3DXCOLOR& that)
{
	return os << "[R:" << that.r << " G:" << that.g << " B:" << that.b << " A:" << that.a << "]";
}

struct UV {
	union {
		struct { float u, v; };
		float vec[2];
	};
};
inline std::ostream& operator <<(std::ostream& os, const UV& that)
{
	return os << "[U=" << that.u << " V=" << that.v << "]";
}

struct VEC2 {
	union {
		struct { float x, y; };
		float vec[2];
	};
};
inline std::ostream& operator <<(std::ostream& os, const VEC2& that)
{
	return os << "[X=" << that.x << " Y=" << that.y << "]";
}

struct VEC3 {
	union {
		struct { float x, y, z; };
		float vec[3];
	};
};
inline std::ostream& operator <<(std::ostream& os, const VEC3& that)
{
	return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << "]";
}

struct VEC4 {
	union {
		struct { float x, y, z, w; };
		float vec[4];
	};
};
inline std::ostream& operator <<(std::ostream& os, const VEC4& that)
{
	return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << " W=" << that.w << "]";
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

// TODO create union of uvLightmap + colLight with single bit flag ti differentiate
// TODO move uvDiffuse into TEX_INDEX
// TODO define input layout in structs
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

typedef uint32_t TEX_INDEX;