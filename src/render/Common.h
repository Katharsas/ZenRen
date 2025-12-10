#pragma once

#include <string>
#include <ostream>
#include <vector>

#include "Util.h"
#include "render/Primitives.h"
#include "render/Grid.h"

#include "magic_enum.hpp"

namespace render
{
	// TODO move to MeshUtil
	constexpr float G_ASSET_RESCALE = 0.01f;

	constexpr bool COMPRESS_VERTEX_ATTRIBUTES = true;

	using TexId = uint16_t;

	using VertexPos = Vec3;
	template <> inline VertexAttributes inputLayout<VertexPos>() {
		return {
			{  Type::FLOAT_3, Semantic::POSITION }
		};
	}

	// TODO maybe we can have a constexpr bool QUANTIZE that dynamically changes VertexPos struct to be quantized or not
	// and use if constexpr to assign to it (or have a setter function that takes unquantized always)
	// -> do we need to turn VertexPos from typedef into actual struct?? maybe not necessary?
	// maybe try with normalUv first
	struct VertexPosQuantized {
		uint64_t pos;
	};
	template <> inline VertexAttributes inputLayout<VertexPosQuantized>() {
		return {
			{  Type::UINT_2, Semantic::POSITION }
		};
	}

	template <bool IS_COMPRESSED>
	struct VertexNorUvTemplate {};

	template <> struct VertexNorUvTemplate<false> {
	private:
		Vec3 m_normal;// quantize to 3 signs + 2*14 bit x/y, reconstruct z
		Uv m_uvDiffuse;// 12 bit per component? -> 24 bit, remaining: 8 bit for scaling factor (sign + 7), assert max values fit
	public:
		VertexNorUvTemplate() {};
		VertexNorUvTemplate(Vec3 normal, Uv uvDiffuse) : m_normal(normal), m_uvDiffuse(uvDiffuse) {};
		Vec3 normal() const { return m_normal; }
		Uv uvDiffuse() const { return m_uvDiffuse; }
	};
	template <> inline VertexAttributes inputLayout<VertexNorUvTemplate<false>>() {
		return {
			{ Type::FLOAT_3, Semantic::NORMAL },
			{ Type::FLOAT_2, Semantic::TEXCOORD },
		};
	}

	template <> struct VertexNorUvTemplate<true> {
	private:
		constexpr static uint32_t uvExponentBits = 4;
		constexpr static uint32_t uvComponentBits = 14;
		constexpr static uint32_t uvExponentMask = (1 << uvExponentBits) - 1;
		constexpr static uint32_t uvComponentMask = (1 << uvComponentBits) - 1;
		constexpr static uint32_t scale = (1 << (uvComponentBits - 1));

		Vec3 m_normal;// quantize to 3 signs + 2*14 bit x/y, reconstruct z
		uint32_t m_uvDiffuse;
	public:
		VertexNorUvTemplate() {};
		VertexNorUvTemplate(Vec3 normal, Uv uvDiffuse) : m_normal(normal)
		{
			uint32_t uCeil = (uint32_t) std::ceil(std::abs(uvDiffuse.u));
			uint32_t vCeil = (uint32_t) std::ceil(std::abs(uvDiffuse.v));
			uint32_t ceil = std::max(uCeil, vCeil);
			uint32_t exponent = std::max(std::bit_width(ceil), 0);
			if (exponent > uvExponentMask) {
				LOG(WARNING) << "Failed to compress UV, value too large! Max magnitude allowed: " << (1 << uvExponentMask);
			}
			uint32_t factor = 1 << exponent;
			Uv uvNorm = mul(uvDiffuse, 1.f * scale / factor);
			// cast to int preserves sign bit, but it is extended into all high bits (2's complement), so we must clear those with mask
			m_uvDiffuse = exponent << (uvComponentBits * 2)
				| ((int32_t)std::lround(uvNorm.u) & uvComponentMask) << uvComponentBits
				| ((int32_t)std::lround(uvNorm.v) & uvComponentMask);
		};
		Vec3 normal() const { return m_normal; }
		Uv uvDiffuse() const
		{
			uint32_t exponent = ((m_uvDiffuse >> (uvComponentBits * 2)) & uvExponentMask);
			uint32_t uBits = (m_uvDiffuse >> uvComponentBits) & uvComponentMask;
			uint32_t vBits = m_uvDiffuse & uvComponentMask;
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
	};
	template <> inline VertexAttributes inputLayout<VertexNorUvTemplate<true>>() {
		return {
			{ Type::FLOAT_3, Semantic::NORMAL },
			{ Type::UINT, Semantic::TEXCOORD },
		};
	}

	using VertexNorUv = VertexNorUvTemplate<COMPRESS_VERTEX_ATTRIBUTES>;
	inline std::ostream& operator <<(std::ostream& os, const VertexNorUv& that)
	{
		return os << "[NOR:" << that.normal() << " UV_DIFF:" << that.uvDiffuse() << "]";
	}

	// TODO create union of uvLightmap + colLight with single bit flag to differentiate
	// TODO move uvDiffuse into TexIndex
	struct VertexBasic {
		Uvi uvLightmap;
		Color colLight;
		Vec3 dirLight;
		float lightSun;
	};
	inline std::ostream& operator <<(std::ostream& os, const VertexBasic& that)
	{
		return os << "[COL_LIGHT:" << that.colLight << " DIR_LIGHT:" << that.dirLight << " UV_LM:" << that.uvLightmap << "]";
	}
	template <> inline VertexAttributes inputLayout<VertexBasic>() {
		return {
			{ Type::FLOAT_3, Semantic::TEXCOORD },
			{ Type::FLOAT_4, Semantic::COLOR },
			{ Type::FLOAT_3, Semantic::NORMAL },
			{ Type::FLOAT, Semantic::TEXCOORD },// TODO use OTHER
		};
	}

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
	const float chunkSizePerDim = 50;// optimized for G1, ideally we would first caluclate the bounds of the worldmesh and then use middlepoint and size

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
	inline std::ostream& operator <<(std::ostream& os, const ChunkIndex& that)
	{
		return os << "[X=" << that.x << " Y=" << that.y << "]";
	}
	} namespace std { template <> struct hash<render::ChunkIndex> : render::ChunkIndex::Hash {}; } namespace render {


	struct ChunkVertCluster {
		GridPos gridPos;
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
		std::vector<VertexIndex> vecIndexLod;
		std::vector<VertexPos> vecPos;
		std::vector<VertexNorUv> vecNormalUv;
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

	// TODO rename to Encoding / Compression or something, since color is always SRGB, even if linear
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
	using ChunkToVerts = std::unordered_map<GridPos, Verts<F>>;
	
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
		std::vector<ChunkVertCluster> vertClustersLod;// TODO make sure we do not have empty cluster cells in here

		std::vector<VertexIndex> vecIndex;// all LOD indices are inserted after normal indices
		uint32_t lodStart = 0;// count of non-LOD indices

		//std::vector<VertexIndex> vecIndexLod;
		std::vector<VertexPos> vecPos;
		std::vector<VertexNorUv> vecNormalUv;
		std::vector<F> vecOther;
		// TODO either remove vec prefix everywhere and use plural or not consistently
		std::vector<TexIndex> texIndices;
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
		const GridPos gridPos;
		const uint32_t vertIndex; // we assume the vert vector is not modified during lifetime of index

		// TODO maybe the whole struct should be templated instead of every single method
		template <VERTEX_FEATURE F>
		const VertsBasic& get(const MatToChunksToVerts<F>& meshData) const
		{
			return meshData.find(this->mat)->second.find(this->gridPos)->second;
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