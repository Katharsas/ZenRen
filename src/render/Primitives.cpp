#include "stdafx.h"
#include "Primitives.h"

#include <numeric>

Color::Color(uint32_t argb) {
	const float f = 1.0f / 255.0f;
	r = f * (float)(uint8_t)(argb >> 16);
	g = f * (float)(uint8_t)(argb >> 8);
	b = f * (float)(uint8_t)(argb >> 0);
	a = f * (float)(uint8_t)(argb >> 24);
}
Color::Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}


namespace render {
	float fromSRGB(const float channel)
	{
		return (channel <= 0.04045f) ? (channel / 12.92f) : pow((channel + 0.055f) / 1.055f, 2.4f);
	}

	Color fromSRGB(const Color color)
	{
		return Color(
			fromSRGB(color.r),
			fromSRGB(color.g),
			fromSRGB(color.b),
			color.a);
	}

	Color from4xUint8(uint8_t const * const rgba)
	{
		const float f = 1.0f / 255.0f;
		return Color(rgba[0] * f, rgba[1] * f, rgba[2] * f, rgba[3] * f);
	}

	Color greyscale(const float channel) {
		return Color(channel, channel, channel, 1);
	}

	Color multiplyColor(Color color, const float factor) {
		return Color(
			color.r * factor,
			color.g * factor,
			color.b * factor,
			color.a);
	}

	template<typename T>
	float* begin(const T& t) {
		return (float*) t.vec;
	}
	template<typename T>
	float* end(const T& t) {
		return (float*) t.vec + std::size(t.vec);
	}

	template<typename T>
	T add(const T& vec1, const T& vec2) {
		T result;
		std::transform(begin(vec1), end(vec1), begin(vec2), begin(result), std::plus<float>());
		return result;
	}
	template<typename T>
	T sub(const T& vec1, const T& vec2) {
		T result;
		std::transform(begin(vec1), end(vec1), begin(vec2), begin(result), std::minus<float>());
		return result;
	}
	template<typename T>
	T mul(const T& vec, float scalar) {
		T result;
		std::transform(begin(vec), end(vec), begin(result), std::bind(std::multiplies<float>(), std::placeholders::_1, scalar));
		return result;
	}
	template<typename T>
	float lengthSq(const T& vec) {
		T squares;
		std::transform(begin(vec), end(vec), begin(squares), std::bind(std::multiplies<float>(), std::placeholders::_1, std::placeholders::_1));
		return std::accumulate(begin(squares), end(squares), 0);
	}

	// instantiate template functions for the types we want to support (needed because templates are not in header)
	template Color add(const Color& vec1, const Color& vec2);
	template Color sub(const Color& vec1, const Color& vec2);
	template Color mul(const Color& vec, float scalar);

	template Uv add(const Uv& vec1, const Uv& vec2);
	template Uv mul(const Uv& vec, float scalar);

	template Vec2 sub(const Vec2& vec1, const Vec2& vec2);
	template float lengthSq(const Vec2& vec);
}