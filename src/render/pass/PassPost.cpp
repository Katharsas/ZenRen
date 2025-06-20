#include "stdafx.h"
#include "PassPost.h"

#include "../WinDx.h"
#include "../Renderer.h"
#include "../Shader.h"
#include "render/RenderUtil.h"
#include "render/d3d/ConstantBuffer.h"

namespace render::pass::post
{
	__declspec(align(16)) struct CbPostSettings {
		// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing

		float contrast;
		float brightness;
		float gamma;
	} postSettings;

	ID3D11SamplerState* samplerState = nullptr; // sampler to read linear backbuffer texture

	ID3D11Texture2D* multisampleTex = nullptr;
	ID3D11RenderTargetView* multisampleRtv = nullptr; // linear color, 64-bit, native resolution, multisampled
	D3D11_VIEWPORT viewport;

	ID3D11Texture2D* resolvedTex = nullptr;
	ID3D11ShaderResourceView* resolvedSrv = nullptr; // linear color, 64-bit, native resolution

	ID3D11Texture2D* backBufferTex = nullptr;
	ID3D11RenderTargetView* backBufferRtv = nullptr; // non-linear color (sRGB), 32-bit, native resolution backbuffer
	
	ID3D11DepthStencilView* multisampleDepthView = nullptr;
	ID3D11DepthStencilState* depthStateNone = nullptr;
	ID3D11RasterizerState* rasterizer;

	d3d::ConstantBuffer<CbPostSettings> settingsCb = {};

	uint32_t downsamplingSamples = 8;

	DecalMesh toneMappingQuad;

	void clean()
	{
		toneMappingQuad.release();
		release(std::vector<IUnknown*> {
			samplerState,

			multisampleTex,
			multisampleRtv,

			resolvedTex,
			resolvedSrv,

			backBufferTex,
			backBufferRtv,

			multisampleDepthView,
			depthStateNone,
			rasterizer,
		});
		settingsCb.release();
	}

	void updatePostSettingsCb(D3d d3d, const RenderSettings& settings) {
		postSettings.brightness = settings.brightness;
		postSettings.contrast = settings.contrast;
		postSettings.gamma = settings.gamma;
		d3d::updateConstantBuf(d3d, settingsCb, postSettings);
	}

	void renderToTexure(D3d d3d, Shader* shader, ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* depth) {
		d3d.deviceContext->OMSetRenderTargets(1, &rtv, depth);
		d3d.deviceContext->RSSetViewports(1, &viewport);

		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);
		d3d.deviceContext->PSSetShaderResources(0, 1, &srv);
		d3d.deviceContext->PSSetSamplers(0, 1, &samplerState);

		UINT stride = sizeof(PosUv);
		UINT offset = 0;
		d3d.deviceContext->IASetVertexBuffers(0, 1, &(toneMappingQuad.vertexBuffer), &stride, &offset);
		d3d.deviceContext->Draw(toneMappingQuad.vertexCount, 0);

