#include "stdafx.h"
#include "PipelineForward.h"

#include "Camera.h"
#include "Sky.h"
#include "PipelineSky.h"
#include "PipelineWorld.h"

namespace renderer::forward {
	using DirectX::XMMATRIX;


	__declspec(align(16))
	struct CbGlobalSettings {
		// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing
		int32_t multisampleTransparency;
		int32_t distantAlphaDensityFix;
		int32_t outputDirectEnabled;
		uint32_t outputDirectType;// TODO make 0 == !outputDirectEnabled (default rendering), so we can finally use this in shader in a sane way without need for outputDirectEnabled
		D3DXCOLOR skyLight;
		float timeOfDay;
		int32_t skyTexBlurEnabled;
	};
	
	__declspec(align(16))
	struct CbCamera {
		// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing
		DirectX::XMMATRIX worldViewMatrix;
		DirectX::XMMATRIX worldViewMatrixInverseTranposed;
		DirectX::XMMATRIX projectionMatrix;
	};

	ID3D11Texture2D* targetTex = nullptr;// linear color, 64-bit, potentially multisampled
	ID3D11RenderTargetView* targetRtv = nullptr;
	D3D11_VIEWPORT viewport;

	ID3D11Texture2D* resolvedTex = nullptr;// only used if targetTex is multisampled
	ID3D11ShaderResourceView* resultSrv = nullptr;// either targetTex or resolveTex, never multisampled

	ID3D11DepthStencilView* depthView = nullptr;
	ID3D11DepthStencilState* depthState = nullptr;
	ID3D11RasterizerState* rasterizer = nullptr;
	ID3D11RasterizerState* rasterizerWf = nullptr;
	ID3D11BlendState1* blendState = nullptr;
	ID3D11BlendState1* blendStateNoAtc = nullptr;// Alpha to coverage must be disabled for transparent surfaces; always nullptr (default blendstate)

	ShaderCbs shaderCbs;

	void clean()
	{
		release(std::vector<IUnknown*> {
			targetTex,
			targetRtv,
			resolvedTex,
			resultSrv,

			depthView,
			depthState,
			rasterizer,
			rasterizerWf,
			blendState,
			blendStateNoAtc,

			shaderCbs.settingsCb,
			shaderCbs.cameraCb,
		});
	}

	void updateSettings(D3d d3d, const RenderSettings& settings)
	{
		CbGlobalSettings cbGlobalSettings;
		cbGlobalSettings.multisampleTransparency = settings.multisampleTransparency;
		cbGlobalSettings.distantAlphaDensityFix = settings.distantAlphaDensityFix;
		cbGlobalSettings.outputDirectEnabled = settings.shader.mode != ShaderMode::Default;
		cbGlobalSettings.outputDirectType = settings.shader.mode - 1;

		cbGlobalSettings.timeOfDay = world::getWorldSettings().timeOfDay;
		cbGlobalSettings.skyLight = getSkyLightFromIntensity(1, cbGlobalSettings.timeOfDay);
		cbGlobalSettings.skyTexBlurEnabled = settings.skyTexBlur;

		d3d.deviceContext->UpdateSubresource(shaderCbs.settingsCb, 0, nullptr, &cbGlobalSettings, 0, 0);
	}

	void updateCamera(D3d d3d, bool fixedPosAtOrigin = false)
	{
		XMMATRIX worldMatrix;
		if (fixedPosAtOrigin) {
			auto cameraPos = camera::getCameraPosition();
			worldMatrix = DirectX::XMMatrixTranslationFromVector(cameraPos);
		}
		else {
			worldMatrix = DirectX::XMMatrixIdentity();
		}

		auto matrices = camera::getWorldViewMatrix(worldMatrix);
		CbCamera camera = {
			matrices.worldView,
			matrices.worldViewNormal,
			camera::getProjectionMatrix(),
		};

		d3d.deviceContext->UpdateSubresource(shaderCbs.cameraCb, 0, nullptr, &camera, 0, 0);
	}

