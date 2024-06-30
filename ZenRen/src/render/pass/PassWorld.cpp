#include "stdafx.h"
#include "PassWorld.h"

#include <filesystem>
#include <stdexcept>

#include "PassSky.h"
#include "../Camera.h"
#include "../Texture.h"
#include "../Sky.h"
#include "../RenderUtil.h"
#include "Util.h"
#include "assets/AssetFinder.h"
#include "assets/ZenLoader.h"
#include "assets/ObjLoader.h"
#include "assets/TexFromVdfLoader.h"


// TODO move to RenderDebugGui
#include "../Gui.h"
#include "imgui/imgui.h"

using namespace DirectX;

namespace render::pass::world
{
	struct World {
		std::vector<PrepassMeshes> prepassMeshes;
		std::vector<Mesh> meshes;
		XMMATRIX transform;
	};


	float rot = 0.1f;
	float scale = 1.0f;
	float scaleDir = 1.0f;

	bool isOutdoorLevel = true;

	WorldSettings worldSettings;
	World world;

	// Texture Ownership
	// BaseColor textures are owned by textureCache
	// Lightmap textures are owned by debugTextures/lightmapTexArray
	//   - debugTextures -> single textures used by ImGUI
	//   - lightmapTexArray -> array used by forward renderer

	std::unordered_map<std::string, Texture*> textureCache;

	ID3D11SamplerState* linearSamplerState = nullptr;
	ID3D11ShaderResourceView* lightmapTexArray = nullptr;

	std::vector<Texture*> debugTextures;
	int32_t selectedDebugTexture = 0;

	void updateTimeOfDay(float timeOfDay) {
		// clamp, wrap around if negative
		float __;
		worldSettings.timeOfDay = std::modf(std::modf(timeOfDay, &__) + 1, &__);
	}

	void initGui()
	{
		addSettings("World", {
			[&]()  -> void {
				ImGui::PushItemWidth(GUI_ELEMENT_WIDTH);
				// Lables starting with ## are hidden
				float timeOfDay = worldSettings.timeOfDay;
				bool changed = ImGui::DragFloat("##TimeOfDay", &timeOfDay, .002f, 0, 0, "%.3f TimeOfDay");
				if (changed) {
					updateTimeOfDay(timeOfDay);
				}
				ImGui::SliderFloat("##TimeOfDayChangeSpeed", &worldSettings.timeOfDayChangeSpeed, 0, 1, "%.3f Time Speed", ImGuiSliderFlags_Logarithmic);
				ImGui::PopItemWidth();
			}
		});

		// TODO move to RenderDebugGui
		addWindow("Lightmaps", {
			[&]()  -> void {
				ImGui::PushItemWidth(60);
				ImGui::InputInt("Lightmap Index", &selectedDebugTexture, 0);
				ImGui::PopItemWidth();

				float zoom = 2.0f;
				if (debugTextures.size() >= 1) {
					if (selectedDebugTexture >= 0 && selectedDebugTexture < (int32_t) debugTextures.size()) {
						ImGui::Image(debugTextures.at(selectedDebugTexture)->GetResourceView(), { 256 * zoom, 256 * zoom });
					}
				}
			}
		});
	}

	Texture* createTexture(D3d d3d, const std::string& texName)
	{
		auto optionalFilepath = assets::existsAsFile(texName);
		if (optionalFilepath.has_value()) {
			auto path = *optionalFilepath.value();
			return new Texture(d3d, ::util::toString(path));
		}

		auto optionalVdfIndex = assets::getVdfIndex();
		if (optionalVdfIndex.has_value()) {
			std::string zTexName = ::util::replaceExtension(texName, "-c.tex");// compiled textures have -C suffix

			if (optionalVdfIndex.value()->hasFile(zTexName)) {
				InMemoryTexFile tex = assets::loadTex(zTexName, optionalVdfIndex.value());
				return new Texture(d3d, tex.ddsRaw, true, zTexName);
			}
		}
		
		return new Texture(d3d, ::util::toString(assets::DEFAULT_TEXTURE));
	}

