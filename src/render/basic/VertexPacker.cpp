#include "stdafx.h"
#include "VertexPacker.h"

namespace render
{
	// Packed Normals:
	// Store signs of all 3 components, store x and z fixed-point values, y is recreated (x^2 + y^2 + z^2 = 1)
	constexpr static uint32_t normalComponentBits = 14;
	constexpr static uint32_t normalComponentMask = (1 << normalComponentBits) - 1;

	// Packed UVs:
	// Custom floating point format where exponent is shared between u and v fixed-point values (mantissa)
	constexpr static uint32_t uvExponentBits = 4;
	constexpr static uint32_t uvComponentBits = 14;
	constexpr static uint32_t uvExponentMask = (1 << uvExponentBits) - 1;
	constexpr static uint32_t uvComponentMask = (1 << uvComponentBits) - 1;
	constexpr static uint32_t scale = (1 << (uvComponentBits - 1));

	uint32_t packNormal(Vec3 raw)
	{
		// TODO it should not be necessary to extract and pack x/z sign manually 
		uint32_t xSign = (raw.x < 0.f) ? 1 : 0;
		uint32_t ySign = (raw.y < 0.f) ? 1 : 0;
		uint32_t zSign = (raw.z < 0.f) ? 1 : 0;

		float xAbs = std::abs(raw.x);
		float zAbs = std::abs(raw.z);

		uint32_t xQuant = std::min((uint32_t)(xAbs * normalComponentMask + 0.5f), normalComponentMask);
		uint32_t zQuant = std::min((uint32_t)(zAbs * normalComponentMask + 0.5f), normalComponentMask);

		return (ySign << (normalComponentBits * 2 + 2))
			| (zSign << (normalComponentBits * 2 + 1)) | (zQuant << (normalComponentBits + 1))
			| (xSign << normalComponentBits) | xQuant;
	}

	Vec3 unpackNormal(uint32_t packed)
	{
		uint32_t xQuant = packed & normalComponentMask;
		uint32_t xSign = (packed >> normalComponentBits) & 1;
		uint32_t zQuant = (packed >> normalComponentBits + 1) & normalComponentMask;
		uint32_t zSign = (packed >> normalComponentBits * 2 + 1) & 1;
		uint32_t ySign = (packed >> normalComponentBits * 2 + 2) & 1;

		constexpr float invMaxVal = 1.f / normalComponentMask;
		float xAbs = xQuant * invMaxVal;
		float zAbs = zQuant * invMaxVal;

		float yAbsSquared = 1.0f - xAbs * xAbs - zAbs * zAbs;
		float yAbs = (yAbsSquared > 0.f) ? std::sqrt(yAbsSquared) : 0.f;

		return {
			xSign ? -xAbs : xAbs,
			ySign ? -yAbs : yAbs,
			zSign ? -zAbs : zAbs
		};
	}

	uint32_t packUv(Uv raw)
	{
		uint32_t uCeil = (uint32_t)std::ceil(std::abs(raw.u));
		uint32_t vCeil = (uint32_t)std::ceil(std::abs(raw.v));
		uint32_t ceil = std::max(uCeil, vCeil);
		uint32_t exponent = std::max(std::bit_width(ceil), 0);
		if (exponent > uvExponentMask) {
			LOG(WARNING) << "Failed to compress UV, value too large! Max magnitude allowed: " << (1 << uvExponentMask);
		}
		uint32_t factor = 1 << exponent;
		Uv uvNorm = mul(raw, 1.f * scale / factor);
		// cast to int preserves sign bit, but it is extended into all high bits (2's complement), so we must clear those with mask
		return exponent << (uvComponentBits * 2)
			| ((int32_t)std::lround(uvNorm.u) & uvComponentMask) << uvComponentBits
			| ((int32_t)std::lround(uvNorm.v) & uvComponentMask);
	}

	Uv unpackUv(uint32_t packed)
	{
		uint32_t exponent = ((packed >> (uvComponentBits * 2)) & uvExponentMask);
		uint32_t uBits = (packed >> uvComponentBits) & uvComponentMask;
		uint32_t vBits = packed & uvComponentMask;
		constexpr uint32_t signMask = 1 << (uvComponentBits - 1);
		constexpr uint32_t signExtend = ~uvComponentMask;
		// extend sign (1 in highest bit) to all higher bits (2's complement)
		Uv uvNorm = {
			(int32_t)(uBits & signMask ? uBits | signExtend : uBits),
			(int32_t)(vBits & signMask ? vBits | signExtend : vBits),
		};
		float factor = 1 << exponent;
		return mul(uvNorm, factor / scale);
	}
}