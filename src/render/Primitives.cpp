#include "stdafx.h"
#include "Primitives.h"

COLOR::COLOR(uint32_t argb) {
	const float f = 1.0f / 255.0f;
	r = f * (float)(uint8_t)(argb >> 16);
	g = f * (float)(uint8_t)(argb >> 8);
	b = f * (float)(uint8_t)(argb >> 0);
	a = f * (float)(uint8_t)(argb >> 24);
}
COLOR::COLOR(float r_, float g_, float b_, float a_) {
	r = r_;
	g = g_;
	b = b_;
	a = a_;
}

namespace render {
	float fromSRGB(const float channel) {
		return (channel <= 0.04045f) ? (channel / 12.92f) : pow((channel + 0.055f) / 1.055f, 2.4f);
	}

	COLOR fromSRGB(const COLOR color) {
		return COLOR(
			fromSRGB(color.r),
			fromSRGB(color.g),
			fromSRGB(color.b),
			color.a);
	}

	COLOR greyscale(const float channel) {
		return COLOR(channel, channel, channel, 1);
	}

	COLOR multiplyColor(COLOR color, const float factor) {
		return COLOR(
			color.r * factor,
			color.g * factor,
			color.b * factor,
			color.a);
	}

	template<typename T>
	float* begin(T& t) {
		return t.vec;
	}
	template<typename T>
	const float* begin(const T& t) {
		return t.vec;
	}
	template<typename T>
	const float* end(const T& t) {
		return t.vec + std::size(t.vec);
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

	// instantiate template functions for the types we want to support (needed because templates are not in header)
	template COLOR add(const COLOR& vec1, const COLOR& vec2);
	template COLOR sub(const COLOR& vec1, const COLOR& vec2);
	template COLOR mul(const COLOR& vec, float scalar);

	template UV add(const UV& vec1, const UV& vec2);
	template UV mul(const UV& vec, float scalar);
}