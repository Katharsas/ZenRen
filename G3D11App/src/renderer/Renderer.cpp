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
#include <algorithm>

using namespace Microsoft::WRL;

#include "Settings.h"
#include "RenderSettings.h"
#include "Camera.h"
#include "PipelineForward.h"
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
	ID3D11ShaderResourceView* linearBackBufferResource = nullptr; // owned by RendererForward

	ShaderManager* shaders;

	RenderSettings settingsPrevious;
	RenderSettings settings;

	BufferSize clientSize;
	BufferSize renderSize;

	// forward definitions
	void initDeviceAndSwapChain(HWND hWnd);

	void updateRenderSize() {
		renderSize = clientSize * settings.resolutionScaling;
		renderSize.width = std::max(renderSize.width, 1u);
		renderSize.height = std::max(renderSize.height, 1u);
	}

	struct RenderDirtyFlags {
		//bool resizeSwapChain = false;

		bool backBuffer = false;// TODO rename to backBufferViewport
		bool backBufferQuadVerts = false;

		bool renderBuffer = false;
		bool renderBufferSampler = false;

		bool depthBuffer = false;
		bool forwardRasterizer = false;
		bool forwardBlendState = false;
		
		bool diffuseTexSampler = false;
		//bool staticVertexBuffer = false;
		bool shaderSettingsGlobal = false;

		void onInit() {
			backBuffer = true, backBufferQuadVerts = true,
				renderBuffer = true, renderBufferSampler = true,
				depthBuffer = true, forwardRasterizer = true, forwardBlendState = true,
				diffuseTexSampler = true; shaderSettingsGlobal = true;
		}
		void onChangeRenderSize() {
			renderBuffer = true;
			depthBuffer = true;
		}
		void onChangeClientSize() {
			onChangeRenderSize();
			backBuffer = true;
		}
		void onChangeMultisampling() {
			renderBuffer = true;
			depthBuffer = true;
			forwardRasterizer = true;
			forwardBlendState = true;
		}
		void onChangeWireframe() {
			forwardRasterizer = true;
		}
		void onChangeReverseZ() {
			backBufferQuadVerts = true;
			depthBuffer = true;
		}
	};

	void reinitRenderer(RenderDirtyFlags& flags) {
		if (flags.backBuffer) {
			postprocess::initDownsampleBuffers(d3d, clientSize);
			postprocess::initBackBuffer(d3d, swapchain);
			postprocess::initViewport(clientSize);
		}
		if (flags.backBufferQuadVerts) {
			postprocess::initVertexBuffers(d3d, settings.reverseZ);
		}
		if (flags.renderBuffer) {
			linearBackBufferResource = forward::initRenderBuffer(d3d, renderSize, settings.multisampleCount);
			forward::initViewport(renderSize);
		}
		if (flags.renderBufferSampler) {
			postprocess::initLinearSampler(d3d, !settings.resolutionUpscaleSmooth);
		}
		if (flags.depthBuffer) {
			forward::initDepthBuffer(d3d, renderSize, settings.multisampleCount, settings.reverseZ);
		}
		if (flags.forwardRasterizer) {
			forward::initRasterizerStates(d3d, settings.multisampleCount, settings.wireframe);
		}
		if (flags.forwardBlendState) {
			forward::initBlendState(d3d, settings.multisampleCount);
		}
		if (flags.diffuseTexSampler) {
			world::initLinearSampler(d3d, settings);
		}
		if (flags.shaderSettingsGlobal) {
			world::updateShaderSettings(d3d, settings);
		}
	}

	void initD3D(HWND hWnd)
	{
		clientSize = {
			::settings::SCREEN_WIDTH,
			::settings::SCREEN_HEIGHT,
		};
		updateRenderSize();

		renderer::addWindow("Info", {
			[]() -> void {
				const std::string line = renderer::util::getVramUsage(dx12.adapter);
				ImGui::Text(line.c_str());
			}
		});

		initDeviceAndSwapChain(hWnd);
		world::initConstantBufferPerObject(d3d);

		RenderDirtyFlags flags;
		flags.onInit();
		reinitRenderer(flags);
		postprocess::initDepthBuffer(d3d, clientSize);

		initGui(hWnd, d3d);

		camera::init();

		shaders = new ShaderManager(d3d);
		world::initConstantBufferPerObject(d3d);
		world::initVertexIndexBuffers(d3d);

		gui::settings::init(settings);
	}

	void onWindowResize(uint32_t width, uint32_t height) {
		clientSize = { width, height };
		updateRenderSize();

		// see https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi#handling-window-resizing
		if (swapchain) {
			d3d.deviceContext->OMSetRenderTargets(0, 0, 0);

			RenderDirtyFlags flags;
			flags.onChangeClientSize();
			reinitRenderer(flags);
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
		
		postprocess::clean();
		forward::clean();

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

		RenderDirtyFlags renderState;
		if (settings.resolutionScaling != settingsPrevious.resolutionScaling) {
			updateRenderSize();
			renderState.onChangeRenderSize();
		}
		if (settings.resolutionUpscaleSmooth != settingsPrevious.resolutionUpscaleSmooth) {
			renderState.renderBufferSampler = true;
		}
		if (settings.wireframe != settingsPrevious.wireframe) {
			renderState.onChangeWireframe();
		}
		if (settings.multisampleCount != settingsPrevious.multisampleCount) {
			renderState.onChangeMultisampling();
		}
		if (settings.reverseZ != settingsPrevious.reverseZ) {
			renderState.onChangeReverseZ();
		}
		if (settings.anisotropicFilter != settingsPrevious.anisotropicFilter || settings.anisotropicLevel != settingsPrevious.anisotropicFilter) {
			renderState.diffuseTexSampler = true;
		}
		if (settings.shader.ambientLight != settingsPrevious.shader.ambientLight || settings.shader.mode != settingsPrevious.shader.mode) {
			renderState.shaderSettingsGlobal = true;
		}
		reinitRenderer(renderState);

		camera::updateCamera(settings.reverseZ, renderSize);

		// foward pipeline renders world/scene to linear backbuffer
		forward::draw(d3d, shaders, settings);
		
		// postprocessing pipeline renders linear backbuffer to real backbuffer
		postprocess::draw(d3d, linearBackBufferResource, shaders, settings.downsampling);

		// gui does not output shading information so it goes to real sRGB backbuffer as well
		settingsPrevious = settings;
		drawGui();

		postprocess::resolveAndPresent(d3d, swapchain);
	}

	void initDeviceAndSwapChain(HWND hWnd)
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
		swapChainDesc.Width = 0;										  // back buffer width and height are set later
		swapChainDesc.Height = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
		swapChainDesc.SampleDesc.Count = 1;                               // sample count (swapchain multisampling should never be used!)
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
}