	Texture* getOrCreateTexture(D3d d3d, const std::string& texName) {
		std::function<Texture* ()> createTex = [d3d, texName]() { return createTexture(d3d, texName); };
		return ::util::getOrCreate(textureCache, texName, createTex);
	}

	uint32_t loadPrepassData(D3d d3d, std::vector<PrepassMeshes>& target, const BATCHED_VERTEX_DATA& meshData)
	{
		uint32_t loadedCount = 0;
		std::vector<VERTEX_POS> allVerts;

		for (const auto& pair : meshData) {
			auto& vecPos = pair.second.vecPos;
			if (!vecPos.empty()) {
				Texture* texture = getOrCreateTexture(d3d, pair.first.texBaseColor);
				if (!texture->hasAlpha()) {
					allVerts.insert(allVerts.end(), vecPos.begin(), vecPos.end());
					loadedCount++;
				}
			}
		}
		PrepassMeshes mesh;
		mesh.vertexCount = allVerts.size();
		util::createVertexBuffer(d3d, &mesh.vertexBufferPos, allVerts);
		target.push_back(mesh);
		return loadedCount;
	}

	uint32_t loadVertexData(D3d d3d, std::vector<Mesh>& target, const BATCHED_VERTEX_DATA& meshData)
	{
		// TODO 
		// create Texture cache that maps texBaseColor name to texture
		// allow prepass to populate cache to get hasAlpha information on texture

		uint32_t loadedCount = 0;
		for (const auto& pair : meshData) {
			auto& material = pair.first;
			auto& vertices = pair.second;

			if (!vertices.vecPos.empty()) {
				Texture* texture = getOrCreateTexture(d3d, material.texBaseColor);
				Mesh mesh;
				{
					mesh.vertexCount = vertices.vecPos.size();
					util::createVertexBuffer(d3d, &mesh.vertexBufferPos, vertices.vecPos);
					util::createVertexBuffer(d3d, &mesh.vertexBufferOther, vertices.vecNormalUv);
				}
				mesh.baseColor = texture;
				target.push_back(mesh);

				loadedCount++;
			}
		}
		return loadedCount;
	}

