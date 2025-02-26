#pragma once

#include <ostream>
#include <concepts>
#include <type_traits>

template <typename C> concept RGBA = requires(C color) {
	{ color.r } -> std::convertible_to<uint32_t>;
	{ color.g } -> std::convertible_to<uint32_t>;
	{ color.b } -> std::convertible_to<uint32_t>;
	{ color.a } -> std::convertible_to<uint32_t>;
};
template <typename V> concept Vec3 = requires(V vec) {
	{ vec.x } -> std::convertible_to<float>;
	{ vec.y } -> std::convertible_to<float>;
	{ vec.z } -> std::convertible_to<float>;
};
template <typename V> concept Vec2 = !Vec3<V> && requires(V vec) {
	{ vec.x } -> std::convertible_to<float>;
	{ vec.y } -> std::convertible_to<float>;
};

struct COLOR {
	union {
		struct { float r, g, b, a; };
		float vec[4];
	};
	COLOR() {};
	COLOR(uint32_t argb);
	COLOR(float r, float g, float b, float a);

	template<RGBA Color>
	COLOR(Color rgba) {
		r = rgba.r;
		g = rgba.g;
		b = rgba.b;
		a = rgba.a;
	}
};
inline std::ostream& operator <<(std::ostream& os, const COLOR& that)
{
	return os << "[R:" << that.r << " G:" << that.g << " B:" << that.b << " A:" << that.a << "]";
}
static_assert(RGBA<COLOR>);

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
static_assert(Vec2<VEC2>);

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
static_assert(Vec3<VEC3>);

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
struct VertexBasic {
	VEC3 normal;
	UV uvDiffuse;
	ARRAY_UV uvLightmap;
	COLOR colLight;
	VEC3 dirLight;
	float lightSun;
};
inline std::ostream& operator <<(std::ostream& os, const VertexBasic& that)
{
	return os << "[NOR:" << that.normal << " COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
}

//struct VertexBlend {
//	VEC3 normal;
//	UV uvDiffuse;
//	ARRAY_UV uvLightmap;
//	COLOR colLight;
//	VEC3 dirLight;
//	float lightSun;
//	BlendType blendType;
//};
//inline std::ostream& operator <<(std::ostream& os, const VertexBlend& that)
//{
//	return os << "[NOR:" << that.normal << " COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
//}

typedef uint32_t TEX_INDEX;

namespace render {

	struct BufferSize {
		uint16_t width;
		uint16_t height;

		BufferSize operator*(const float scalar) const
		{
			return { (uint16_t)((width * scalar) + 0.5f), (uint16_t)((height * scalar) + 0.5f) };
		}
		bool operator!=(const BufferSize& other) const
		{
			return width != other.width || height != other.height;
		}
		operator std::tuple<uint16_t&, uint16_t&>()
		{
			return std::tuple<uint16_t&, uint16_t&>{width, height};
		}
	};
	inline std::ostream& operator <<(std::ostream& os, const BufferSize& that)
	{
		return os << "[W:" << that.width << " H:" << that.height << "]";
	}

	float fromSRGB(const float channel);
	COLOR fromSRGB(const COLOR color);
	COLOR from4xUint8(uint8_t const* const rgba);
	COLOR greyscale(const float channel);
	COLOR multiplyColor(COLOR color, const float factor);

	template<typename T> T add(const T& vec1, const T& vec2);
	template<typename T> T sub(const T& vec1, const T& vec2);
	template<typename T> T mul(const T& vec, float scalar);
}