	void draw(D3d d3d, ShaderManager* shaders, RenderSettings& settings) {
		// set the linear back buffer as rtv
		d3d.deviceContext->OMSetRenderTargets(1, &targetRtv, depthView);
		d3d.deviceContext->RSSetViewports(1, &viewport);

		// clear the back buffer to background color
		d3d.deviceContext->ClearRenderTargetView(targetRtv, world::getBackgroundColor());

		// clear depth and stencil buffer
		const float zFar = settings.reverseZ ? 0.0 : 1.0f;
		d3d.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, zFar, 0);

		// set rasterizer state
		d3d.deviceContext->RSSetState(rasterizer);
		d3d.deviceContext->OMSetBlendState(blendState, NULL, 0xffffffff);

		// enable depth
		d3d.deviceContext->OMSetDepthStencilState(depthState, 1);

		// update common constant buffers
		updateSettings(d3d, settings);

		// draw world to linear buffer
		updateCamera(d3d);
		if (settings.depthPrepass) {
			world::drawPrepass(d3d, shaders, shaderCbs);
		}
		world::drawWorld(d3d, shaders, shaderCbs, targetRtv);
		if (settings.wireframe) {
			d3d.deviceContext->RSSetState(rasterizerWf);
			world::drawWireframe(d3d, shaders, shaderCbs);
		}

		// draw sky (depth disabled, camera at origin)
		// TODO somehow make sure world is split into prepass, opaque renderer, transparency texture renderer, sky renderer, other renderers
		// TODO create custom shader for sky (see https://www.braynzarsoft.net/viewtutorial/q16390-20-cube-mapping-skybox )
		//   - instead of using different depth state, just set z value in shader to infinity after projection matrix is applied
		//   - this will effectively draw the sky behind all other geometry without disabling early z and reduces state changes in pipeline
		//   - test drawing sky last (especially against trees)
		d3d.deviceContext->OMSetBlendState(blendStateNoAtc, NULL, 0xffffffff);
		updateCamera(d3d, true);
		world::drawSky(d3d, shaders, shaderCbs);

