#include "stdafx.h"
#include "Renderer.h"

// Directx 11 Renderer according to this tutorial:
// http://www.directxtutorial.com/Lesson.aspx?lessonid=11-4-2
// And this one:
// https://www.braynzarsoft.net/viewtutorial/q16390-braynzar-soft-directx-11-tutorials
//
// Beefed up with whatever code i could find on the internet.
// The renderer can optionally use some DX 12 functionality like flip model and VRAM stats.

#include <wrl/client.h>

using namespace Microsoft::WRL;

#include "Settings.h"
#include "RenderSettings.h"
#include "Camera.h"
#include "PipelinePostProcess.h"
#include "PipelineWorld.h"
#include "ShaderManager.h"
#include "Shader.h"
#include "RenderUtil.h"
#include "../Util.h"
#include "Gui.h"
#include "imgui/imgui.h"

//#include <vdfs/fileIndex.h>
//#include <zenload/zenParser.h>

#define DEBUG_D3D11

namespace renderer
{
	// global declarations
	struct Dx12 {
		bool isAvailable = false;// true if DXGI 1.4 (IDXGIFactory4) is available
		IDXGIAdapter3* adapter = nullptr;// we need this for VRAM usage queries
	};

	D3d d3d;
	Dx12 dx12;
	
	IDXGISwapChain1* swapchain;
	ID3D11RenderTargetView* linearBackBuffer; // 64-bit
	ID3D11ShaderResourceView* linearBackBufferResource;

	ShaderManager* shaders;

	ID3D11DepthStencilView* depthStencilView = nullptr;

	ID3D11RasterizerState* wireFrame;

	RenderSettings settingsPrevious;
	RenderSettings settings;
	BufferSize clientSize;

	// forward definitions
	void initDeviceAndSwapChain(HWND hWnd, BufferSize& size);
	void initDepthAndStencilBuffer(BufferSize& size);
	void initLinearBackBuffer(BufferSize& size);
	void initViewport(BufferSize& size);
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
		clientSize = {
			::settings::SCREEN_WIDTH,
			::settings::SCREEN_HEIGHT
		};

		renderer::addWindow("Info", {
			[]() -> void {
				const std::string line = renderer::util::getVramUsage(dx12.adapter);
				ImGui::Text(line.c_str());
			}
		});

		initDeviceAndSwapChain(hWnd, clientSize);

		postprocess::initBackBuffer(d3d, swapchain);
		initDepthAndStencilBuffer(clientSize);
		initLinearBackBuffer(clientSize);

		initGui(hWnd, d3d);

		initViewport(clientSize);

		camera::init();

		shaders = new ShaderManager(d3d);
		postprocess::initLinearSampler(d3d);
		postprocess::initVertexBuffers(d3d, settings.reverseZ);
		world::initLinearSampler(d3d, settings);
		world::initConstantBufferPerObject(d3d);
		world::initVertexIndexBuffers(d3d, settings.reverseZ);
		initRasterizerStates();

		gui::settings::init(settings);

