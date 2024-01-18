#include "stdafx.h"
#include "PipelineForward.h"

#include "PipelineWorld.h"

namespace renderer::forward {

	ID3D11RenderTargetView* linearBackBuffer = nullptr; // linear color, 64-bit
	ID3D11ShaderResourceView* linearBackBufferResource = nullptr;
	D3D11_VIEWPORT linearBackBufferViewport;

	ID3D11DepthStencilView* depthView = nullptr;
	ID3D11DepthStencilState* depthState = nullptr;
	ID3D11RasterizerState* wireFrame;


	void draw(D3d d3d, ShaderManager* shaders, RenderSettings& settings) {
		// enable depth
		d3d.deviceContext->OMSetDepthStencilState(depthState, 1);

		// set the linear back buffer as rtv
		d3d.deviceContext->OMSetRenderTargets(1, &linearBackBuffer, depthView);
		d3d.deviceContext->RSSetViewports(1, &linearBackBufferViewport);

		// clear the back buffer to a deep blue
		d3d.deviceContext->ClearRenderTargetView(linearBackBuffer, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

		// clear depth and stencil buffer
		const float zFar = settings.reverseZ ? 0.0 : 1.0f;
		d3d.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, zFar, 0);

		// set rasterizer state
		if (settings.wireframe) {
			d3d.deviceContext->RSSetState(wireFrame);
		}

		// draw world to linear buffer
		world::draw(d3d, shaders);
	}

	ID3D11ShaderResourceView* initRenderBuffer(D3d d3d, BufferSize& size)
	{
		if (linearBackBuffer != nullptr) {
			linearBackBuffer->Release();
		}
		if (linearBackBufferResource != nullptr) {
			linearBackBufferResource->Release();
		}

		auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		// Create buffer texture
		D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(format, size.width, size.height);
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;

		ID3D11Texture2D* texture;
		d3d.device->CreateTexture2D(&desc, nullptr, &texture);

		// Create RTV
		D3D11_RENDER_TARGET_VIEW_DESC descRTV = CD3D11_RENDER_TARGET_VIEW_DESC(
			D3D11_RTV_DIMENSION_TEXTURE2D,
			format
		);
		d3d.device->CreateRenderTargetView(texture, &descRTV, &linearBackBuffer);

		// Create SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = CD3D11_SHADER_RESOURCE_VIEW_DESC(
			D3D11_SRV_DIMENSION_TEXTURE2D,
			format
		);
		descSRV.Texture2D.MipLevels = 1;
		d3d.device->CreateShaderResourceView((ID3D11Resource*)texture, &descSRV, &linearBackBufferResource);

		// Done
		texture->Release();

		return linearBackBufferResource;
	}

	void initViewport(BufferSize& size)
	{
		initViewport(size, &linearBackBufferViewport);
	}

	void initDepthBuffer(D3d d3d, BufferSize& size, bool reverseZ)
	{
		release(depthView);

		// create depth buffer
		D3D11_TEXTURE2D_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(D3D11_TEXTURE2D_DESC));

		depthStencilDesc.Width = size.width;
		depthStencilDesc.Height = size.height;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		ID3D11Texture2D* depthStencilBuffer;
		d3d.device->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer);
		d3d.device->CreateDepthStencilView(depthStencilBuffer, nullptr, &depthView);
		depthStencilBuffer->Release();

		// create and set depth state
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
		ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

		depthStencilStateDesc.DepthEnable = TRUE;
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = reverseZ ? D3D11_COMPARISON_GREATER : D3D11_COMPARISON_LESS;
		depthStencilStateDesc.StencilEnable = FALSE;
		depthStencilStateDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		depthStencilStateDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		const D3D11_DEPTH_STENCILOP_DESC defaultStencilOp =
		{
			D3D11_STENCIL_OP_KEEP,
			D3D11_STENCIL_OP_KEEP,
			D3D11_STENCIL_OP_KEEP,
			D3D11_COMPARISON_ALWAYS
		};
		depthStencilStateDesc.FrontFace = defaultStencilOp;
		depthStencilStateDesc.BackFace = defaultStencilOp;

		d3d.device->CreateDepthStencilState(&depthStencilStateDesc, &depthState);
	}

	void initRasterizerStates(D3d d3d)
	{
		D3D11_RASTERIZER_DESC wireFrameDesc;
		ZeroMemory(&wireFrameDesc, sizeof(D3D11_RASTERIZER_DESC));

		wireFrameDesc.FillMode = D3D11_FILL_WIREFRAME;
		wireFrameDesc.CullMode = D3D11_CULL_NONE;
		d3d.device->CreateRasterizerState(&wireFrameDesc, &wireFrame);
	}

	void clean()
	{
		linearBackBuffer->Release();
		linearBackBufferResource->Release();

		depthView->Release();
		wireFrame->Release();
	}
}
