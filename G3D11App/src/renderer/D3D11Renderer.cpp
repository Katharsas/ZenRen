#include "stdafx.h"
#include "D3D11Renderer.h"

// Directx 11 Renderer according to this tutorial:
// http://www.directxtutorial.com/Lesson.aspx?lessonid=11-4-2
// Beefed up with whatever code i could find on the internet.

#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>
#include <array>

#include "Settings.h"
#include "ShaderManager.h"
#include "Shader.h"
#include "../Util.h"
//#include <vdfs/fileIndex.h>
//#include <zenload/zenParser.h>

// include the Direct3D Library file
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")

#define DEBUG_D3D11

namespace renderer
{
	struct Mesh
	{
		ID3D11Buffer* vertexBuffer;
		int32_t vertexCount;

		void release()
		{
			vertexBuffer->Release();
		}
	};
	struct MeshIndexed
	{
		ID3D11Buffer* vertexBuffer;
		ID3D11Buffer* indexBuffer;
		int32_t indexCount;

		void release()
		{
			vertexBuffer->Release();
			indexBuffer->Release();
		}
	};

	struct POS {
		POS(FLOAT x, FLOAT y, FLOAT z) { X = x; Y = y; Z = z; }
		FLOAT X, Y, Z;
	};
	struct UV {
		UV(FLOAT u, FLOAT v) { U = u; V = v; }
		FLOAT U, V;
	};
	struct VERTEX {
		POS position;
		D3DXCOLOR color;
	};
	struct QUAD {
		POS position;
		UV uvCoordinates;
	};

	// global declarations
	IDXGISwapChain* swapchain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;

	ID3D11RenderTargetView* backBuffer;    // 32-bit
	ID3D11RenderTargetView* backBufferHDR; // 64-bit
	ID3D11ShaderResourceView* backBufferHDRResource;
	ID3D11SamplerState* linearSamplerState;

	ShaderManager* shaders;

	MeshIndexed toneMappingQuad;
	Mesh triangle;
	Mesh triangle2;

	ID3D11DepthStencilView* depthStencilView;

	void initSwapChainAndBackBuffer(HWND hWnd);
	void initDepthAndStencilBuffer();
	void initBackBufferHDR();
	void initPipelineToneMapping();
	void initGraphics();

	void initWorld()
	{
		//VDFS::FileIndex vdf;
		//vdf.loadVDF(u8"../g1_data/worlds.VDF");
		//ZenLoad::ZenParser parser(u8"WORLD.ZEN", vdf);
		//parser.readHeader();
		//ZenLoad::oCWorldData world;
		//parser.readWorld(world);
		//ZenLoad::zCMesh* mesh = parser.getWorldMesh();
	}

	void initD3D(HWND hWnd)
	{
		//initWorld();

		initSwapChainAndBackBuffer(hWnd);
		initDepthAndStencilBuffer();
		initBackBufferHDR();

		// Set the viewport
		D3D11_VIEWPORT viewport;
		ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = settings::SCREEN_WIDTH;
		viewport.Height = settings::SCREEN_HEIGHT;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		deviceContext->RSSetViewports(1, &viewport);

		shaders = new ShaderManager(device);
		initPipelineToneMapping();
		initGraphics();
	}

	void cleanD3D()
	{
		swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

		// close and release all existing COM objects
		delete shaders;

		triangle.release();
		toneMappingQuad.release();

		swapchain->Release();
		depthStencilView->Release();
		backBuffer->Release();
		backBufferHDR->Release();
		backBufferHDRResource->Release();
		linearSamplerState->Release();

		device->Release();
		deviceContext->Release();
	}

