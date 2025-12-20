#pragma once

namespace render
{
	enum class BufferUsage // castable to D3D11_USAGE
	{
		IMMUTABLE = 1,// D3D11_USAGE_IMMUTABLE -> in GPU VRAM, TODO rename to GPU_IMMUTABLE
		WRITE_CPU = 2,// D3D11_USAGE_DYNAMIC -> in GPU VRAM, implicit synchronization mechanism to copy from CPU to GPU in driver, TODO rename to GPU_READ_CPU_MAP
		WRITE_GPU = 0,// D3D11_USAGE_DEFAULT -> in GPU VRAM, TODO rename to GPU_READ_WRITE
		READBACK = 3  // D3D11_USAGE_STAGING -> in CPU VRAM, for explicit synchronization (copied to/from other usage types with copy functions), TODO rename to CPU_READ_WRITE or CPU_FOR_COPY
	};

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

	enum class VertexLightType
	{
		WORLD_COLOR = 0,
		WORLD_LIGHTMAP = 1,
		OBJECT_COLOR = 2,
		OBJECT_DECAL = 3,
	};

	enum class Type { // equivalent to DXGI_FORMAT
		FLOAT,
		FLOAT_2,
		FLOAT_3,
		FLOAT_4,
		UINT,
		UINT_2,
	};

	enum class Semantic {
		POSITION,
		NORMAL,
		TEXCOORD,
		COLOR,
		INDEX,
		OTHER
	};

	using VertexAttribute = std::pair<Type, Semantic>;
	using VertexAttributes = std::vector<VertexAttribute>;

	constexpr bool PACK_VERTEX_ATTRIBUTES = true;
}