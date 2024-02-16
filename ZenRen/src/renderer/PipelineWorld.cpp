#include "stdafx.h"
#include "PipelineWorld.h"

#include <filesystem>
#include <stdexcept>

#include "Camera.h"
#include "Texture.h"
#include "../Util.h"
#include "loader/AssetFinder.h";
#include "loader/ZenLoader.h";
#include "loader/ObjLoader.h"
#include "loader/TexFromVdfLoader.h"

// TODO move to RenderDebugGui
#include "Gui.h"
#include "imgui/imgui.h"

using namespace DirectX;

namespace renderer::world {

	typedef WORLD_VERTEX VERTEX;

	// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing

	enum ShaderOutputDirect : uint32_t {
		Solid = 0,
		Diffuse = 1,
		Normal = 2,
		Lightmap = 3,
	};

	__declspec(align(16))
	struct CbGlobalSettings {
		float ambientLight;
		int32_t outputDirectEnabled;
		ShaderOutputDirect outputDirectType;
	};

	__declspec(align(16))
	struct CbPerObject {
		XMMATRIX worldViewMatrix;
		XMMATRIX worldViewMatrixInverseTranposed;
		XMMATRIX projectionMatrix;
	};

	struct World {
		std::vector<Mesh> meshes;
		XMMATRIX transform;
	};

	ID3D11Buffer* cbGlobalSettingsBuffer;
	ID3D11Buffer* cbPerObjectBuffer;

	float rot = 0.1f;
	float scale = 1.0f;
	float scaleDir = 1.0f;

	World world;

	ID3D11SamplerState* linearSamplerState = nullptr;
	ID3D11ShaderResourceView* lightmapTexArray = nullptr;

	std::vector<Texture*> debugTextures;
	int32_t selectedDebugTexture = 0;

	void initGui() {
		// TODO move to RenderDebugGui
		
		//addWindow("Debug World", {
		//	[&]()  -> void {
		//	}
		//});

		addWindow("Lightmaps", {
			[&]()  -> void {
				ImGui::PushItemWidth(60);
				ImGui::InputInt("Lightmap Index", &selectedDebugTexture, 0);
				ImGui::PopItemWidth();

				float zoom = 2.0f;
				if (debugTextures.size() >= 1) {
					if (selectedDebugTexture >= 0 && selectedDebugTexture < debugTextures.size()) {
						ImGui::Image(debugTextures.at(selectedDebugTexture)->GetResourceView(), { 256 * zoom, 256 * zoom });
					}
				}
			}
		});
	}

	Texture* createTexture(D3d d3d, const std::string& texName)
	{
		auto optionalFilepath = loader::existsAsFile(texName);
		if (optionalFilepath.has_value()) {
			auto path = *optionalFilepath.value();
			return new Texture(d3d, util::toString(path));
		}

		auto optionalVdfIndex = loader::getVdfIndex();
		if (optionalVdfIndex.has_value()) {
			std::string zTexName = util::replaceExtension(texName, "-c.tex");// compiled textures have -C suffix

			if (optionalVdfIndex.value()->hasFile(zTexName)) {
				InMemoryTexFile tex = loader::loadTex(zTexName, optionalVdfIndex.value());
				return new Texture(d3d, tex.ddsRaw, true, zTexName);
			}
		}
		
		return new Texture(d3d, util::toString(loader::DEFAULT_TEXTURE));
	}

