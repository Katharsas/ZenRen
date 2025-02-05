#pragma once

#include "Primitives.h"
#include <string>
#include <ostream>
#include <vector>

//enum DXGI_FORMAT : int;

namespace render
{
	// TODO move to MeshUtil
	constexpr float G_ASSET_RESCALE = 0.01f;

	typedef uint16_t TexId;

	typedef VEC3 VERTEX_POS;
	typedef NORMAL_CL_UV_LUV_STATIC_LIGHT VERTEX_OTHER;

	struct TexInfo {
		// TODO make width/height of type BufferSize?
		uint16_t width = 0;
		uint16_t height = 0;
		uint16_t mipLevels = 0;
		bool hasAlpha = true;

		// compression and color space
		// DXGI_FORMAT
		uint32_t format = 0; // DXGI_FORMAT_UNKNOWN

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

	struct VEC_VERTEX_DATA {
		std::vector<VERTEX_POS> vecPos;
		std::vector<VERTEX_OTHER> vecOther;
	};

	struct Material {
		TexId texBaseColor;

		auto operator<=>(const Material&) const = default;
	};

	typedef std::unordered_map<Material, VEC_VERTEX_DATA> VERTEX_DATA_BY_MAT;

	typedef std::unordered_map<ChunkIndex, VEC_VERTEX_DATA> VERT_CHUNKS;
	typedef std::unordered_map<Material, VERT_CHUNKS> VERT_CHUNKS_BY_MAT;

	struct VEC_VERTEX_DATA_BATCH {
		std::vector<ChunkVertCluster> vertClusters;
		std::vector<VERTEX_POS> vecPos;
		std::vector<VERTEX_OTHER> vecOther;
		std::vector<TEX_INDEX> texIndices;
		std::vector<TexId> texIndexedIds;
	};

    // TODO call with [[msvc::forceinline]] if necessary for performance
	template <typename FaceData>
	using GetFace = const FaceData(*) (const Material&, const ChunkIndex&, uint32_t, const VEC_VERTEX_DATA&);

	template <typename FaceData, GetFace<FaceData> GetFace>
	void for_each_face(
		const VERT_CHUNKS_BY_MAT& data,
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
	struct hash<ChunkIndex>
	{
		size_t operator()(const ChunkIndex& key) const
		{
			size_t res = 0;
			hashCombine(res, key.x);
			hashCombine(res, key.y);
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


		const VEC_VERTEX_DATA& get(const VERT_CHUNKS_BY_MAT& meshData) const
		{
			return meshData.find(this->mat)->second.find(this->chunkIndex)->second;
		}
		const std::array<VERTEX_POS, 3> getPos(const VERT_CHUNKS_BY_MAT& meshData) const
		{
			auto& vertData = get(meshData).vecPos;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
		const std::array<VERTEX_OTHER, 3> getOther(const VERT_CHUNKS_BY_MAT& meshData) const
		{
			auto& vertData = get(meshData).vecOther;
			return { vertData[this->vertIndex], vertData[this->vertIndex + 1], vertData[this->vertIndex + 2] };
		}
	};
}