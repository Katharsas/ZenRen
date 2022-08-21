#include "stdafx.h"
#include "PipelinePostProcess.h"

#include "Renderer.h"
#include "Settings.h"
#include "Shader.h"

namespace renderer::postprocess
{
	struct QUAD {
		POS position;
		UV uvCoordinates;
	};

	ID3D11RenderTargetView* backBuffer;    // real non-linear backbuffer, 32-bit
	ID3D11SamplerState* linearSamplerState;// sampler to read linear backbuffer texture

	Mesh toneMappingQuad;

	void draw(D3d d3d, ID3D11ShaderResourceView* linearBackBuffer, ShaderManager* shaders)
	{
		// draw HDR back buffer to real back buffer via tone mapping

		// set rasterizer state back to default (disables wireframe mode etc.)
		d3d.deviceContext->RSSetState(nullptr);

		// set real back buffer as rtv
		d3d.deviceContext->OMSetRenderTargets(1, &backBuffer, nullptr);

		// set shaders and linear backbuffer as texture
		Shader* toneMappingShader = shaders->getShader("toneMapping");
		d3d.deviceContext->VSSetShader(toneMappingShader->getVertexShader(), 0, 0);
		d3d.deviceContext->IASetInputLayout(toneMappingShader->getVertexLayout());
		d3d.deviceContext->PSSetShader(toneMappingShader->getPixelShader(), 0, 0);
		d3d.deviceContext->PSSetShaderResources(0, 1, &linearBackBuffer);
		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		// select which vertex buffer to display
		UINT stride = sizeof(QUAD);
		UINT offset = 0;
		d3d.deviceContext->IASetVertexBuffers(0, 1, &(toneMappingQuad.vertexBuffer), &stride, &offset);
		//deviceContext->IASetIndexBuffer(toneMappingQuad.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

		//deviceContext->DrawIndexed(toneMappingQuad.indexCount, 0, 0);
		d3d.deviceContext->Draw(toneMappingQuad.vertexCount, 0);

		// unbind linear backbuffer texture so the next frame can use it as rtv again
		ID3D11ShaderResourceView* srv = nullptr;
		d3d.deviceContext->PSSetShaderResources(0, 1, &srv);
	}

	void initBackBuffer(D3d d3d, IDXGISwapChain1* swapchain)
	{
		// get the address of the back buffer
		ID3D11Texture2D* texture;
		swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&texture);

		// since swapchain itself was described to be linear, we need to trigger sRGB conversion
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = CD3D11_RENDER_TARGET_VIEW_DESC(
			D3D11_RTV_DIMENSION_TEXTURE2D,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		);
		// use the texture address to create the render target
		d3d.device->CreateRenderTargetView(texture, &rtvDesc, &backBuffer);
		texture->Release();
	}

	void initLinearSampler(D3d d3d)
	{
		D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC();
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		//samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 1.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		samplerDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
		samplerDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX

		d3d.device->CreateSamplerState(&samplerDesc, &linearSamplerState);
		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);// TODO why is this not needed?
	}

	void initVertexBuffers(D3d d3d, bool reverseZ)
	{
		const float zNear = reverseZ ? 1.0f : 0.0f;

		std::array<QUAD, 6> fullscreenQuadData = { {
			{ POS(-1.0f, 1.0f, zNear), UV(0.0f, 0.0f) },
			{ POS(1.0f, 1.0, zNear), UV(1.0f, 0.0f) },
			{ POS(1.0, -1.0, zNear), UV(1.0f, 1.0f) },
			{ POS(-1.0f, 1.0f, zNear), UV(0.0f, 0.0f) },
			{ POS(1.0, -1.0, zNear), UV(1.0f, 1.0f) },
			{ POS(-1.0, -1.0, zNear), UV(0.0f, 1.0f) },
		} };

		// create vertex buffer containing fullscreen quad
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(QUAD) * fullscreenQuadData.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = fullscreenQuadData.data();
			d3d.device->CreateBuffer(&bufferDesc, &initialData, &(toneMappingQuad.vertexBuffer));
			toneMappingQuad.vertexCount = fullscreenQuadData.size();
		}
	}

	void reInitVertexBuffers(D3d d3d, bool reverseZ)
	{
		toneMappingQuad.release();
		initVertexBuffers(d3d, reverseZ);
	}

	void clean(bool onlyBackBuffer)
	{
		backBuffer->Release();
		if (!onlyBackBuffer) {
			toneMappingQuad.release();
			linearSamplerState->Release();
		}
	}
}