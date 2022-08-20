#include "stdafx.h"
#include "Renderer.h"

// Directx 11 Renderer according to this tutorial:
// http://www.directxtutorial.com/Lesson.aspx?lessonid=11-4-2
// And this one:
// https://www.braynzarsoft.net/viewtutorial/q16390-braynzar-soft-directx-11-tutorials
//
// Beefed up with whatever code i could find on the internet.

#include <windowsx.h>
#include <d3dx11.h>
#include <d3dx10.h>

#include "Settings.h"
#include "Camera.h"
#include "PipelinePostProcess.h"
#include "PipelineWorld.h"
#include "ShaderManager.h"
#include "Shader.h"
#include "../Util.h"
#include "Gui.h"
#include "imgui/imgui.h"
//#include <vdfs/fileIndex.h>
//#include <zenload/zenParser.h>

// include the Direct3D Library file
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")

#define DEBUG_D3D11

namespace renderer
{
	struct Settings {
		bool wireframe = false;
		bool reverseZ = true;// TODO a substantial part of the pipeline needs to be reinitialized for this to be changable at runtime!
	};

	// global declarations
	D3d d3d;
	
	IDXGISwapChain* swapchain;
	ID3D11RenderTargetView* linearBackBuffer; // 64-bit
	ID3D11ShaderResourceView* linearBackBufferResource;

	ShaderManager* shaders;

	ID3D11DepthStencilView* depthStencilView;

	ID3D11RasterizerState* wireFrame;

	Settings settingsPrevious;
	Settings settings;

	// forward definitions
	void initDeviceAndSwapChain(HWND hWnd);
	void initDepthAndStencilBuffer();
	void initBackBufferHDR();
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

		initDeviceAndSwapChain(hWnd);

		postprocess::initBackBuffer(d3d, swapchain);
		initDepthAndStencilBuffer();
		initBackBufferHDR();

		initGui(hWnd, d3d.device, d3d.deviceContext);

		// Set the viewport
		D3D11_VIEWPORT viewport;
		ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = settings::SCREEN_WIDTH;
		viewport.Height = settings::SCREEN_HEIGHT;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		d3d.deviceContext->RSSetViewports(1, &viewport);

		camera::init();

		shaders = new ShaderManager(d3d.device);
		postprocess::initLinearSampler(d3d);
		postprocess::initVertexBuffers(d3d, settings.reverseZ);
		world::initConstantBufferPerObject(d3d);
		world::initVertexIndexBuffers(d3d, settings.reverseZ);
		initRasterizerStates();

		addSettings("Renderer", {
			[]() -> void {
				ImGui::Checkbox("Wireframe Mode", &settings.wireframe);
				ImGui::Checkbox("Reverse Z", &settings.reverseZ);
			}
		});
	}

	void cleanD3D()
	{
		swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

		cleanGui();

		// close and release all existing COM objects
		delete shaders;

		world::clean();

		swapchain->Release();
		depthStencilView->Release();
		
		postprocess::clean();

		linearBackBuffer->Release();
		linearBackBufferResource->Release();
		wireFrame->Release();

		d3d.device->Release();
		d3d.deviceContext->Release();
	}

	void update()
	{
		world::updateObjects();
	}

	void renderFrame(void)
	{
		auto deviceContext = d3d.deviceContext;

		
		if (settings.reverseZ != settingsPrevious.reverseZ) {
			postprocess::reInitVertexBuffers(d3d, settings.reverseZ);
		}
		if (settings.reverseZ != settingsPrevious.reverseZ) {
			// TODO this leaks d3d objects because reinitialization does not release previous objects!
			initDepthAndStencilBuffer();
		}

		camera::updateCamera(settings.reverseZ);

		// set the linear back buffer as rtv
		deviceContext->OMSetRenderTargets(1, &linearBackBuffer, depthStencilView);

		// clear the back buffer to a deep blue
		deviceContext->ClearRenderTargetView(linearBackBuffer, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

		// clear depth and stencil buffer
		const float zFar = settings.reverseZ ? 0.0 : 1.0f;
		deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, zFar, 0);

		// set rasterizer state
		if (settings.wireframe) {
			deviceContext->RSSetState(wireFrame);
		}

		// draw world to linear buffer
		world::draw(d3d, shaders);
		
		// postprocessing pipeline renders linear backbuffer to real backbuffer
		postprocess::draw(d3d, linearBackBufferResource, shaders);

		// gui does not output shading information so it goes to real sRGB backbuffer as well
		settingsPrevious = settings;
		drawGui();

		// switch the back buffer and the front buffer
		swapchain->Present(0, 0);
	}

	void initDeviceAndSwapChain(HWND hWnd)
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
			&d3d.device,
			nullptr,
			&d3d.deviceContext);

		if (FAILED(hr))
		{
			LOG(FATAL) << "Could not create D3D11 device. Make sure your GPU supports DirectX 11.";
		}
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
		d3d.device->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer);
		d3d.device->CreateDepthStencilView(depthStencilBuffer, nullptr, &depthStencilView);
		depthStencilBuffer->Release();

		// create and set depth state
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
		ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

		depthStencilStateDesc.DepthEnable = TRUE;
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = settings.reverseZ ? D3D11_COMPARISON_GREATER : D3D11_COMPARISON_LESS;
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
		d3d.device->CreateDepthStencilState(&depthStencilStateDesc, &depthStencilState);
		d3d.deviceContext->OMSetDepthStencilState(depthStencilState, 1);
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
	}

	void initRasterizerStates()
	{
		D3D11_RASTERIZER_DESC wireFrameDesc;
		ZeroMemory(&wireFrameDesc, sizeof(D3D11_RASTERIZER_DESC));

		wireFrameDesc.FillMode = D3D11_FILL_WIREFRAME;
		wireFrameDesc.CullMode = D3D11_CULL_NONE;
		d3d.device->CreateRasterizerState(&wireFrameDesc, &wireFrame);
	}
	
}