	void loadLevel(D3d d3d, std::string& level)
	{
		bool levelDataFound = false;
		RenderData data;
		::util::asciiToLower(level);

		if (!::util::endsWith(level, ".obj") && !::util::endsWith(level, ".zen")) {
			LOG(WARNING) << "Level file format not supported: " << level;
		}

		auto optionalFilepath = assets::existsAsFile(level);
		auto optionalVdfIndex = assets::getVdfIndex();

		if (optionalFilepath.has_value()) {
			if (::util::endsWith(level, ".obj")) {
				data = { true, assets::loadObj(::util::toString(*optionalFilepath.value())) };
				levelDataFound = true;
			}
			else {
				// TODO support loading ZEN files
				LOG(WARNING) << "Loading from single level file not supported: " << level;
			}
		}
		else if (optionalVdfIndex.has_value()) {
			if (::util::endsWith(level, ".zen")) {
				data = assets::loadZen(level, optionalVdfIndex.value());
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

		isOutdoorLevel = data.isOutdoorLevel;

		{
			release(lightmapTexArray);
			debugTextures.clear();

			std::vector<std::vector<uint8_t>> ddsRaws;
			for (auto& lightmap : data.worldMeshLightmaps) {
				ddsRaws.push_back(lightmap.ddsRaw);

				Texture* texture = new Texture(d3d, lightmap.ddsRaw, false, "lightmap_xxx");
				debugTextures.push_back(texture);
			}
			lightmapTexArray = createShaderTexArray(d3d, ddsRaws, 256, 256, true);
		}
		
		world.meshes.clear();
		world.prepassMeshes.clear();

		int32_t loadedCount = 0;
		loadedCount = loadPrepassData(d3d, world.prepassMeshes, data.worldMesh);
		//loadedCount += loadPrepassData(d3d, world.prepassMeshes, data.staticMeshes);// for now we only use world mesh in depth prepass
		LOG(DEBUG) << "Depth Prepass - World material/texture count: " << loadedCount;

		loadedCount = loadVertexData(d3d, world.meshes, data.worldMesh);
		LOG(DEBUG) << "World Pass - World material/texture count: " << loadedCount;

		loadedCount = loadVertexData(d3d, world.meshes, data.staticMeshes);
		LOG(DEBUG) << "World Pass- StaticInstances material/texture count: " << loadedCount;
	}

	void updateObjects(float deltaTime)
	{
		updateTimeOfDay(worldSettings.timeOfDay + (worldSettings.timeOfDayChangeSpeed * deltaTime));

		// TODO remove
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

	const WorldSettings& getWorldSettings() {
		return worldSettings;
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

		// select which primtive type we are using, TODO this should be managed more centrally, because otherwise changing this affects other parts of pipeline
		d3d.deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		sky::initConstantBuffers(d3d);
	}

	D3DXCOLOR getBackgroundColor() {
		if (isOutdoorLevel) {
			return getSkyColor(worldSettings.timeOfDay);
		}
		else {
			return D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f);// deep blue
		}
	}

	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		if (isOutdoorLevel) {
			const auto layers = getSkyLayers(worldSettings.timeOfDay);
			bool swapLayers = getSwapLayers(worldSettings.timeOfDay);
			sky::updateSkyLayers(d3d, layers, getSkyColor(worldSettings.timeOfDay), worldSettings.timeOfDay, swapLayers);
			sky::drawSky(d3d, shaders, cbs, linearSamplerState);
		}
	}

	void drawPrepass(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		Shader* shader = shaders->getShader("depthPrepass");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(nullptr, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);

		// vertex buffers
		for (auto& mesh : world.prepassMeshes) {
			UINT strides[] = { sizeof(VERTEX_POS) };
			UINT offsets[] = { 0 };
			ID3D11Buffer* vertexBuffers[] = { mesh.vertexBufferPos };

			d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
			d3d.deviceContext->Draw(mesh.vertexCount, 0);
		}
	}

	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, ID3D11RenderTargetView* targetRtv)
	{
		// set the shader objects avtive
		Shader* shader = shaders->getShader("mainPass");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		// constant buffers
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbs.settingsCb);
		d3d.deviceContext->PSSetConstantBuffers(0, 1, &cbs.settingsCb);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);
		d3d.deviceContext->PSSetConstantBuffers(1, 1, &cbs.cameraCb);

		// lightmaps
		d3d.deviceContext->PSSetShaderResources(1, 1, &lightmapTexArray);

		// vertex buffers
		for (auto& mesh : world.meshes) {
			auto* resourceView = mesh.baseColor->GetResourceView();
			d3d.deviceContext->PSSetShaderResources(0, 1, &resourceView);

			UINT strides[] = { sizeof(VERTEX_POS), sizeof(VERTEX_OTHER) };
			UINT offsets[] = { 0, 0 };
			ID3D11Buffer* vertexBuffers[] = { mesh.vertexBufferPos, mesh.vertexBufferOther };

			d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
			d3d.deviceContext->Draw(mesh.vertexCount, 0);
		}
	}

	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		Shader* shader = shaders->getShader("wireframe");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);

		// vertex buffers
		for (auto& mesh : world.meshes) {
			UINT strides[] = { sizeof(VERTEX_POS), sizeof(VERTEX_OTHER) };
			UINT offsets[] = { 0, 0 };
			ID3D11Buffer* vertexBuffers[] = { mesh.vertexBufferPos, mesh.vertexBufferOther };

			d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
			d3d.deviceContext->Draw(mesh.vertexCount, 0);
		}
	}

	void clean()
	{
		release(lightmapTexArray);
		release(linearSamplerState);

		for (auto& tex : debugTextures) {
			delete tex;
		}
		for (auto& mesh : world.meshes) {
			mesh.release();
		}
		for (auto& tex : textureCache) {
			delete tex.second;
		}
	}
}