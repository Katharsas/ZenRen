#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3DUserDefinedAnnotation;
struct ID3D11Debug;

struct IDXGISwapChain1;

struct ID3D11Resource;
struct ID3D11Buffer;
struct ID3D11Texture2D;

struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

struct ID3D11SamplerState;

enum class BufferUsage // castable to D3D11_USAGE
{
	IMMUTABLE = 1,// D3D11_USAGE_IMMUTABLE -> in GPU VRAM, TODO rename to GPU_IMMUTABLE
	WRITE_CPU = 2,// D3D11_USAGE_DYNAMIC -> in GPU VRAM, implicit synchronization mechanism to copy from CPU to GPU in driver, TODO rename to GPU_READ_CPU_MAP
	WRITE_GPU = 0,// D3D11_USAGE_DEFAULT -> in GPU VRAM, TODO rename to GPU_READ_WRITE
	READBACK = 3  // D3D11_USAGE_STAGING -> in CPU VRAM, for explicit synchronization (copied to/from other usage types with copy functions), TODO rename to CPU_READ_WRITE
};

namespace render
{
	struct D3d;

	//void release(ID3D11Buffer *const dx11object);
	//void release(ID3D11ShaderResourceView *const dx11object);

	// since forward declared D3D do not have a type hierarchy, release(IUnknown* ...) is not a valid overload for parameter of other types,
	// we just implement it with template for all types and assert correct type in implementation.

	template<typename T>
	void release(T* const dx11object);
}
