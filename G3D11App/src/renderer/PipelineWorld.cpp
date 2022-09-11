#include "stdafx.h"
#include "PipelineWorld.h"

#include <filesystem>

#include "Camera.h"
#include "Texture.h"
#include "../Util.h"
#include "loader/TextureFinder.h";
#include "loader/ZenLoader.h";
#include "loader/ObjLoader.h"

// TODO move to RenderDebugGui
#include "Gui.h"
#include "imgui/imgui.h"

using namespace DirectX;

namespace renderer::world {

	typedef POS_NORMAL_UV VERTEX;

	// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing

	enum ShaderOutputDirect : uint32_t {
		Solid = 0,
		Diffuse = 1,
		Normal = 2,
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

	std::vector<Texture*> debugTextures;

	void initGui() {
		// TODO move to RenderDebugGui

		addWindow("Lightmaps", {
			[&]()  -> void {
				float zoom = 1.5f;
				// TODO make all 42 textures selectable
				if (debugTextures.size() >= 1) {
					ImGui::Image(debugTextures.at(0)->GetResourceView(), {256 * zoom, 256 * zoom});
				}
			}
		});
	}

	void loadTestObj(D3d d3d) {
		
		std::unordered_map<std::string, std::vector<VERTEX>> matsToVertices;
		bool loadObj = false;

		if (loadObj) {
			std::string inputFile = "data_g1/world.obj";
			matsToVertices = loader::loadObj(inputFile);
		}
		else {
			matsToVertices = loader::loadZen();
		}

		{
			auto& lightmaps = loader::loadZenLightmaps();
			for (auto& lightmap : lightmaps) {
				Texture* texture = new Texture(d3d, lightmap.ddsRaw);
				debugTextures.push_back(texture);
			}
		}
		
		std::filesystem::path userDir = util::getUserFolderPath();
		std::filesystem::path texDir = userDir / "CLOUD/Eigene Projekte/Gothic Reloaded Mod/Textures";
		//std::filesystem::path texDir = "Level";
		loader::scanDirForTextures(texDir);

		int32_t meshCount = 0;
		int32_t maxMeshCount = 5000;

		for (const auto& it : matsToVertices) {
			if (meshCount >= maxMeshCount) {
				break;
			}
			auto& filename = it.first;
			auto& vertices = it.second;
			if (!vertices.empty()) {
				auto& actualPath = loader::getTexturePathOrDefault(filename);

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

				Texture* texture = new Texture(d3d, actualPath.u8string());

				mesh.baseColor = texture;
				world.meshes.push_back(mesh);

				meshCount++;
			}
		}

		LOG(DEBUG) << "World material/texture count: " << meshCount;
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

	void updateShaderSettings(D3d d3d, RenderSettings settings)
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
		else {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Solid;
		}

		d3d.deviceContext->UpdateSubresource(cbGlobalSettingsBuffer, 0, nullptr, &cbGlobalSettings, 0, 0);
	}

	void initLinearSampler(D3d d3d, RenderSettings settings)
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

	void initVertexIndexBuffers(D3d d3d, bool reverseZ)
	{
		initGui();

		loadTestObj(d3d);


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
		linearSamplerState->Release();

		for (auto& mesh : world.meshes) {
			mesh.release();
		}
	}
}