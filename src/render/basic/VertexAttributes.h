#pragma once

#include "render/basic/Primitives.h"
#include "render/basic/VertexPacker.h"

namespace render
{
	template <typename ATTR>
	VertexAttributes inputLayout();

	using TexIndex = uint32_t;
	template <> inline VertexAttributes inputLayout<TexIndex>() {
		return {
			{  Type::UINT, Semantic::OTHER }
		};
	}

	using VertexPos = Vec3;
	template <> inline VertexAttributes inputLayout<VertexPos>() {
		return {
			{  Type::FLOAT_3, Semantic::POSITION }
		};
	}

	template <bool IS_PACKED>
	struct VertexNorUvTemplate {};

	template <> struct VertexNorUvTemplate<false> {
	private:
		Vec3 m_normal;
		Uv m_uvDiffuse;
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
		uint32_t m_normal;
		uint32_t m_uvDiffuse;
	public:
		VertexNorUvTemplate() {};
		VertexNorUvTemplate(Vec3 normal, Uv uvDiffuse)
			: m_normal(packNormal(normal)), m_uvDiffuse(packUv(uvDiffuse)) {};
		Vec3 normal() const { return unpackNormal(m_normal); }
		Uv uvDiffuse() const { return unpackUv(m_uvDiffuse); }
	};
	template <> inline VertexAttributes inputLayout<VertexNorUvTemplate<true>>() {
		return {
			{ Type::UINT, Semantic::NORMAL },
			{ Type::UINT, Semantic::TEXCOORD },
		};
	}

	using VertexNorUv = VertexNorUvTemplate<PACK_VERTEX_ATTRIBUTES>;
	inline std::ostream& operator <<(std::ostream& os, const VertexNorUv& that)
	{
		return os << "[NOR:" << that.normal() << " UV_DIFF:" << that.uvDiffuse() << "]";
	}


	constexpr uint32_t instanceIdNone = UINT16_MAX;

	template <bool IS_PACKED>
	struct VertexLightTemplate {};

	template <> struct VertexLightTemplate<false> {
	private:
		Vec3 m_colLight;
		uint32_t m_instanceId;
		Uvi m_uviLightmap;
		uint32_t m_type;
	public:
		VertexLightTemplate() {};
		VertexLightTemplate(VertexLightType type, Color colLight, Uvi uviLightmap, uint32_t instanceId = instanceIdNone)
			: m_type((uint32_t)type), m_uviLightmap(uviLightmap), m_instanceId(instanceId)
		{
			// TODO add some nicer factory functions that make upholding asserts easier

			// TODO some world vertex light values have random alpha values??? what does that mean?
			//  isolate in shader to check which parts of worldmesh are affected
			//if (colLight.a != 1.f && colLight.a != 0.f) {
			//	LOG(INFO) << "Invalid alpha value for light: " << std::to_string(colLight.a);
			//}

			// TODO apparently there are also world vertex light values for vertices that are using lightmap,
			//  but maybe thats just some kind of default value?
			//assert((m_type == (uint32_t)VertexLightType::WORLD_LIGHTMAP) == (colLight.a == 0.f));

			assert((type == VertexLightType::WORLD_LIGHTMAP) == (uviLightmap.i >= 0.f));
			assert((type == VertexLightType::OBJECT_COLOR
				|| type == VertexLightType::OBJECT_DECAL) == (instanceId < instanceIdNone));
			m_colLight = { colLight.r, colLight.g, colLight.b };
		};
		VertexLightType type() const { return (VertexLightType)m_type; }
		uint32_t instanceId() const { return m_instanceId; }
		Color colLight() const { return Color(m_colLight.x, m_colLight.y, m_colLight.z, 1.f); }
		Uvi uviLightmap() const { return m_uviLightmap; }
	};
	template <> inline VertexAttributes inputLayout<VertexLightTemplate<false>>() {
		return {
			{ Type::FLOAT_3, Semantic::COLOR },
			{ Type::UINT, Semantic::OTHER },
			{ Type::FLOAT_3, Semantic::TEXCOORD },
			{ Type::UINT, Semantic::OTHER },
		};
	}

	template <> struct VertexLightTemplate<true> {
	private:
		std::pair<uint32_t, uint32_t> packed;
	public:
		VertexLightTemplate() {};
		VertexLightTemplate(VertexLightType type, Color colLight, Uvi uviLightmap, uint32_t instanceId = instanceIdNone)
		{
			assert((type == VertexLightType::WORLD_LIGHTMAP) == (uviLightmap.i >= 0.f));
			assert((type == VertexLightType::OBJECT_COLOR
				|| type == VertexLightType::OBJECT_DECAL) == (instanceId < instanceIdNone));
			packed = packLight(type, colLight, uviLightmap, (uint16_t) instanceId);
		};
		VertexLightType type() const { return unpackLightType(packed); }
		uint32_t instanceId() const { return unpackInstanceId(packed); }
		Color colLight() const { return unpackLightColor(packed); }
		Uvi uviLightmap() const { return unpackLightmap(packed); }
	};
	template <> inline VertexAttributes inputLayout<VertexLightTemplate<true>>() {
		return {
			{ Type::UINT, Semantic::OTHER },
			{ Type::UINT, Semantic::OTHER },
		};
	}

	using VertexLight = VertexLightTemplate<PACK_VERTEX_ATTRIBUTES>;
	using VertexBasic = VertexLight;
	inline std::ostream& operator <<(std::ostream& os, const VertexLight& that)
	{
		return os << "[TYPE:" << (uint32_t)that.type() << " ID:" << that.instanceId() << " LIGHT_SUN:" << that.colLight() << " UVI_LM:" << that.uviLightmap() << "]";
	}
}