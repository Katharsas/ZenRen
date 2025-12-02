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
struct ID3D11BlendState;

struct ID3D11InputLayout;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;

namespace render
{
	using WindowHandle = void*;

	struct D3d {
		ID3D11Device* device;
		ID3D11DeviceContext* deviceContext;
		ID3DUserDefinedAnnotation* annotation;
		std::optional<ID3D11Debug*> debug = {};
	};

	// since forward declared D3D types do not have a type hierarchy, release(IUnknown* ...) is not a valid overload for parameter of other types,
	// we just implement it with template for all types and assert correct type in implementation.

	template<typename T>
	void release(T* const dx11object);
}
