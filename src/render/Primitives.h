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
template <typename V> concept XYZ = requires(V vec) {
	{ vec.x } -> std::convertible_to<float>;
	{ vec.y } -> std::convertible_to<float>;
	{ vec.z } -> std::convertible_to<float>;
};
template <typename V> concept XY = !XYZ<V> && requires(V vec) {
	{ vec.x } -> std::convertible_to<float>;
	{ vec.y } -> std::convertible_to<float>;
};

struct Color {
	union {
		struct { float r, g, b, a; };
		float vec[4];
	};
	Color() {};
	Color(uint32_t argb);
	Color(float r, float g, float b, float a);

	template<RGBA COLOR>
	Color(COLOR rgba) {
		r = rgba.r;
		g = rgba.g;
		b = rgba.b;
		a = rgba.a;
	}
};
inline std::ostream& operator <<(std::ostream& os, const Color& that)
{
	return os << "[R:" << that.r << " G:" << that.g << " B:" << that.b << " A:" << that.a << "]";
}
static_assert(RGBA<Color>);

struct Uv {
	union {
		struct { float u, v; };
		float vec[2];
	};
};
inline std::ostream& operator <<(std::ostream& os, const Uv& that)
{
	return os << "[U=" << that.u << " V=" << that.v << "]";
}

struct Vec2 {
	union {
		struct { float x, y; };
		float vec[2];
	};
};
inline std::ostream& operator <<(std::ostream& os, const Vec2& that)
{
	return os << "[X=" << that.x << " Y=" << that.y << "]";
}
static_assert(XY<Vec2>);

struct Vec3 {
	union {
		struct { float x, y, z; };
		float vec[3];
	};
};
inline std::ostream& operator <<(std::ostream& os, const Vec3& that)
{
	return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << "]";
}
static_assert(XYZ<Vec3>);

struct Vec4 {
	union {
		struct { float x, y, z, w; };
		float vec[4];
	};
};
inline std::ostream& operator <<(std::ostream& os, const Vec4& that)
{
	return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << " W=" << that.w << "]";
}

struct Uvi {
	float u, v;
	float i;
};
inline std::ostream& operator <<(std::ostream& os, const Uvi& that)
{
	return os << "[U=" << that.u << " V=" << that.v << " I=" << that.i << "]";
}

struct PosUv {
	Vec3 pos;
	Uv uv;
};
inline std::ostream& operator <<(std::ostream& os, const PosUv& that)
{
	return os << "[POS: " << that.pos << " UV:" << that.uv << "]";
}


// TODO create union of uvLightmap + colLight with single bit flag ti differentiate
// TODO move uvDiffuse into TexIndex
// TODO define input layout in structs
struct VertexBasic {
	Vec3 normal;
	Uv uvDiffuse;
	Uvi uvLightmap;
	Color colLight;
	Vec3 dirLight;
	float lightSun;
};
inline std::ostream& operator <<(std::ostream& os, const VertexBasic& that)
{
	return os << "[NOR:" << that.normal << " COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
}

//struct VertexBlend {
//	Vec3 normal;
//	Uv uvDiffuse;
//	Uvi uvLightmap;
//	Color colLight;
//	Vec3 dirLight;
//	float lightSun;
//	BlendType blendType;
//};
//inline std::ostream& operator <<(std::ostream& os, const VertexBlend& that)
//{
//	return os << "[NOR:" << that.normal << " COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_DIFF:" << that.uvDiffuse << " UV_LM:" << that.uvLightmap << "]";
//}

using VertexIndex = uint32_t;

using TexIndex = uint32_t;

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
	Color fromSRGB(const Color color);
	Color from4xUint8(uint8_t const* const rgba);
	Color greyscale(const float channel);
	Color multiplyColor(Color color, const float factor);

	template<typename T> T add(const T& vec1, const T& vec2);
	template<typename T> T sub(const T& vec1, const T& vec2);
	template<typename T> T mul(const T& vec, float scalar);
}