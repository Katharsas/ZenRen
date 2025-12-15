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
			{  Type::UINT, Semantic::TEXCOORD }
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

	// TODO create union of uvLightmap + colLight with single bit flag to differentiate
	// TODO rename to VertexLight?
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
}