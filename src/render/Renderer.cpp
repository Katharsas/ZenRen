#include "stdafx.h"
#include "Renderer.h"

#include <wrl/client.h>
#include <algorithm>

using namespace Microsoft::WRL;

#include "WinDx.h"
#include "render/Settings.h"
#include "render/SettingsGui.h"
#include "render/Camera.h"
#include "render/pass/PassForward.h"
#include "render/pass/PassPost.h"
#include "render/pass/world/PassWorld.h"
#include "render/pass/PassSky.h"
#include "render/ShaderManager.h"
#include "render/Shader.h"
#include "render/RenderUtil.h"
#include "Util.h"
#include "Gui.h"

#include <imgui.h>

namespace render
{
	using namespace ::render::pass;

	HWND windowHandle;

	// global declarations
	struct Dx12 {
		bool isAvailable = false;// true if DXGI 1.4 (IDXGIFactory4) is available
		IDXGIAdapter3* adapter = nullptr;// we need this for VRAM usage queries
	};

	D3d dx11;
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
		renderSize.width = std::max(renderSize.width, (uint16_t) 1u);
		renderSize.height = std::max(renderSize.height, (uint16_t) 1u);
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

		bool cameraProjection = false;

		void onInit() {
			backBuffer = true, backBufferQuadVerts = true,
				renderBuffer = true, renderBufferSampler = true,
				depthBuffer = true, forwardRasterizer = true, forwardBlendState = true,
				diffuseTexSampler = true; cameraProjection = true;
		}
		void onChangeRenderSize() {
			renderBuffer = true;
			depthBuffer = true;
		}
		void onChangeClientSize() {
			onChangeRenderSize();
			backBuffer = true;
			cameraProjection = true;
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
			cameraProjection = true;
		}
	};

	void reinitRenderer(D3d d3d, RenderDirtyFlags& flags) {
		if (flags.backBuffer) {
			post::initDownsampleBuffers(d3d, clientSize);
			post::initBackBuffer(d3d, swapchain);
			post::initViewport(clientSize);
		}
		if (flags.backBufferQuadVerts) {
			post::initVertexBuffers(d3d, settings.reverseZ);
		}
		if (flags.renderBuffer) {
			linearBackBufferResource = forward::initRenderBuffer(d3d, renderSize, settings.multisampleCount);
			forward::initViewport(renderSize);
		}
		if (flags.renderBufferSampler) {
			post::initLinearSampler(d3d, !settings.resolutionUpscaleSmooth);
		}
		if (flags.depthBuffer) {
			forward::initDepthBuffer(d3d, renderSize, settings.multisampleCount, settings.reverseZ);
		}
		if (flags.forwardRasterizer) {
			forward::initRasterizerStates(d3d, settings.multisampleCount, settings.wireframe);
		}
		if (flags.forwardBlendState) {
			forward::initBlendStates(d3d, settings.multisampleCount, settings.multisampleTransparency);
		}
		if (flags.diffuseTexSampler) {
			world::initLinearSampler(d3d, settings);
		}
		if (flags.cameraProjection) {
			camera::initProjection(settings.reverseZ, renderSize, settings.viewDistance, settings.fovVertical);
		}
	}

	void initD3D(void* hWnd, const BufferSize& startSize)
	{
		auto& d3d = dx11;

		windowHandle = (HWND) hWnd;
		clientSize = startSize;
		updateRenderSize();

		render::gui::addInfo("", {
			[]() -> void {
				const std::string memory = "MEM: " + render::util::getMemUsage();
				ImGui::Text(memory.c_str());
				const std::string vram = "VRAM: " + render::util::getVramUsage(dx12.adapter);
				ImGui::Text(vram.c_str());
			}
		});

		initDeviceAndSwapChain(windowHandle);

		RenderDirtyFlags flags;
		flags.onInit();
		reinitRenderer(d3d, flags);
		post::initDepthBuffer(d3d, clientSize);
		post::initRasterizer(d3d);
		post::initConstantBuffers(d3d);

		gui::init(windowHandle, d3d);

		camera::init();
		forward::initConstantBuffers(d3d);
		shaders = new ShaderManager(d3d);

		world::init(d3d);

		gui::settings::init(settings, [&]() -> void { world::notifyGameSwitch(settings); });
	}

	bool loadLevel(const std::optional<std::string>& level, bool defaultSky)
	{
		auto& d3d = dx11;

		bool loaded = false;
		if (level.has_value()) {
			auto loadResult = world::loadWorld(d3d, level.value());
			if (loadResult.loaded) {
				settings.isG2 = loadResult.isG2;
				world::notifyGameSwitch(settings);
				defaultSky = loadResult.isOutdoorLevel;
				loaded = true;
			}
		}
		if (defaultSky) {
			sky::loadSky(d3d);
		}
		else {
			world::getWorldSettings().drawSky = false;
		}
		
		return loaded;
	}

	void onWindowResize(const BufferSize& changedSize)
	{
		auto& d3d = dx11;

		clientSize = changedSize;
		updateRenderSize();

		// see https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi#handling-window-resizing
		if (swapchain) {
			d3d.deviceContext->OMSetRenderTargets(0, 0, 0);

			RenderDirtyFlags flags;
			flags.onChangeClientSize();
			reinitRenderer(d3d, flags);
		}
	}

	void onWindowDpiChange(float dpiScale)
	{
		gui::onWindowDpiChange(windowHandle);
	}

	void cleanD3D()
	{
		auto& d3d = dx11;

		swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

		// close and release all existing COM objects

		d3d.deviceContext->ClearState();
		d3d.deviceContext->Flush();

		gui::clean();

		delete shaders;

		world::clean();
		
		post::clean();
		forward::clean();

		swapchain->Release();

		if (dx12.isAvailable) {
			dx12.adapter->Release();
		}

		d3d.device->Release();
		d3d.deviceContext->Release();
		d3d.annotation->Release();

		if (d3d.debug.has_value()) {
			d3d.debug.value()->ReportLiveDeviceObjects(
				D3D11_RLDO_FLAGS::D3D11_RLDO_DETAIL
				| D3D11_RLDO_FLAGS::D3D11_RLDO_IGNORE_INTERNAL);
			LOG(INFO) << "Reported all relevant remaining objects.";
			d3d.debug.value()->Release();
		}
	}

	void update(float deltaTime)
	{
		world::updateObjects(deltaTime);
	}

	void renderFrame()
	{
		auto& d3d = dx11;

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
		if (settings.multisampleCount != settingsPrevious.multisampleCount || settings.multisampleTransparency != settingsPrevious.multisampleTransparency) {
			renderState.onChangeMultisampling();
		}
		if (settings.reverseZ != settingsPrevious.reverseZ) {
			renderState.onChangeReverseZ();
		}
		if (settings.anisotropicLevel != settingsPrevious.anisotropicLevel) {
			renderState.diffuseTexSampler = true;
		}
		if (settings.viewDistance != settingsPrevious.viewDistance) {
			renderState.cameraProjection = true;
		}
		if (settings.fovVertical != settingsPrevious.fovVertical) {
			renderState.cameraProjection = true;
		}
		reinitRenderer(d3d, renderState);

		bool hasCameraChanged = camera::updateCamera();

		// foward pipeline renders scene to linear backbuffer
		forward::draw(d3d, shaders, settings, hasCameraChanged);
		
		// postprocessing pipeline renders linear backbuffer to real backbuffer
		post::draw(d3d, linearBackBufferResource, shaders, settings);

		// gui does not output shading information so it goes to real sRGB backbuffer as well
		settingsPrevious = settings;
		d3d.annotation->BeginEvent(L"imgui");
		world::updateSettings(settings.showAdvancedSettings);
		gui::draw();
		d3d.annotation->EndEvent();
	}

	void presentFrameBlocking()
	{
		auto& d3d = dx11;
		post::resolveAndPresent(d3d, swapchain);
	}

	void initDeviceAndSwapChain(HWND hWnd)
	{
		auto& d3d = dx11;
		// TODO HR handling

		ComPtr<ID3D11Device> device_11_0;
		ComPtr<ID3D11DeviceContext> deviceContext_11_0;

		// all feature levels that will be checked in order, we only care about 11_0
		const D3D_FEATURE_LEVEL requiredLevels[] = { D3D_FEATURE_LEVEL_11_0 };

		UINT creationFlags = 0;
#if defined(_DEBUG)
		// enable debug layer
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
		LOG(INFO) << "D3D11 debug layer enabled!";
#else
		LOG(INFO) << "D3D11 debug layer disabled.";
#endif

		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			creationFlags,
			requiredLevels,
			_countof(requiredLevels),
			D3D11_SDK_VERSION,
			&device_11_0,
			nullptr,
			&deviceContext_11_0);

		if (hr == E_INVALIDARG) {
			LOG(FATAL) << "DirectX Feature Level 11_0 capable GPU required! If your are on Windows 7, please install KB 2670838.";
		}
		
		// Convert to dxgiDevice and get adapter/factory that originally created our device (in "D3D11CreateDevice")
		ComPtr<IDXGIDevice2> dxgiDevice;
		device_11_0.As(&dxgiDevice);
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
			device_11_0.Get(),
			hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapchain);

		d3d.device = device_11_0.Detach();
		d3d.deviceContext = deviceContext_11_0.Detach();

		// debugging - named render phases
		d3d.deviceContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&d3d.annotation);

		// debugging - named resources and cleanup checks
		ID3D11Debug* debug;
		d3d.device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debug);
		if (debug != nullptr) {
			d3d.debug = debug;
		}
	}
}
