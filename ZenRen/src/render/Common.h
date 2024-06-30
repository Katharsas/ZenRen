#pragma once

#include "dx11.h"
#include "Primitives.h"

#include <string>
#include <ostream>
#include <vector>

namespace render
{
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

	typedef std::unordered_map<Material, VEC_VERTEX_DATA> BATCHED_VERTEX_DATA;
}
namespace std
{
	using namespace render;
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
namespace render
{
	struct VertKey {
		const Material* mat;// this should be ok because elements in an unordered_map never move
		const uint32_t vertIndex;

		const VEC_VERTEX_DATA& get(const BATCHED_VERTEX_DATA& meshData) const {
			return meshData.find(*this->mat)->second;
		}
		const std::array<VERTEX_POS, 3> getPos(const BATCHED_VERTEX_DATA& meshData) const {
			auto& vertData = meshData.find(*this->mat)->second.vecPos;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
		const std::array<VERTEX_OTHER, 3> getOther(const BATCHED_VERTEX_DATA& meshData) const {
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