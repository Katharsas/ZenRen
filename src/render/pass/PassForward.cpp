#include "stdafx.h"
#include "PassForward.h"

#include "render/d3d/ConstantBuffer.h"
#include "render/d3d/BlendState.h"
#include "render/RenderUtil.h"

#include "render/Camera.h"
#include "render/Sky.h"
#include "render/pass/PassSky.h"
#include "render/pass/PassWorld.h"


namespace render::pass::forward
{
	using DirectX::XMMATRIX;

	// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing

	__declspec(align(16))
	struct CbGlobalSettings {
		COLOR skyLight;
		int32_t multisampleTransparency;
		int32_t distantAlphaDensityFix;
		uint32_t outputType;
		float timeOfDay;
		int32_t skyTexBlurEnabled;
	};
	
	__declspec(align(16))
	struct CbCamera {
		DirectX::XMMATRIX worldViewMatrix;
		DirectX::XMMATRIX worldViewMatrixInverseTranposed;
		DirectX::XMMATRIX projectionMatrix;
	};

	__declspec(align(16))
	struct CbBlendMode {
		bool alphaTestEnabled;
	};

	__declspec(align(16))
	struct CbDebug {
		float debugFloat1;
		float debugFloat2;
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
	std::array<ID3D11BlendState*, PASS_COUNT> blendStates = { nullptr, nullptr, nullptr, nullptr };
	ID3D11BlendState* blendStateNoAtc = nullptr;// Alpha to coverage must be disabled for transparent surfaces; always nullptr (default blendstate)

	ShaderCbs shaderCbs;


	void clean()
	{
		release(blendStates);
		release(std::vector<IUnknown*> {
			targetTex,
			targetRtv,
			resolvedTex,
			resultSrv,

			depthView,
			depthState,
			rasterizer,
			rasterizerWf,
			blendStateNoAtc,

			shaderCbs.settingsCb,
			shaderCbs.cameraCb,
			shaderCbs.blendModeCb,
		});
	}

	void updateSettings(D3d d3d, const RenderSettings& settings)
	{
		CbGlobalSettings cbGlobalSettings;
		cbGlobalSettings.multisampleTransparency = settings.multisampleTransparency;
		cbGlobalSettings.distantAlphaDensityFix = settings.distantAlphaDensityFix;
		cbGlobalSettings.outputType = settings.shader.mode;

		cbGlobalSettings.timeOfDay = world::getWorldSettings().timeOfDay;
		cbGlobalSettings.skyLight = getSkyLightFromIntensity(1, cbGlobalSettings.timeOfDay);
		cbGlobalSettings.skyTexBlurEnabled = settings.skyTexBlur;

		d3d::updateConstantBuf(d3d, shaderCbs.settingsCb, cbGlobalSettings);

		// debug sliders
		d3d::updateConstantBuf(d3d, shaderCbs.debugCb, CbDebug{ settings.debugFloat1, settings.debugFloat2 });
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

		d3d::updateConstantBuf(d3d, shaderCbs.cameraCb, camera);
	}

	void updateBlendMode(D3d d3d, bool alphaTestEnabled)
	{
		CbBlendMode blendMode { alphaTestEnabled };
		d3d::updateConstantBuf(d3d, shaderCbs.blendModeCb, blendMode);
	}

	std::wstring toWString(const std::string prefix, BlendType blendType)
	{
		return ::util::utf8ToWide(prefix + std::string(magic_enum::enum_name(blendType)));
	}

	void draw(D3d d3d, ShaderManager* shaders, const RenderSettings& settings) {
		d3d.annotation->BeginEvent(L"Pass Foward");

		// set the linear back buffer as rtv
		d3d.deviceContext->OMSetRenderTargets(1, &targetRtv, depthView);
		d3d.deviceContext->RSSetViewports(1, &viewport);

		// clear the back buffer to background color
		d3d.deviceContext->ClearRenderTargetView(targetRtv, world::getBackgroundColor().vec);

		// clear depth and stencil buffer
		const float zFar = settings.reverseZ ? 0.0f : 1.0f;
		d3d.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, zFar, 0);

		// set rasterizer state
		d3d.deviceContext->RSSetState(rasterizer);

		// enable depth
		d3d.deviceContext->OMSetDepthStencilState(depthState, 1);

		// update common constant buffers
		updateSettings(d3d, settings);

		// update camera
		updateCamera(d3d);
		world::updateCameraFrustum(camera::getFrustum());

