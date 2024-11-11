#pragma once

#include <ostream>

struct COLOR {
	union {
		struct { float r, g, b, a; };
		float vec[4];
	};
	COLOR() {};
	COLOR(uint32_t argb);
	COLOR(float r, float g, float b, float a);
};
inline std::ostream& operator <<(std::ostream& os, const COLOR& that)
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
	COLOR color;
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
	COLOR colLight;
	VEC3 dirLight;
	float lightSun;
};
inline std::ostream& operator <<(std::ostream& os, const NORMAL_CL_UV_LUV_STATIC_LIGHT& that)
{
	return os << "[NOR:" << that.normal << " COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
}

typedef uint32_t TEX_INDEX;

namespace render {

	struct BufferSize {
		uint32_t width;
		uint32_t height;

		BufferSize operator*(const float scalar) const
		{
			return { (uint32_t)((width * scalar) + 0.5f), (uint32_t)((height * scalar) + 0.5f) };
		}
	};
	inline std::ostream& operator <<(std::ostream& os, const BufferSize& that)
	{
		return os << "[W:" << that.width << " H:" << that.height << "]";
	}

	float fromSRGB(const float channel);
	COLOR fromSRGB(const COLOR color);
	COLOR greyscale(const float channel);
	COLOR multiplyColor(COLOR color, const float factor);

	template<typename T> T add(const T& vec1, const T& vec2);
	template<typename T> T sub(const T& vec1, const T& vec2);
	template<typename T> T mul(const T& vec, float scalar);
}