		world::updateShaderSettings(d3d, settings);
	}

	void initViewport(BufferSize& size) {
		D3D11_VIEWPORT viewport;
		ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = size.width;
		viewport.Height = size.height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		d3d.deviceContext->RSSetViewports(1, &viewport);
	}

	void onWindowResize(uint32_t width, uint32_t height) {
		clientSize = { width, height };

		// see https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi#handling-window-resizing
		if (swapchain) {
			d3d.deviceContext->OMSetRenderTargets(0, 0, 0);

			linearBackBuffer->Release();
			linearBackBufferResource->Release();
			postprocess::clean(true);

			d3d.deviceContext->Flush();

			// Preserve the existing buffer count and format.
			// Automatically choose the width and height to match the client rect for HWNDs.
			swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

			postprocess::initBackBuffer(d3d, swapchain);
			initDepthAndStencilBuffer(clientSize);
			initLinearBackBuffer(clientSize);

			initViewport(clientSize);
		}
	}

	void cleanD3D()
	{
		swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

		cleanGui();

		// close and release all existing COM objects
		if (dx12.isAvailable) {
			dx12.adapter->Release();
		}

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

	void renderFrame()
	{
		auto deviceContext = d3d.deviceContext;
		
		if (settings.reverseZ != settingsPrevious.reverseZ) {
			postprocess::initVertexBuffers(d3d, settings.reverseZ);
		}
		if (settings.reverseZ != settingsPrevious.reverseZ) {
			initDepthAndStencilBuffer(clientSize);
		}
		if (settings.anisotropicFilter != settingsPrevious.anisotropicFilter || settings.anisotropicLevel != settingsPrevious.anisotropicFilter) {
			world::initLinearSampler(d3d, settings);
		}
		if (settings.shader.ambientLight != settingsPrevious.shader.ambientLight || settings.shader.mode != settingsPrevious.shader.mode) {
			world::updateShaderSettings(d3d, settings);
		}

		camera::updateCamera(settings.reverseZ, clientSize);

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

	void initDeviceAndSwapChain(HWND hWnd, BufferSize& size)
	{
		ComPtr<ID3D11Device> device_11_0;
		ComPtr<ID3D11DeviceContext> deviceContext_11_0;

		ComPtr<ID3D11Device1> device_11_1;
		ComPtr<ID3D11DeviceContext1> deviceContext_11_1;

		// all feature levels that will be checked in order, we only care about 11.1
		const D3D_FEATURE_LEVEL requiredLevels[] = { D3D_FEATURE_LEVEL_11_1 };

		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_DEBUG,//TODO this should be disabled for more performance in prod releases
			requiredLevels,
			_countof(requiredLevels),
			D3D11_SDK_VERSION,
			&device_11_0,
			nullptr,
			&deviceContext_11_0);

		if (hr == E_INVALIDARG) {
			LOG(FATAL) << "DirectX 11.1 capable GPU required! If your are on Windows 7, please install KB 2670838.";
		}

		// Convert the interfaces from the DirectX 11.0 to the DirectX 11.1 
		device_11_0.As(&device_11_1);
		deviceContext_11_0.As(&deviceContext_11_1);
		
		// Convert to dxgiDevice and get adapter/factory that originally created our device (in "D3D11CreateDevice")
		ComPtr<IDXGIDevice2> dxgiDevice;
		device_11_1.As(&dxgiDevice);
		ComPtr<IDXGIAdapter> dxgiAdapter;
		dxgiDevice->GetAdapter(&dxgiAdapter);
		ComPtr<IDXGIFactory2> dxgiFactory_2;
		dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), &dxgiFactory_2);

		dxgiDevice.Get()->SetMaximumFrameLatency(1);
		
		ComPtr<IDXGIFactory4> dxgiFactory_4;
		if (FAILED(dxgiFactory_2.As(&dxgiFactory_4))) {
			dx12.isAvailable = false;
		}
		else {
			dx12.isAvailable = true;
			dxgiFactory_4->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&dx12.adapter));
		}

		// Create windowed swapchain, TODO create fullscreen swapchain and pass both
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC1));

		// fill the swap chain description struct
		swapChainDesc.BufferCount = 2;                                    // one back buffer
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;                // use 32-bit color
		swapChainDesc.Width = size.width;                                 // set the back buffer width
		swapChainDesc.Height = size.height;                               // set the back buffer height
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
		swapChainDesc.SampleDesc.Count = 1;                               // how many multisamples
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;     // allow full-screen switching
		if (dx12.isAvailable) {
			LOG(DEBUG) << "Detected Windows 10 or greater! Using flip present model.";
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		}

		dxgiFactory_2->CreateSwapChainForHwnd(
			device_11_1.Get(),
			hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapchain);

		d3d.device = device_11_1.Detach();
		d3d.deviceContext = deviceContext_11_1.Detach();
	}

	void initDepthAndStencilBuffer(BufferSize& size)
	{
		if (depthStencilView != nullptr) {
			depthStencilView->Release();
		}

		// create depth buffer
		D3D11_TEXTURE2D_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(D3D11_TEXTURE2D_DESC));

		depthStencilDesc.Width = size.width;
		depthStencilDesc.Height = size.height;
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

	void initLinearBackBuffer(BufferSize& size)
	{
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
