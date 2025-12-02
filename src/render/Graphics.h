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
		OTHER
	};

	using VertexAttribute = std::pair<Type, Semantic>;
	using VertexAttributes = std::vector<VertexAttribute>;
}