	void renderFrame(void)
	{
		// clear depth and stencil buffer
		deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

		// set the HDR back buffer as rtv
		deviceContext->OMSetRenderTargets(1, &backBufferHDR, depthStencilView);

		// clear the back buffer to a deep blue
		deviceContext->ClearRenderTargetView(backBufferHDR, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

		// do 3D rendering here
		{
			// set the shader objects avtive
			Shader* triangleShader = shaders->getShader("testTriangle");
			deviceContext->VSSetShader(triangleShader->getVertexShader(), 0, 0);
			deviceContext->IASetInputLayout(triangleShader->getVertexLayout());
			deviceContext->PSSetShader(triangleShader->getPixelShader(), 0, 0);

			// select which vertex buffer to display
			UINT stride = sizeof(VERTEX);
			UINT offset = 0;
			deviceContext->IASetVertexBuffers(0, 1, &(triangle.vertexBuffer), &stride, &offset);

			// draw the vertex buffer to the back buffer
			deviceContext->Draw(triangle.vertexCount, 0);

			// triangle 2
			deviceContext->IASetVertexBuffers(0, 1, &(triangle2.vertexBuffer), &stride, &offset);
			deviceContext->Draw(triangle2.vertexCount, 0);
		}

		// draw HDR back buffer to real back buffer via tone mapping

		// set real back buffer as rtv
		deviceContext->OMSetRenderTargets(1, &backBuffer, nullptr);

		// set shaders and HDR buffer as texture
		Shader* toneMappingShader = shaders->getShader("toneMapping");
		deviceContext->VSSetShader(toneMappingShader->getVertexShader(), 0, 0);
		deviceContext->IASetInputLayout(toneMappingShader->getVertexLayout());
		deviceContext->PSSetShader(toneMappingShader->getPixelShader(), 0, 0);
		deviceContext->PSSetShaderResources(0, 1, &backBufferHDRResource);
		deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		// select which vertex buffer to display
		UINT stride = sizeof(QUAD);
		UINT offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &(toneMappingQuad.vertexBuffer), &stride, &offset);
		deviceContext->IASetIndexBuffer(toneMappingQuad.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

		deviceContext->DrawIndexed(toneMappingQuad.indexCount, 0, 0);

		// unbind HDR back buffer texture so the next frame can use it as rtv again
		ID3D11ShaderResourceView* srv = nullptr;
		deviceContext->PSSetShaderResources(0, 1, &srv);

		// switch the back buffer and the front buffer
		swapchain->Present(0, 0);
	}

	void initSwapChainAndBackBuffer(HWND hWnd)
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

		// fill the swap chain description struct
		swapChainDesc.BufferCount = 1;                                    // one back buffer
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;// use 32-bit color
		swapChainDesc.BufferDesc.Width = settings::SCREEN_WIDTH;          // set the back buffer width
		swapChainDesc.BufferDesc.Height = settings::SCREEN_HEIGHT;        // set the back buffer height
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
		swapChainDesc.OutputWindow = hWnd;                                // the window to be used
		swapChainDesc.SampleDesc.Count = 1;                               // how many multisamples
		swapChainDesc.Windowed = TRUE;                                    // windowed/full-screen mode
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;     // allow full-screen switching

		const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;           // require directx 11.0

		// create a device, device context and swap chain using the information in the scd struct
		HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			D3D11_CREATE_DEVICE_DEBUG,
			&level,
			1,
			D3D11_SDK_VERSION,
			&swapChainDesc,
			&swapchain,
			&device,
			nullptr,
			&deviceContext);

		if (FAILED(hr))
		{
			LOG(FATAL) << "Could not create D3D11 device. Make sure your GPU supports DirectX 11.";
		}

