#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3DUserDefinedAnnotation;
struct ID3D11Debug;

struct IDXGISwapChain1;

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

namespace render {
	struct D3d;

	void release(ID3D11Buffer* dx11object);
	void release(ID3D11ShaderResourceView* dx11object);
}
