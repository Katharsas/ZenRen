#include "stdafx.h"
#include "D3D11Renderer.h"

// Directx 11 Renderer according to this tutorial:
// http://www.directxtutorial.com/Lesson.aspx?lessonid=11-4-2
// And this one:
// https://www.braynzarsoft.net/viewtutorial/q16390-braynzar-soft-directx-11-tutorials
//
// Beefed up with whatever code i could find on the internet.

#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>
#pragma warning(push)
#pragma warning(disable : 4838)
#include <xnamath.h>
#pragma warning(pop)
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
	struct CameraMatrices
	{
		XMMATRIX view;
		XMMATRIX projection;
	};

	struct Camera
	{
		XMVECTOR position;
		XMVECTOR target;
		XMVECTOR up;
	};

	struct CbPerObject
	{
		XMMATRIX WVP;
	};

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

	Mesh toneMappingQuad;

	MeshIndexed cube;
	Mesh triangle;

	CameraMatrices camMatrices;
	Camera camera;

	ID3D11Buffer* cbPerObjectBuffer;

	const bool reverseZ = true;
	ID3D11DepthStencilView* depthStencilView;

	ID3D11RasterizerState* wireFrame;

	// scene state
	float rot = 0.1f;
	XMMATRIX cube1World;
	XMMATRIX cube2World;

	// forward definitions
	void initSwapChainAndBackBuffer(HWND hWnd);
	void initDepthAndStencilBuffer();
	void initBackBufferHDR();
	void initPipelineToneMapping();
	void initVertexIndexBuffers();
	void initConstantBufferPerObject();
	void initRasterizerStates();

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

		// Set camera
		camera = {
			XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		};

		// Set view & projection matrix
		camMatrices.view = XMMatrixLookAtLH(camera.position, camera.target, camera.up);

		const float aspectRatio = static_cast<float>(settings::SCREEN_WIDTH) / settings::SCREEN_HEIGHT;
		const float nearZ = 0.1f;
		const float farZ = 1000.0f;
		camMatrices.projection = XMMatrixPerspectiveFovLH(0.4f*3.14f, aspectRatio, reverseZ ? farZ : nearZ, reverseZ ? nearZ : farZ);

		shaders = new ShaderManager(device);
		initPipelineToneMapping();
		initConstantBufferPerObject();
		initVertexIndexBuffers();
		initRasterizerStates();
	}

	void cleanD3D()
	{
		swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

		// close and release all existing COM objects
		delete shaders;

		cube.release();
		triangle.release();
		toneMappingQuad.release();

		swapchain->Release();
		depthStencilView->Release();
		backBuffer->Release();
		backBufferHDR->Release();
		backBufferHDRResource->Release();
		linearSamplerState->Release();
		cbPerObjectBuffer->Release();
		wireFrame->Release();

		device->Release();
		deviceContext->Release();
	}

	/* Calculates combined World-View-Projection-Matrix for use as constant buffer value
	 */
	CbPerObject calculateWvpConstantBuffer(const XMMATRIX objectsWorldMatrix, CameraMatrices viewAndProjection)
	{
		const XMMATRIX wvp = objectsWorldMatrix * camMatrices.view * camMatrices.projection;
		return { XMMatrixTranspose(wvp) };
	}

	void updateObjects()
	{
		//Keep the cubes rotating
		rot += .015f;
		if (rot > 6.28f) rot = 0.0f;

		cube1World = XMMatrixIdentity();
		XMVECTOR rotAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMMATRIX Rotation = XMMatrixRotationAxis(rotAxis, rot);
		XMMATRIX Translation = XMMatrixTranslation(0.0f, 0.0f, 4.0f);
		cube1World = Translation * Rotation;

		cube2World = XMMatrixIdentity();
		Rotation = XMMatrixRotationAxis(rotAxis, -rot);
		XMMATRIX Scale = XMMatrixScaling(1.3f, 1.3f, 1.3f);
		cube2World = Rotation * Scale;
	}

	void renderFrame(void)
	{
		// set the HDR back buffer as rtv
		deviceContext->OMSetRenderTargets(1, &backBufferHDR, depthStencilView);

		// clear the back buffer to a deep blue
		deviceContext->ClearRenderTargetView(backBufferHDR, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

		// clear depth and stencil buffer
		const float zFar = reverseZ ? 0.0 : 1.0f;
		deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, zFar, 0);

		// do 3D rendering here
		{
			// set rasterizer state
			deviceContext->RSSetState(wireFrame);

			// set the shader objects avtive
			Shader* triangleShader = shaders->getShader("testTriangle");
			deviceContext->VSSetShader(triangleShader->getVertexShader(), 0, 0);
			deviceContext->IASetInputLayout(triangleShader->getVertexLayout());
			deviceContext->PSSetShader(triangleShader->getPixelShader(), 0, 0);

			// select which vertex / index buffer to display
			UINT stride = sizeof(VERTEX);
			UINT offset = 0;
			deviceContext->IASetVertexBuffers(0, 1, &(cube.vertexBuffer), &stride, &offset);
			deviceContext->IASetIndexBuffer(cube.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

			// set the World/View/Projection matrix, send it to constant buffer, draw to HDR
			// cube 1
			CbPerObject cbPerObject = calculateWvpConstantBuffer(cube1World, camMatrices);
			deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
			deviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

			deviceContext->DrawIndexed(cube.indexCount, 0, 0);

			// cube 2
			cbPerObject = calculateWvpConstantBuffer(cube2World, camMatrices);
			deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
			deviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

			deviceContext->DrawIndexed(cube.indexCount, 0, 0);
		}

		// draw HDR back buffer to real back buffer via tone mapping

		// set rasterizer state back to default
		deviceContext->RSSetState(nullptr);

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
		//deviceContext->IASetIndexBuffer(toneMappingQuad.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

		//deviceContext->DrawIndexed(toneMappingQuad.indexCount, 0, 0);
		deviceContext->Draw(toneMappingQuad.vertexCount, 0);

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
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
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
			D3D11_CREATE_DEVICE_DEBUG,//TODO this should be disabled for more performance in prod releases
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

		// since swapchain itself was described to be linear, we need to trigger sRGB conversion
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = CD3D11_RENDER_TARGET_VIEW_DESC(
			D3D11_RTV_DIMENSION_TEXTURE2D,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		);
		// use the texture address to create the render target
		device->CreateRenderTargetView(texture, &rtvDesc, &backBuffer);
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

	void initConstantBufferPerObject()
	{
		D3D11_BUFFER_DESC cbbd;
		ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

		cbbd.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		cbbd.ByteWidth = sizeof(CbPerObject);
		cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbbd.CPUAccessFlags = 0;
		cbbd.MiscFlags = 0;

		device->CreateBuffer(&cbbd, nullptr, &cbPerObjectBuffer);
	}

	void initVertexIndexBuffers()
	{
		std::array<VERTEX, 8> cubeVerts = { {
			{ POS(-1.0f, -1.0f, -1.0f), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(-1.0f, +1.0f, -1.0f), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(+1.0f, +1.0f, -1.0f), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(+1.0f, -1.0f, -1.0f), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(-1.0f, -1.0f, +1.0f), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(-1.0f, +1.0f, +1.0f), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(+1.0f, +1.0f, +1.0f), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(+1.0f, -1.0f, +1.0f), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
		} };
		// make sure rotation of each triangle is clockwise
		std::array<DWORD, 36> cubeIndices = { {
			// front
			0, 1, 2, 0, 2, 3,
			// back
			4, 6, 5, 4, 7, 6,
			// left
			4, 5, 1, 4, 1, 0,
			// right
			3, 2, 6, 3, 6, 7,
			// top
			1, 5, 6, 1, 6, 2,
			// bottom
			4, 0, 3, 4, 3, 7
		} };

		const float zNear = reverseZ ? 1.0f : 0.0f;
		std::array<VERTEX, 3> triangleData = { {
			{ POS(+0.0f, +0.5f, zNear), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(+0.45f, -0.5f, zNear), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(-0.45f, -0.5f, zNear), D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f) },
		} };

		std::array<QUAD, 6> fullscreenQuadData = { {
			{ POS(-1.0f, 1.0f, zNear), UV(0.0f, 0.0f) },
			{ POS(1.0f, 1.0, zNear), UV(1.0f, 0.0f) },
			{ POS(1.0, -1.0, zNear), UV(1.0f, 1.0f) },
			{ POS(-1.0f, 1.0f, zNear), UV(0.0f, 0.0f) },
			{ POS(1.0, -1.0, zNear), UV(1.0f, 1.0f) },
			{ POS(-1.0, -1.0, zNear), UV(0.0f, 1.0f) },
		} };

		// vertex buffer triangle (dynamic, mappable)
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DYNAMIC;				// write access by CPU and read access by GPU
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// use as a vertex buffer
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	// allow CPU to write in buffer
			bufferDesc.ByteWidth = sizeof(VERTEX) * triangleData.size();

			// see https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/ne-d3d11-d3d11_usage for cpu/gpu data exchange
			device->CreateBuffer(&bufferDesc, nullptr, &(triangle.vertexBuffer));
			triangle.vertexCount = triangleData.size();
		}
		// map to copy triangle vertices into its buffer
		{
			D3D11_MAPPED_SUBRESOURCE ms;
			deviceContext->Map(triangle.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);// map the buffer
			memcpy(ms.pData, triangleData.data(), sizeof(triangleData));									// copy the data
			deviceContext->Unmap(triangle.vertexBuffer, 0);											// unmap the buffer
		}
		// vertex buffer cube
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(VERTEX) * cubeVerts.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = cubeVerts.data();
			device->CreateBuffer(&bufferDesc, &initialData, &(cube.vertexBuffer));
		}
		// index buffer cube
		{
			D3D11_BUFFER_DESC indexBufferDesc;
			ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

			indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			indexBufferDesc.ByteWidth = sizeof(DWORD) * cubeIndices.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = cubeIndices.data();
			device->CreateBuffer(&indexBufferDesc, &initialData, &(cube.indexBuffer));
			cube.indexCount = cubeIndices.size();
		}
		// vertex buffer fullscreen quad
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(QUAD) * fullscreenQuadData.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = fullscreenQuadData.data();
			device->CreateBuffer(&bufferDesc, &initialData, &(toneMappingQuad.vertexBuffer));
			toneMappingQuad.vertexCount = fullscreenQuadData.size();
		}

		// select which primtive type we are using
		deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void initRasterizerStates()
	{
		D3D11_RASTERIZER_DESC wireFrameDesc;
		ZeroMemory(&wireFrameDesc, sizeof(D3D11_RASTERIZER_DESC));

		wireFrameDesc.FillMode = D3D11_FILL_WIREFRAME;
		wireFrameDesc.CullMode = D3D11_CULL_NONE;
		device->CreateRasterizerState(&wireFrameDesc, &wireFrame);
	}


}