		// get the address of the back buffer
		ID3D11Texture2D* texture;
		swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&texture);

		// use the texture address to create the render target
		device->CreateRenderTargetView(texture, nullptr, &backBuffer);
		texture->Release();
	}

	void initDepthAndStencilBuffer()
	{
		// create depth buffer
		D3D11_TEXTURE2D_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(D3D11_TEXTURE2D_DESC));

		depthStencilDesc.Width = settings::SCREEN_WIDTH;
		depthStencilDesc.Height = settings::SCREEN_HEIGHT;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;

		ID3D11Texture2D* depthStencilBuffer;
		device->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer);
		device->CreateDepthStencilView(depthStencilBuffer, nullptr, &depthStencilView);
		depthStencilBuffer->Release();

		// create and set depth state
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
		ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

		depthStencilStateDesc.DepthEnable = TRUE;
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_GREATER;
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

		ID3D11DepthStencilState* depthStencilState;
		device->CreateDepthStencilState(&depthStencilStateDesc, &depthStencilState);
		deviceContext->OMSetDepthStencilState(depthStencilState, 1);
		depthStencilState->Release();
	}

	void initBackBufferHDR()
	{
		auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		// Create buffer texture
		D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(format, settings::SCREEN_WIDTH, settings::SCREEN_HEIGHT);
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;

		ID3D11Texture2D* texture;
		device->CreateTexture2D(&desc, nullptr, &texture);

		// Create RTV
		D3D11_RENDER_TARGET_VIEW_DESC descRTV = CD3D11_RENDER_TARGET_VIEW_DESC(
			D3D11_RTV_DIMENSION_TEXTURE2D,
			format
		);
		device->CreateRenderTargetView(texture, &descRTV, &backBufferHDR);

		// Create SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = CD3D11_SHADER_RESOURCE_VIEW_DESC(
			D3D11_SRV_DIMENSION_TEXTURE2D,
			format
		);
		descSRV.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView((ID3D11Resource*)texture, &descSRV, &backBufferHDRResource);

		// Done
		texture->Release();
	}


	void initPipelineToneMapping()
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

		device->CreateSamplerState(&samplerDesc, &linearSamplerState);
		deviceContext->PSSetSamplers(0, 1, &linearSamplerState);
	}

	void initGraphics()
	{
		std::array<VERTEX, 3> triangleData = { {
			{ POS(0.0f, 0.5f, 1.0f), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(0.45f, -0.5, 1.0f), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(-0.45f, -0.5f, 1.0f), D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f) }
		} };
		std::array<VERTEX, 3> triangleData2 = { {
			{ POS(0.5f, 0.7f, 0.8f), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(0.95f, -0.3, 0.8f), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(0.05f, -0.3f, 0.8f), D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f) }
		} };

		std::array<QUAD, 4> fullscreenQuadData = { {
			{ POS(-1.0f, 1.0f, 1.0f), UV(0.0f, 0.0f) },
			{ POS(1.0f, 1.0, 1.0f), UV(1.0f, 0.0f) },
			{ POS(1.0, -1.0, 1.0f), UV(1.0f, 1.0f) },
			{ POS(-1.0, -1.0, 1.0f), UV(0.0f, 1.0f) },
		} };

		// make sure rotation of each triangle is clockwise
		std::array<DWORD, 6> fullscreenQuadIndices = {
				0, 1, 2,
				0, 2, 3,
		};

		// vertex buffer triangle (dynamic, mappable)
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DYNAMIC;				// write access by CPU and read access by GPU
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// use as a vertex buffer
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	// allow CPU to write in buffer
			bufferDesc.ByteWidth = sizeof(VERTEX) * triangleData.size();

			// D3D11_USAGE_DYNAMIC allows writing from CPU multiple times by 'mapping' (see below)
			// D3D11_USAGE_DEFAULT would only allow to write data once by passing a D3D11_MAPPED_SUBRESOURCE to next line (pInitialData)
			device->CreateBuffer(&bufferDesc, nullptr, &(triangle.vertexBuffer));
			triangle.vertexCount = triangleData.size();

			device->CreateBuffer(&bufferDesc, nullptr, &(triangle2.vertexBuffer));
			triangle2.vertexCount = triangleData.size();
		}
		// map to copy triangle vertices into its buffer
		{
			D3D11_MAPPED_SUBRESOURCE ms;
			deviceContext->Map(triangle.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);// map the buffer
			memcpy(ms.pData, triangleData.data(), sizeof(triangleData));													// copy the data
			deviceContext->Unmap(triangle.vertexBuffer, 0);											// unmap the buffer

			deviceContext->Map(triangle2.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);// map the buffer
			memcpy(ms.pData, triangleData2.data(), sizeof(triangleData2));													// copy the data
			deviceContext->Unmap(triangle2.vertexBuffer, 0);											// unmap the buffer
		}
		// vertex buffer fullscreen quad
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;				// can only be initialized once by CPU, read/write access by GPU
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(QUAD) * fullscreenQuadData.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = fullscreenQuadData.data();
			device->CreateBuffer(&bufferDesc, &initialData, &(toneMappingQuad.vertexBuffer));
		}
		// index buffer
		{
			D3D11_BUFFER_DESC indexBufferDesc;
			ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

			indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			indexBufferDesc.ByteWidth = sizeof(DWORD) * fullscreenQuadIndices.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = fullscreenQuadIndices.data();
			device->CreateBuffer(&indexBufferDesc, &initialData, &(toneMappingQuad.indexBuffer));
			toneMappingQuad.indexCount = fullscreenQuadIndices.size();
		}

		// select which primtive type we are using
		deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

}