	void loadLevel(D3d d3d, std::string& level)
	{
		bool levelDataFound = false;
		RenderData data;
		util::asciiToLower(level);

		if (!util::endsWith(level, ".obj") && !util::endsWith(level, ".zen")) {
			LOG(WARNING) << "Level file format not supported: " << level;
		}

		auto optionalFilepath = loader::existsAsFile(level);
		auto optionalVdfIndex = loader::getVdfIndex();

		if (optionalFilepath.has_value()) {
			if (util::endsWith(level, ".obj")) {
				data = { loader::loadObj(util::toString(*optionalFilepath.value())) };
				levelDataFound = true;
			}
			else {
				// TODO support loading ZEN files
				LOG(WARNING) << "Loading from single level file not supported: " << level;
			}
		}
		else if (optionalVdfIndex.has_value()) {
			if (util::endsWith(level, ".zen")) {
				data = loader::loadZen(level, optionalVdfIndex.value());
				levelDataFound = true;
			}
			else {
				LOG(WARNING) << "Loading from VDF file not supported: " << level;
			}
		}
		else {
			LOG(WARNING) << "Failed to find level file '" << level << "'!";
		}

		if (!levelDataFound) {
			return;
		}

		auto& matsToVerts = data.worldMesh;

		{
			release(lightmapTexArray);
			debugTextures.clear();

			std::vector<std::vector<uint8_t>> ddsRaws;
			for (auto& lightmap : data.worldMeshLightmaps) {
				ddsRaws.push_back(lightmap.ddsRaw);

				Texture* texture = new Texture(d3d, lightmap.ddsRaw, false, "lightmap_xxx");
				debugTextures.push_back(texture);
			}
			// TODO it is totally unclear if these should be loaded as sRGB or linear!
			lightmapTexArray = createShaderTexArray(d3d, ddsRaws, 256, 256);
		}
		
		
		world.meshes.clear();

		int32_t loadedCount = 0;

		for (const auto& pair : matsToVerts) {
			auto& material = pair.first;
			auto& vertices = pair.second;

			if (!vertices.empty()) {
				Texture* texture = createTexture(d3d, material.texBaseColor);
				Mesh mesh;
				{
					// vertex data
					mesh.vertexCount = vertices.size();
					D3D11_BUFFER_DESC bufferDesc;
					ZeroMemory(&bufferDesc, sizeof(bufferDesc));

					bufferDesc.Usage = D3D11_USAGE_DEFAULT;
					bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
					bufferDesc.ByteWidth = sizeof(VERTEX) * vertices.size();

					D3D11_SUBRESOURCE_DATA initialData;
					initialData.pSysMem = vertices.data();
					d3d.device->CreateBuffer(&bufferDesc, &initialData, &(mesh.vertexBuffer));
				}
				mesh.baseColor = texture;
				world.meshes.push_back(mesh);

				loadedCount++;
			}
		}

		LOG(DEBUG) << "World material/texture count: " << loadedCount;

		loadedCount = 0;

		for (const auto& pair : data.staticMeshes) {

			auto& material = pair.first;
			auto& vertices = pair.second;

			if (!vertices.empty()) {
				Texture* texture = createTexture(d3d, material.texBaseColor);
				Mesh mesh;
				{
					// vertex data
					mesh.vertexCount = vertices.size();
					D3D11_BUFFER_DESC bufferDesc;
					ZeroMemory(&bufferDesc, sizeof(bufferDesc));

					bufferDesc.Usage = D3D11_USAGE_DEFAULT;
					bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
					bufferDesc.ByteWidth = sizeof(POS_NORMAL_UV) * vertices.size();

					D3D11_SUBRESOURCE_DATA initialData;
					initialData.pSysMem = vertices.data();
					d3d.device->CreateBuffer(&bufferDesc, &initialData, &(mesh.vertexBuffer));
				}
				mesh.baseColor = texture;
				world.meshes.push_back(mesh);

				loadedCount++;
			}
		}

		LOG(DEBUG) << "StaticInstances material/texture count: " << loadedCount;
	}

	void updateObjects()
	{
		rot += .015f;
		if (rot > 6.28f) rot = 0.0f;

		//scale += (scaleDir * 0.01);
		if (scale > 3.0f) scaleDir = -1.0f;
		if (scale < 0.6f) scaleDir = 1.0f;

		float halfPi = 1.57f;
		world.transform = XMMatrixIdentity();
		XMVECTOR rotAxis = XMVectorSet(1.0f, 0, 0, 0);
		XMMATRIX rotation = XMMatrixRotationAxis(rotAxis, rot);
		XMMATRIX scaleM = XMMatrixScaling(1, scale, 1);
		world.transform = scaleM;
		//world.transform = world.transform * rotation;
	}

