#pragma once

#include "dx11.h"
#include "Primitives.h"

#include <string>
#include <ostream>
#include <vector>

namespace render
{
	// TODO move to MeshUtil
	constexpr float G_ASSET_RESCALE = 0.01f;

	typedef uint16_t TexId;

	typedef VEC3 VERTEX_POS;
	typedef NORMAL_CL_UV_LUV_STATIC_LIGHT VERTEX_OTHER;

	struct TexInfo {
		uint16_t width = -1;
		uint16_t height = -1;
		uint16_t mipLevels = -1;
		bool hasAlpha = true;

		// compression and color space
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

		auto operator<=>(const TexInfo&) const = default;
	};

	struct VEC_VERTEX_DATA {
		std::vector<VERTEX_POS> vecPos;
		std::vector<VERTEX_OTHER> vecNormalUv;
	};

	struct VEC_VERTEX_DATA_BATCH {
		std::vector<VERTEX_POS> vecPos;
		std::vector<VERTEX_OTHER> vecNormalUv;
		std::vector<TEX_INDEX> texIndices;
		std::vector<TexId> texIndexedIds;
	};

	struct Material {
		TexId texBaseColor;

		auto operator<=>(const Material&) const = default;
	};

	typedef std::unordered_map<Material, VEC_VERTEX_DATA> VERTEX_DATA_BY_MAT;

	// the boost hash_combine function
	template <class Hashable>
	inline void hashCombine(std::size_t& hash, const Hashable& value)
	{
		std::hash<Hashable> hasher;
		hash ^= hasher(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	}
}
namespace std
{
	using namespace render;

	template <>
	struct hash<Material>
	{
		size_t operator()(const Material& key) const
		{
			size_t res = 0;
			hashCombine(res, key.texBaseColor);
			return res;
		}
	};

	template <>
	struct hash<TexInfo>
	{
		size_t operator()(const TexInfo& key) const
		{
			size_t res = 0;
			hashCombine(res, key.width);
			hashCombine(res, key.height);
			hashCombine(res, key.mipLevels);
			hashCombine(res, key.hasAlpha);
			hashCombine(res, key.format);
			return res;
		}
	};
}
namespace render
{
	struct VertKey {
		const Material* mat;// this should be ok because elements in an unordered_map never move
		const uint32_t vertIndex;

		const VEC_VERTEX_DATA& get(const VERTEX_DATA_BY_MAT& meshData) const {
			return meshData.find(*this->mat)->second;
		}
		const std::array<VERTEX_POS, 3> getPos(const VERTEX_DATA_BY_MAT& meshData) const {
			auto& vertData = meshData.find(*this->mat)->second.vecPos;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
		const std::array<VERTEX_OTHER, 3> getOther(const VERTEX_DATA_BY_MAT& meshData) const {
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
	
	float fromSRGB(const float channel);
	D3DXCOLOR fromSRGB(const D3DXCOLOR color);
	D3DXCOLOR greyscale(const float channel);
	D3DXCOLOR multiplyColor(D3DXCOLOR color, const float factor);

	template<typename T> T add(const T& vec1, const T& vec2);
	template<typename T> T mul(const T& vec, float scalar);
}