		//deviceContext->IASetIndexBuffer(toneMappingQuad.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		//deviceContext->DrawIndexed(toneMappingQuad.indexCount, 0, 0);
	}

	void draw(D3d d3d, ID3D11ShaderResourceView* linearBackBuffer, ShaderManager* shaders, const RenderSettings& settings)
	{
		// default blend state
		d3d.deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

		bool renderDirectlyToBackBuffer = !settings.downsampling;
		// draw HDR back buffer to real back buffer via tone mapping

		// disable depth
		d3d.deviceContext->OMSetDepthStencilState(depthStateNone, 1);

		// set rasterizer state back to default (disables wireframe mode etc.)
		d3d.deviceContext->RSSetState(rasterizer);

		ID3D11RenderTargetView* rtv = renderDirectlyToBackBuffer ? backBufferRtv : multisampleRtv;
		ID3D11DepthStencilView* depth = renderDirectlyToBackBuffer ? nullptr : multisampleDepthView;

		Shader* toneMappingShader = shaders->getShader("toneMapping");
		updatePostSettingsCb(d3d, settings);
		d3d.deviceContext->PSSetConstantBuffers(0, 1, &settingsCb.buffer);
		renderToTexure(d3d, toneMappingShader, linearBackBuffer, rtv, depth);

		if (!renderDirectlyToBackBuffer) {
			auto format = DXGI_FORMAT_R8G8B8A8_UNORM;
			d3d.deviceContext->ResolveSubresource(
				resolvedTex, D3D11CalcSubresource(0, 0, 1),
				multisampleTex, D3D11CalcSubresource(0, 0, 1),
				format);

			Shader* renderToTexShader = shaders->getShader("renderToTexture");
			renderToTexure(d3d, renderToTexShader, resolvedSrv, backBufferRtv, nullptr);
		}

		// unbind srv so we can use it as rtv next frame
		ID3D11ShaderResourceView* srv = nullptr;
		d3d.deviceContext->PSSetShaderResources(0, 1, &srv);
	}

	void resolveAndPresent(D3d d3d, IDXGISwapChain1* swapchain)
	{
		// TODO
		//d3d.deviceContext->OMSetRenderTargets(1, &backBuffer, nullptr);

		// switch the back buffer and the front buffer
		swapchain->Present(0, 0);
	}

	void initDownsampleBuffers(D3d d3d, BufferSize& size)
	{
		release(multisampleRtv);
		release(multisampleTex);
		release(resolvedSrv);
		release(resolvedTex);

		auto format = DXGI_FORMAT_R8G8B8A8_UNORM;
		{
			// TEX
			D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(format, size.width, size.height);
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.MipLevels = 1;
			desc.SampleDesc.Count = downsamplingSamples;// multisampled
			d3d.device->CreateTexture2D(&desc, nullptr, &multisampleTex);

			// RTV
			D3D11_RENDER_TARGET_VIEW_DESC descRTV = CD3D11_RENDER_TARGET_VIEW_DESC(
				D3D11_RTV_DIMENSION_TEXTURE2DMS,
				format
			);
			d3d.device->CreateRenderTargetView(multisampleTex, &descRTV, &multisampleRtv);
		} {
			// TEX
			D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(format, size.width, size.height);
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.MipLevels = 1;
			d3d.device->CreateTexture2D(&desc, nullptr, &resolvedTex);

			// SRV
			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = CD3D11_SHADER_RESOURCE_VIEW_DESC(
				D3D11_SRV_DIMENSION_TEXTURE2D,
				format
			);
			descSRV.Texture2D.MipLevels = 1;
			d3d.device->CreateShaderResourceView((ID3D11Resource*)resolvedTex, &descSRV, &resolvedSrv);
		}
	}

	void initDepthBuffer(D3d d3d, BufferSize& size)
	{
		release(multisampleDepthView);
		release(depthStateNone);
		HRESULT hr;

		// create depth buffer
		D3D11_TEXTURE2D_DESC depthTexDesc = CD3D11_TEXTURE2D_DESC(
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
			size.width,
			size.height
		);
		//depthTexDesc.Width = size.width;
		//depthTexDesc.Height = size.height;
		depthTexDesc.MipLevels = 1;
		//depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		depthTexDesc.SampleDesc.Count = downsamplingSamples;
		depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		ID3D11Texture2D* depthTex;
		hr = d3d.device->CreateTexture2D(&depthTexDesc, nullptr, &depthTex);
		::util::throwOnError(hr);

		CD3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = CD3D11_DEPTH_STENCIL_VIEW_DESC(
			D3D11_DSV_DIMENSION_TEXTURE2DMS
		);
		D3D11_TEX2DMS_DSV depthViewDmsDesc;
		depthViewDmsDesc.UnusedField_NothingToDefine = 0;
		depthViewDesc.Texture2DMS = depthViewDmsDesc;
		//depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		hr = d3d.device->CreateDepthStencilView(depthTex, &depthViewDesc, &multisampleDepthView);
		::util::throwOnError(hr);

		hr = depthTex->Release();
		::util::throwOnError(hr);

		// create and set depth state
		D3D11_DEPTH_STENCIL_DESC depthStateDesc;
		ZeroMemory(&depthStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
		depthStateDesc.DepthEnable = FALSE;

		hr = d3d.device->CreateDepthStencilState(&depthStateDesc, &depthStateNone);
		::util::throwOnError(hr);
	}

	void initBackBuffer(D3d d3d, IDXGISwapChain1* swapchain)
	{
		release(backBufferRtv);
		release(backBufferTex);
		HRESULT hr;

		// Preserve the existing buffer count and format.
		// Automatically choose the width and height to match the client rect for HWNDs.
		hr = swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		::util::warnOnError(hr);

		// get the address of the back buffer
		hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferTex);
		::util::throwOnError(hr);

		// since swapchain itself was described to be linear, we need to trigger sRGB conversion
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = CD3D11_RENDER_TARGET_VIEW_DESC(
			D3D11_RTV_DIMENSION_TEXTURE2D,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		);
		// use the texture address to create the render target
		hr = d3d.device->CreateRenderTargetView(backBufferTex, &rtvDesc, &backBufferRtv);
		::util::throwOnError(hr);
	}

	void initViewport(BufferSize& size) {
		util::initViewport(size, &viewport);
	}

	void initLinearSampler(D3d d3d, bool pointSampling)
	{
		release(samplerState);

		D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC();
		ZeroMemory(&samplerDesc, sizeof(samplerDesc));

		// point samping does not interpolate when upscaling, so we can more easily see pixel aliasing
		samplerDesc.Filter = pointSampling ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxAnisotropy = 1;// no anisotropy, TODO how to multisample when upscaling?
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = 0;// always use most detailed level

		d3d.device->CreateSamplerState(&samplerDesc, &samplerState);
	}

	void initVertexBuffers(D3d d3d, bool reverseZ)
	{
		toneMappingQuad.release();

		const float zNear = reverseZ ? 1.0f : 0.0f;

		std::array<PosUv, 6> fullscreenQuadData = { {
			{ { -1.0f, 1.0f, zNear }, { 0.0f, 0.0f } },
			{ { 1.0f, 1.0, zNear }, { 1.0f, 0.0f } },
			{ { 1.0, -1.0, zNear }, { 1.0f, 1.0f } },
			{ { -1.0f, 1.0f, zNear }, { 0.0f, 0.0f } },
			{ { 1.0, -1.0, zNear }, { 1.0f, 1.0f } },
			{ { -1.0, -1.0, zNear }, { 0.0f, 1.0f } },
		} };

		// create vertex buffer containing fullscreen quad
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(PosUv) * fullscreenQuadData.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = fullscreenQuadData.data();
			d3d.device->CreateBuffer(&bufferDesc, &initialData, &(toneMappingQuad.vertexBuffer));
			toneMappingQuad.vertexCount = fullscreenQuadData.size();
		}
	}

	void initRasterizer(D3d d3d)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc;
		ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

		rasterizerDesc.MultisampleEnable = true;
		d3d.device->CreateRasterizerState(&rasterizerDesc, &rasterizer);
	}

	void initConstantBuffers(D3d d3d)
	{
		// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		d3d::createConstantBuf(d3d, settingsCb, BufferUsage::WRITE_GPU);
	}
}