		if (settings.multisampleCount > 1) {
			d3d.deviceContext->ResolveSubresource(
				resolvedTex, D3D11CalcSubresource(0, 0, 1),
				targetTex, D3D11CalcSubresource(0, 0, 1),
				DXGI_FORMAT_R16G16B16A16_FLOAT);
		}
	}

	ID3D11ShaderResourceView* initRenderBuffer(D3d d3d, BufferSize& size, uint32_t multisampleCount)
	{
		release(targetTex);
		release(targetRtv);
		release(resolvedTex);
		release(resultSrv);

		auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		// Create buffer texture
		D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(format, size.width, size.height);
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = multisampleCount;

		d3d.device->CreateTexture2D(&desc, nullptr, &targetTex);

		// Create RTV
		D3D11_RENDER_TARGET_VIEW_DESC descRTV = CD3D11_RENDER_TARGET_VIEW_DESC(
			multisampleCount > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D,
			format
		);
		d3d.device->CreateRenderTargetView(targetTex, &descRTV, &targetRtv);

		
		if (multisampleCount > 1) {
			D3D11_TEXTURE2D_DESC resolvedTexDesc = CD3D11_TEXTURE2D_DESC(format, size.width, size.height);
			resolvedTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			resolvedTexDesc.MipLevels = 1;
			resolvedTexDesc.SampleDesc.Count = 1;

			d3d.device->CreateTexture2D(&resolvedTexDesc, nullptr, &resolvedTex);
		}

		// Create SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = CD3D11_SHADER_RESOURCE_VIEW_DESC(
			D3D11_SRV_DIMENSION_TEXTURE2D,
			format
		);
		descSRV.Texture2D.MipLevels = 1;
		if (multisampleCount > 1) {
			d3d.device->CreateShaderResourceView((ID3D11Resource*)resolvedTex, &descSRV, &resultSrv);
		}
		else {
			d3d.device->CreateShaderResourceView((ID3D11Resource*)targetTex, &descSRV, &resultSrv);
		}

		// Done
		return resultSrv;
	}

	void initViewport(BufferSize& size)
	{
		initViewport(size, &viewport);
	}

	void initDepthBuffer(D3d d3d, BufferSize& size, uint32_t multisampleCount, bool reverseZ)
	{
		release(depthView);
		release(depthState);

		// create depth buffer
		{
			D3D11_TEXTURE2D_DESC depthTexDesc;
			ZeroMemory(&depthTexDesc, sizeof(D3D11_TEXTURE2D_DESC));

			depthTexDesc.Width = size.width;
			depthTexDesc.Height = size.height;
			depthTexDesc.MipLevels = 1;
			depthTexDesc.ArraySize = 1;
			depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			depthTexDesc.SampleDesc.Count = multisampleCount;
			depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
			depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

			ID3D11Texture2D* depthStencilBuffer;
			d3d.device->CreateTexture2D(&depthTexDesc, nullptr, &depthStencilBuffer);

			if (multisampleCount > 1) {
				CD3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = CD3D11_DEPTH_STENCIL_VIEW_DESC(
					D3D11_DSV_DIMENSION_TEXTURE2DMS
				);
				//D3D11_TEX2DMS_DSV depthViewDmsDesc;
				//depthViewDmsDesc.UnusedField_NothingToDefine = 0;
				depthViewDesc.Texture2DMS = D3D11_TEX2DMS_DSV{};
				d3d.device->CreateDepthStencilView(depthStencilBuffer, &depthViewDesc, &depthView);
			}
			else {
				d3d.device->CreateDepthStencilView(depthStencilBuffer, nullptr, &depthView);
			}

			depthStencilBuffer->Release();
		}

		// create and set depth states
		{
			D3D11_DEPTH_STENCIL_DESC depthStateDesc;
			ZeroMemory(&depthStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

			depthStateDesc.DepthEnable = TRUE;
			depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStateDesc.DepthFunc = reverseZ ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL;
			depthStateDesc.StencilEnable = FALSE;
			depthStateDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
			depthStateDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
			const D3D11_DEPTH_STENCILOP_DESC defaultStencilOp =
			{
				D3D11_STENCIL_OP_KEEP,
				D3D11_STENCIL_OP_KEEP,
				D3D11_STENCIL_OP_KEEP,
				D3D11_COMPARISON_ALWAYS
			};
			depthStateDesc.FrontFace = defaultStencilOp;
			depthStateDesc.BackFace = defaultStencilOp;

			d3d.device->CreateDepthStencilState(&depthStateDesc, &depthState);
		}
	}

	void initRasterizerStates(D3d d3d, uint32_t multisampleCount, bool wireframe)
	{
		release(rasterizer);
		release(rasterizerWf);
		{
			D3D11_RASTERIZER_DESC rasterizerDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT{});
			if (multisampleCount > 1) {
				rasterizerDesc.MultisampleEnable = TRUE;
			}
			d3d.device->CreateRasterizerState(&rasterizerDesc, &rasterizer);
		} {
			D3D11_RASTERIZER_DESC rasterizerDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT{});
			if (multisampleCount > 1) {
				rasterizerDesc.MultisampleEnable = TRUE;
			}
			rasterizerDesc.AntialiasedLineEnable = TRUE;
			rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
			rasterizerDesc.CullMode = D3D11_CULL_NONE;
			d3d.device->CreateRasterizerState(&rasterizerDesc, &rasterizerWf);
		}
	}

	void initBlendState(D3d d3d, uint32_t multisampleCount, bool multisampleTransparency) {
		release(blendState);

		if (multisampleCount > 1) {
			D3D11_BLEND_DESC1 blendStateDesc = CD3D11_BLEND_DESC1(CD3D11_DEFAULT{});
			blendStateDesc.AlphaToCoverageEnable = multisampleTransparency;
			d3d.device->CreateBlendState1(&blendStateDesc, &blendState);
		}
		else {
			blendState = nullptr;
		}
	}

	void initConstantBuffers(D3d d3d)
	{
		{
			// render settings
			D3D11_BUFFER_DESC bufferDesc;
			ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
			bufferDesc.ByteWidth = sizeof(CbGlobalSettings);
			bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;

			d3d.device->CreateBuffer(&bufferDesc, nullptr, &shaderCbs.settingsCb);
		} {
			// camera matrices
			D3D11_BUFFER_DESC bufferDesc;
			ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
			bufferDesc.ByteWidth = sizeof(CbCamera);
			bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;

			d3d.device->CreateBuffer(&bufferDesc, nullptr, &shaderCbs.cameraCb);
		}
	}
}
