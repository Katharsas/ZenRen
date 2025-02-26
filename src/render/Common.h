#pragma once

#include <string>
#include <ostream>
#include <vector>

#include "Util.h"
#include "render/Primitives.h"
#include "render/d3d/BlendState.h"

#include "magic_enum.hpp"

//enum DXGI_FORMAT : int;

namespace render
{
	// TODO move to MeshUtil
	constexpr float G_ASSET_RESCALE = 0.01f;

	typedef uint16_t TexId;

	typedef VEC3 VertexPos;

	struct TexInfo {
		// TODO make width/height of type BufferSize?
		uint16_t width = 0;
		uint16_t height = 0;
		uint16_t mipLevels = 0;
		bool hasAlpha = true;

		// compression and color space
		// DXGI_FORMAT
		uint32_t format = 0; // DXGI_FORMAT_UNKNOWN
		bool srgb = true;

		auto operator<=>(const TexInfo&) const = default;

		BufferSize getSize() {
			return { (uint16_t) width, (uint16_t) height };
		}
	};

	// TODO make adjustable for reload, implement level selection and changing vdf path with load button
	// TODO should be based on vertex density by default (make "auto" setting available)
	const float chunkSizePerDim = 145;// optimized for G1, ideally we would first caluclate the bounds of the worldmesh and then use middlepoint and size

	struct ChunkIndex {
		int16_t x;
		int16_t y;

		auto operator<=>(const ChunkIndex&) const = default;
	};

	struct ChunkVertCluster {
		ChunkIndex pos;
		uint32_t vertStartIndex;
	};


	template<typename F>
	concept IS_VERTEX_BASIC = std::is_same_v<F, VertexBasic>;

	//template<typename F>
	//concept IS_VERTEX_BLEND = std::is_same_v<F, VertexBlend>;

	template <typename F>
	concept VERTEX_FEATURE = IS_VERTEX_BASIC<F>;// || IS_VERTEX_BLEND<F>;


	template <VERTEX_FEATURE F>
	struct Verts {
		std::vector<VertexPos> vecPos;
		std::vector<F> vecOther;
	};

	typedef Verts<VertexBasic> VertsBasic;
	//typedef Verts<VertexBlend> VertsBlend;

	constexpr uint16_t PASS_COUNT = 5;

	static_assert(magic_enum::enum_count<BlendType>() == PASS_COUNT);

	enum class ColorSpace {
		LINEAR, SRGB
	};
	
	struct Material {
		TexId texBaseColor;
		BlendType blendType;
		ColorSpace colorSpace;

		auto operator<=>(const Material&) const = default;
	};

	template <VERTEX_FEATURE F>
	using ChunkToVerts = std::unordered_map<ChunkIndex, Verts<F>>;
	
	template <VERTEX_FEATURE F>
	using MatToChunksToVerts = std::unordered_map<Material, ChunkToVerts<F>>;
	using MatToChunksToVertsBasic = MatToChunksToVerts<VertexBasic>;
	//using MatToChunksToVertsBlend = MatToChunksToVerts<VertexBlend>;

	template <VERTEX_FEATURE F>
	using MatToVerts = std::unordered_map<Material, Verts<F>>;
	using MatToVertsBasic = MatToVerts<VertexBasic>;


	template <VERTEX_FEATURE F>
	struct VertsBatch {
		std::vector<ChunkVertCluster> vertClusters;
		std::vector<VertexPos> vecPos;
		std::vector<F> vecOther;
		std::vector<TEX_INDEX> texIndices;
		std::vector<TexId> texIndexedIds;
	};

    // TODO call with [[msvc::forceinline]] if necessary for performance
	template <typename FaceData>
	using GetFace = const FaceData(*) (const Material&, const ChunkIndex&, uint32_t, const VertsBasic&);

	template <typename FaceData, GetFace<FaceData> GetFace>
	void for_each_face(
		const MatToChunksToVertsBasic& data,
		const std::function<void(const FaceData& face)>& func)
	{
		for (const auto& [material, chunkData] : data) {
			for (const auto& [chunkIndex, vertData] : chunkData) {
				for (uint32_t i = 0; i < vertData.vecPos.size(); i += 3) {
					const auto faceData = GetFace(material, chunkIndex, i, vertData);
					func(faceData);
				}
			}
		}
	}
}

namespace std
{
	using namespace render;

	template <>
	struct hash<TexInfo>
	{
		size_t operator()(const TexInfo& key) const
		{
			size_t res = 0;
			util::hashCombine(res, key.width);
			util::hashCombine(res, key.height);
			util::hashCombine(res, key.mipLevels);
			util::hashCombine(res, key.hasAlpha);
			util::hashCombine(res, key.format);
			return res;
		}
	};

	template <>
	struct hash<Material>
	{
		size_t operator()(const Material& key) const
		{
			size_t res = 0;
			util::hashCombine(res, key.texBaseColor);
			util::hashCombine(res, key.blendType);
			util::hashCombine(res, key.colorSpace);
			return res;
		}
	};

	template <>
	struct hash<ChunkIndex>
	{
		size_t operator()(const ChunkIndex& key) const
		{
			size_t res = 0;
			util::hashCombine(res, key.x);
			util::hashCombine(res, key.y);
			return res;
		}
	};
}
namespace render
{
	struct VertKey {
		// this should be ok because elements in an unordered_map never move
		const Material mat;
		const ChunkIndex chunkIndex;

		// we assume the vert vector is not modified during lifetime of index
		const uint32_t vertIndex;


		const VertsBasic& get(const MatToChunksToVertsBasic& meshData) const
		{
			return meshData.find(this->mat)->second.find(this->chunkIndex)->second;
		}
		const std::array<VertexPos, 3> getPos(const MatToChunksToVertsBasic& meshData) const
		{
			auto& vertData = get(meshData).vecPos;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
		const std::array<VertexBasic, 3> getOther(const MatToChunksToVertsBasic& meshData) const
		{
			auto& vertData = get(meshData).vecOther;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
	};
}