#pragma once

#include <string>
#include <ostream>
#include <vector>

#include "Util.h"
#include "render/Primitives.h"

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

		BufferSize getSize() const {
			return { (uint16_t)width, (uint16_t)height };
		}

		struct Hash
		{
			size_t operator()(const TexInfo& key) const
			{
				size_t res = 0;
				::util::hashCombine(res, key.width);
				::util::hashCombine(res, key.height);
				::util::hashCombine(res, key.mipLevels);
				::util::hashCombine(res, key.hasAlpha);
				::util::hashCombine(res, key.format);
				::util::hashCombine(res, key.srgb);
				return res;
			}
		};
	};
	inline std::ostream& operator <<(std::ostream& os, const TexInfo& that)
	{
		std::string width = util::leftPad(std::to_string(that.width), 4, ' ');
		std::string height = util::leftPad(std::to_string(that.height), 4, ' ');
		std::string mips = util::leftPad(std::to_string(that.mipLevels), 2, ' ');
		return os << "[W: " << width << ", H: " << height << ", M: " << mips
			<< (that.srgb ? ", srgb  " : ", linear")
			<< (that.hasAlpha ? ", alpha" : "")
			<< ", F: " << that.format << "]";
	}
	} namespace std { template <> struct hash<render::TexInfo> : render::TexInfo::Hash {}; } namespace render {

	// TODO make adjustable for reload, implement level selection and changing vdf path with load button
	// TODO should be based on vertex density by default (make "auto" setting available)
	const float chunkSizePerDim = 145;// optimized for G1, ideally we would first caluclate the bounds of the worldmesh and then use middlepoint and size

	struct ChunkIndex {
		int16_t x;
		int16_t y;

		auto operator<=>(const ChunkIndex&) const = default;

		struct Hash
		{
			size_t operator()(const ChunkIndex& key) const
			{
				size_t res = 0;
				::util::hashCombine(res, key.x);
				::util::hashCombine(res, key.y);
				return res;
			}
		};
	};
	} namespace std { template <> struct hash<render::ChunkIndex> : render::ChunkIndex::Hash {}; } namespace render {

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
		bool useIndices;
		std::vector<VertexIndex> vecIndex;
		std::vector<VertexPos> vecPos;
		std::vector<F> vecOther;
	};

	typedef Verts<VertexBasic> VertsBasic;
	//typedef Verts<VertexBlend> VertsBlend;

	
	// TODO disable depth writing
	// TODO if face order can result in different outcome, we would need to sort BLEND_ALPHA and BLEND_FACTOR faces
	// by distance to camera theoretically (see waterfall near G2 start)
	// To prevent having to sort all water faces:
	// While above water, render water before (sorted) alpha/factor blenjd faces, while below last
	enum class BlendType : uint8_t {
		NONE = 0,         // normal opaque or alpha-tested, shaded
		ADD = 1,          // alpha channel additive blending
		MULTIPLY = 2,     // color multiplication, linear color textures, no shading
		BLEND_ALPHA = 3,  // alpha channel blending, ??
		BLEND_FACTOR = 4, // fixed factor blending (water), shaded (?)
	};
	
	constexpr uint8_t BLEND_TYPE_COUNT = magic_enum::enum_count<BlendType>();

	enum class ColorSpace {
		LINEAR, SRGB
	};
	
	struct Material {
		TexId texBaseColor;
		BlendType blendType;
		ColorSpace colorSpace;

		auto operator<=>(const Material&) const = default;

		struct Hash
		{
			size_t operator()(const Material& key) const
			{
				size_t res = 0;
				::util::hashCombine(res, key.texBaseColor);
				::util::hashCombine(res, key.blendType);
				::util::hashCombine(res, key.colorSpace);
				return res;
			}
		};
	};
	} namespace std { template <> struct hash<render::Material> : render::Material::Hash {}; } namespace render {

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
		std::vector<VertexIndex> vecIndex;
		std::vector<VertexPos> vecPos;
		std::vector<F> vecOther;
		// TODO either remove vec prefix everywhere and use plural or not consistently
		std::vector<TEX_INDEX> texIndices;
		std::vector<TexId> texIndexedIds;
	};


	template <typename VERT_TYPE, VERTEX_FEATURE F>
	const std::vector<VERT_TYPE>& GetVertVec(const Verts<F>& verts)
	{
		if constexpr (std::is_same_v<VERT_TYPE, VertexPos>) {
			return verts.vecPos;
		}
		if constexpr (std::is_same_v<VERT_TYPE, F>) {
			return verts.vecOther;
		}
	}

	template <typename VERT_TYPE, VERTEX_FEATURE F>
	const std::array<VERT_TYPE, 3> GetFace(const Verts<F>& verts, uint32_t index)
	{
		const std::vector<VERT_TYPE>& vec = GetVertVec<VERT_TYPE, F>(verts);
		if (verts.useIndices) {
			return {
				vec[verts.vecIndex[index]],
				vec[verts.vecIndex[index + 1]],
				vec[verts.vecIndex[index + 2]]
			};
		} else {
			return {
				vec[index],
				vec[index + 1],
				vec[index + 2]
			};
		};
	}

	struct VertKey {
		const Material mat;
		const ChunkIndex chunkIndex;
		const uint32_t vertIndex; // we assume the vert vector is not modified during lifetime of index

		// TODO maybe the whole struct should be templated instead of every single method
		template <VERTEX_FEATURE F>
		const VertsBasic& get(const MatToChunksToVerts<F>& meshData) const
		{
			return meshData.find(this->mat)->second.find(this->chunkIndex)->second;
		}
		template <VERTEX_FEATURE F>
		const std::array<VertexPos, 3> getPos(const MatToChunksToVerts<F>& meshData) const
		{
			return GetFace<VertexPos, F>(get(meshData), vertIndex);
		}
		template <VERTEX_FEATURE F>
		const std::array<VertexBasic, 3> getOther(const MatToChunksToVerts<F>& meshData) const
		{
			return GetFace<F, F>(get(meshData), vertIndex);
		}
	};

	void forEachFace(const MatToChunksToVertsBasic& data, const std::function<void(const VertKey& vertKey)>& func);
}