		// set common constant buffers
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &shaderCbs.settingsCb);
		d3d.deviceContext->PSSetConstantBuffers(0, 1, &shaderCbs.settingsCb);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &shaderCbs.cameraCb);
		d3d.deviceContext->PSSetConstantBuffers(1, 1, &shaderCbs.cameraCb);
		d3d.deviceContext->VSSetConstantBuffers(2, 1, &shaderCbs.blendModeCb);
		d3d.deviceContext->PSSetConstantBuffers(2, 1, &shaderCbs.blendModeCb);
		d3d.deviceContext->VSSetConstantBuffers(3, 1, &shaderCbs.debugCb);
		d3d.deviceContext->PSSetConstantBuffers(3, 1, &shaderCbs.debugCb);

		// opaque / alpha tested passes (unsorted)
		uint8_t blendType = 0;
		if (settings.passesOpaque) {
			updateBlendMode(d3d, true);
			d3d.annotation->BeginEvent(toWString("Pass Blend: ", (BlendType)blendType).c_str());
			d3d.deviceContext->OMSetBlendState(blendStates.at(blendType), nullptr, 0xffffffff);
			if (settings.depthPrepass) {
				world::drawPrepass(d3d, shaders, shaderCbs);
			}
			world::drawWorld(d3d, shaders, shaderCbs, (BlendType)blendType);
			if (settings.wireframe) {
				d3d.deviceContext->RSSetState(rasterizerWf);
				world::drawWireframe(d3d, shaders, shaderCbs, (BlendType)blendType);
				d3d.deviceContext->RSSetState(rasterizer);
			}
			d3d.annotation->EndEvent();

			// draw sky (depth disabled, camera at origin)
			d3d.deviceContext->OMSetBlendState(blendStateNoAtc, NULL, 0xffffffff);
			updateCamera(d3d, true);
			world::drawSky(d3d, shaders, shaderCbs);
			updateCamera(d3d);
		}

		// transparent / (alpha) blend passes
		if (settings.passesBlend) {
			updateBlendMode(d3d, false);
			COLOR blendFactorCol = greyscale(.920f);// used by BlendType::BLEND_FACTOR pass

			for (blendType = 1; blendType < PASS_COUNT; blendType++) {
				d3d.annotation->BeginEvent(toWString("Pass Blend: ", (BlendType)blendType).c_str());
				d3d.deviceContext->OMSetBlendState(blendStates.at(blendType), blendFactorCol.vec, 0xffffffff);
				world::drawWorld(d3d, shaders, shaderCbs, (BlendType)blendType);
				if (settings.wireframe) {
					d3d.deviceContext->RSSetState(rasterizerWf);
					world::drawWireframe(d3d, shaders, shaderCbs, (BlendType)blendType);
					d3d.deviceContext->RSSetState(rasterizer);
				}
				d3d.annotation->EndEvent();
			}
		}

		if (settings.multisampleCount > 1) {
			d3d.deviceContext->ResolveSubresource(
				resolvedTex, D3D11CalcSubresource(0, 0, 1),
				targetTex, D3D11CalcSubresource(0, 0, 1),
				DXGI_FORMAT_R16G16B16A16_FLOAT);
		}

		d3d.annotation->EndEvent();
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
		util::initViewport(size, &viewport);
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

	void initBlendStates(D3d d3d, uint32_t multisampleCount, bool multisampleTransparency) {
		for (uint8_t blendTypeIndex = 0; blendTypeIndex < PASS_COUNT; blendTypeIndex++) {
			auto blendType = (BlendType)blendTypeIndex;
			bool alphaToCoverage = multisampleCount > 1 && multisampleTransparency;
			dx::createBlendState(d3d, &blendStates.at(blendTypeIndex), blendType, alphaToCoverage);
		}
	}

	void initConstantBuffers(D3d d3d)
	{
		// TODO these should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		d3d::createConstantBuf<CbGlobalSettings>(d3d, &shaderCbs.settingsCb, BufferUsage::WRITE_GPU);
		d3d::createConstantBuf<CbCamera>(d3d, &shaderCbs.cameraCb, BufferUsage::WRITE_GPU);
		d3d::createConstantBuf<CbBlendMode>(d3d, &shaderCbs.blendModeCb, BufferUsage::WRITE_GPU);
		d3d::createConstantBuf<CbDebug>(d3d, &shaderCbs.debugCb, BufferUsage::WRITE_GPU);
	}
}