	void updateShaderSettings(D3d d3d, RenderSettings& settings)
	{
		CbGlobalSettings cbGlobalSettings;
		cbGlobalSettings.ambientLight = settings.shader.ambientLight;
		cbGlobalSettings.outputDirectEnabled = settings.shader.mode != ShaderMode::Default;
		
		if (settings.shader.mode == ShaderMode::Diffuse) {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Diffuse;
		}
		else if (settings.shader.mode == ShaderMode::Normals) {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Normal;
		}
		else if (settings.shader.mode == ShaderMode::Lightmap) {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Lightmap;
		}
		else if (settings.shader.mode == ShaderMode::Solid || settings.shader.mode == ShaderMode::Default) {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Solid;
		}
		else {
			throw std::invalid_argument("Unknown ShaderMode!");
		}

		d3d.deviceContext->UpdateSubresource(cbGlobalSettingsBuffer, 0, nullptr, &cbGlobalSettings, 0, 0);
	}

	void initLinearSampler(D3d d3d, RenderSettings& settings)
	{
		if (linearSamplerState != nullptr) {
			linearSamplerState->Release();
		}
		D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC();
		ZeroMemory(&samplerDesc, sizeof(samplerDesc));

		if (settings.anisotropicFilter) {
			samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		} else {
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = settings.anisotropicLevel;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		d3d.device->CreateSamplerState(&samplerDesc, &linearSamplerState);
	}

	void init(D3d d3d)
	{
		initGui();

		// select which primtive type we are using
		d3d.deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void initConstantBufferPerObject(D3d d3d)
	{
		{
			// TODO rename or move or something
			D3D11_BUFFER_DESC bufferDesc;
			ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
			bufferDesc.ByteWidth = sizeof(CbGlobalSettings);
			bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;

			d3d.device->CreateBuffer(&bufferDesc, nullptr, &cbGlobalSettingsBuffer);
		}
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

		bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		bufferDesc.ByteWidth = sizeof(CbPerObject);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;

		d3d.device->CreateBuffer(&bufferDesc, nullptr, &cbPerObjectBuffer);
	}

	void draw(D3d d3d, ShaderManager* shaders)
	{
		// set the shader objects avtive
		Shader* shader = shaders->getShader("flatBasicColorTexShader");
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		// constant buffer (object)
		const auto objectMatrices = camera::getWorldViewMatrix(world.transform);
		CbPerObject cbPerObject = {
			objectMatrices.worldView,
			objectMatrices.worldViewNormal,
			camera::getProjectionMatrix(),
		};
		d3d.deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbPerObjectBuffer);
		d3d.deviceContext->PSSetConstantBuffers(1, 1, &cbPerObjectBuffer);

		// constant buffer (settings)
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbGlobalSettingsBuffer);
		d3d.deviceContext->PSSetConstantBuffers(0, 1, &cbGlobalSettingsBuffer);

		// lightmaps
		d3d.deviceContext->PSSetShaderResources(1, 1, &lightmapTexArray);

		// vertex buffer(s)
		UINT stride = sizeof(VERTEX);
		UINT offset = 0;

		for (auto& mesh : world.meshes) {
			auto* resourceView = mesh.baseColor->GetResourceView();
			d3d.deviceContext->PSSetShaderResources(0, 1, &resourceView);
			d3d.deviceContext->IASetVertexBuffers(0, 1, &(mesh.vertexBuffer), &stride, &offset);

			d3d.deviceContext->Draw(mesh.vertexCount, 0);
		}
	}

	void clean()
	{
		cbPerObjectBuffer->Release();
		cbGlobalSettingsBuffer->Release();
		lightmapTexArray->Release();
		linearSamplerState->Release();

		for (auto& tex : debugTextures) {
			delete tex;
		}
		for (auto& mesh : world.meshes) {
			mesh.release();
		}
	}
}