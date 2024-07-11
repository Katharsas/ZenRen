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
#include "assets/AssetCache.h"
#include "assets/AssetFinder.h"
#include "assets/ZenLoader.h"
#include "assets/ObjLoader.h"
#include "assets/TexFromVdfLoader.h"


// TODO move to RenderDebugGui
#include "../Gui.h"
#include "imgui/imgui.h"
#include "imgui/imgui_custom.h"

namespace render::pass::world
{
	using namespace DirectX;
	using ::std::unordered_map;
	using ::std::vector;

	struct World {
		vector<PrepassMeshes> prepassMeshes;
		vector<MeshBatch> meshBatchesWorld;
		vector<MeshBatch> meshBatchesObjects;
		XMMATRIX transform;
	};

	struct LoadResult {
		uint32_t loadedDraws = 0;
		uint32_t loadedVerts = 0;
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

	std::unordered_map<TexId, Texture*> textureCache;

	ID3D11SamplerState* linearSamplerState = nullptr;
	ID3D11ShaderResourceView* lightmapTexArray = nullptr;

	vector<Texture*> debugTextures;
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

				ImGui::PushStyleColorDebugText();
				ImGui::Checkbox("Draw Worlds", &worldSettings.drawWorld);
				ImGui::Checkbox("Draw VOBs/MOBs", &worldSettings.drawStaticObjects);
				ImGui::PopStyleColor();
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

	Texture* createTexture(D3d d3d, TexId texId)
	{
		const auto texName = ::assets::getTexName(texId);

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

	Texture* getOrCreateTexture(D3d d3d, TexId texId) {
		return ::util::getOrSet(textureCache, texId, [&](Texture*& ref) { ref = createTexture(d3d, texId); });
	}

	LoadResult loadPrepassData(D3d d3d, vector<PrepassMeshes>& target, const VERTEX_DATA_BY_MAT& meshData)
	{
		uint32_t vertCount = 0;
		vector<VERTEX_POS> allVerts;

		for (const auto& pair : meshData) {
			auto& vecPos = pair.second.vecPos;
			if (!vecPos.empty()) {
				Texture* texture = getOrCreateTexture(d3d, pair.first.texBaseColor);
				if (!texture->getInfo().hasAlpha) {
					::util::insert(allVerts, vecPos);
					vertCount += vecPos.size();
				}
			}
		}
		PrepassMeshes mesh;
		mesh.vertexCount = allVerts.size();
		util::createVertexBuffer(d3d, &mesh.vertexBufferPos, allVerts);
		target.push_back(mesh);

		return { 1, vertCount };
	}

	void initDxTextureArray(D3d d3d, ID3D11ShaderResourceView** targetSrv, const TexInfo& info, uint32_t arraySize)
	{
		D3D11_TEXTURE2D_DESC texDesc = CD3D11_TEXTURE2D_DESC(info.format, info.width, info.height);
		texDesc.MipLevels = info.mipLevels;
		texDesc.ArraySize = arraySize;

		ID3D11Texture2D* targetTex;
		auto hr = d3d.device->CreateTexture2D(&texDesc, nullptr, &targetTex);
		::util::throwOnError(hr, "Failed to initialize texture array resource!");

		D3D11_SHADER_RESOURCE_VIEW_DESC descSrv = CD3D11_SHADER_RESOURCE_VIEW_DESC(
			D3D11_SRV_DIMENSION_TEXTURE2DARRAY,
			info.format,
			0, info.mipLevels,
			0, arraySize
		);

		hr = d3d.device->CreateShaderResourceView((ID3D11Resource*)targetTex, &descSrv, targetSrv);
		::util::throwOnError(hr, "Failed to initialize texture array SRV!");

		targetTex->Release();
	}

	void createTexArray(D3d d3d, ID3D11ShaderResourceView** targetSrv, const TexInfo& info, const vector<TexId>& texIds)
	{
		initDxTextureArray(d3d, targetSrv, info, texIds.size());
		ID3D11Resource* targetTex;
		(*targetSrv)->GetResource(&targetTex);

		// https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-subresources
		uint32_t currentTex = 0;

		for (const auto texId : texIds) {
			ID3D11Resource* source;
			const Texture* tex = getOrCreateTexture(d3d, texId);
			tex->GetResourceView()->GetResource(&source);

			for (uint32_t mipLevel = 0; mipLevel < info.mipLevels; mipLevel++) {
				d3d.deviceContext->CopySubresourceRegion(targetTex, (currentTex * info.mipLevels) + mipLevel, 0, 0, 0, source, mipLevel, NULL);
			}

			currentTex++;
		}
	}

	void loadBatch(D3d d3d, vector<MeshBatch>& target, TexInfo batchInfo, const VEC_VERTEX_DATA_BATCH& batchData) {
		MeshBatch batch;
		batch.vertexCount = batchData.vecPos.size();
		util::createVertexBuffer(d3d, &batch.vertexBufferPos, batchData.vecPos);
		util::createVertexBuffer(d3d, &batch.vertexBufferOther, batchData.vecNormalUv);
		util::createVertexBuffer(d3d, &batch.vertexBufferTexIndices, batchData.texIndices);
		createTexArray(d3d, &batch.texColorArray, batchInfo, batchData.texIndexedIds);
		target.push_back(batch);
	}

	LoadResult loadVertexDataBatched(D3d d3d, vector<MeshBatch>& target, const VERTEX_DATA_BY_MAT& meshData)
	{
		// load and bucket all materials so textures that are texture-array-compatible are grouped in a single bucket
		unordered_map<TexInfo, vector<Material>> texBuckets;

		for (const auto& [mat, data] : meshData) {
			if (!data.vecPos.empty()) {
				Texture* texture = getOrCreateTexture(d3d, mat.texBaseColor);
				auto& vec = ::util::getOrCreate(texBuckets, texture->getInfo());
				vec.push_back(mat);
			}
		}

		const TEX_INDEX texturesPerBatch = 128;
		LoadResult result;

		// create batches
		for (const auto& [info, textures] : texBuckets) {

			VEC_VERTEX_DATA_BATCH batchData;
			TEX_INDEX currentIndex = 0;

			for (const auto mat : textures) {

				VEC_VERTEX_DATA curentMatVertices = meshData.find(mat)->second;
				::util::insert(batchData.vecPos, curentMatVertices.vecPos);
				::util::insert(batchData.vecNormalUv, curentMatVertices.vecNormalUv);
				batchData.texIndices.insert(batchData.texIndices.end(), curentMatVertices.vecPos.size(), currentIndex);
				batchData.texIndexedIds.push_back(mat.texBaseColor);

				// create and start new batch because we reached max texture size per batch
				if (currentIndex + 1 >= texturesPerBatch) {
					loadBatch(d3d, target, info, batchData);
					result.loadedDraws++;
					result.loadedVerts += batchData.vecPos.size();

					batchData = VEC_VERTEX_DATA_BATCH();
					currentIndex = 0;
				}
				else {
					currentIndex++;
				}
			}

			// create batch for leftover textures from this bucket
			loadBatch(d3d, target, info, batchData);
			result.loadedDraws++;
			result.loadedVerts += batchData.vecPos.size();
		}

		return result;
	}

	// TODO remove this, this is identical to setting texturesPerBatch to 1
	LoadResult loadVertexData(D3d d3d, vector<MeshBatch>& target, const VERTEX_DATA_BY_MAT& meshData)
	{
		LoadResult result;

		for (const auto& pair : meshData) {
			auto& material = pair.first;
			auto& matData = pair.second;

			if (!matData.vecPos.empty()) {
				VEC_VERTEX_DATA_BATCH batchData;

				uint32_t vertCount = matData.vecPos.size();
				batchData.vecPos = matData.vecPos;
				batchData.vecNormalUv = matData.vecNormalUv;
				batchData.texIndices.insert(batchData.texIndices.end(), vertCount, 0);
				batchData.texIndexedIds = { material.texBaseColor };

				Texture* texture = getOrCreateTexture(d3d, material.texBaseColor);
				loadBatch(d3d, target, texture->getInfo(), batchData);
				result.loadedDraws++;
				result.loadedVerts += batchData.vecPos.size();
			}
		}
		return result;
	}

	void releaseClearMeshData() {
		for (auto& mesh : world.meshBatchesWorld) {
			mesh.release();
		}
		for (auto& mesh : world.meshBatchesObjects) {
			mesh.release();
		}
		for (auto& mesh : world.prepassMeshes) {
			mesh.release();
		}
		world.meshBatchesWorld.clear();
		world.meshBatchesObjects.clear();
		world.prepassMeshes.clear();
	}

	void loadLevel(D3d d3d, std::string& level)
	{
		bool levelDataFound = false;
		RenderData data;
		::util::asciiToLowerMut(level);

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

			vector<vector<uint8_t>> ddsRaws;
			for (auto& lightmap : data.worldMeshLightmaps) {
				ddsRaws.push_back(lightmap.ddsRaw);

				Texture* texture = new Texture(d3d, lightmap.ddsRaw, false, "lightmap_xxx");
				debugTextures.push_back(texture);
			}
			lightmapTexArray = createShaderTexArray(d3d, ddsRaws, 256, 256, true);
		}
		
		releaseClearMeshData();

		LoadResult loadResult;
		loadResult = loadPrepassData(d3d, world.prepassMeshes, data.worldMesh);
		//loadedCount += loadPrepassData(d3d, world.prepassMeshes, data.staticMeshes);// for now we only use world mesh in depth prepass
		LOG(INFO) << "Render Data Loaded - Depth Prepass    - Draws: " << loadResult.loadedDraws << " Verts: " << loadResult.loadedVerts;

		loadResult = loadVertexDataBatched(d3d, world.meshBatchesWorld, data.worldMesh);
		LOG(INFO) << "Render Data Loaded - World Mesh       - Draws: " << loadResult.loadedDraws << " Verts: " << loadResult.loadedVerts;

		loadResult = loadVertexDataBatched(d3d, world.meshBatchesObjects, data.staticMeshes);
		LOG(INFO) << "Render Data Loaded - Static Instances - Draws: " << loadResult.loadedDraws << " Verts: " << loadResult.loadedVerts;

		// since single textures have been copied to texture arrays, we can release them
		for (auto& tex : textureCache) {
			delete tex.second;
		}
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

	void drawVertexBuffersPosOnly(D3d d3d, vector<PrepassMeshes> batches)
	{
		UINT strides[] = { sizeof(VERTEX_POS) };
		UINT offsets[] = { 0 };
		for (auto& batch : batches) {
			ID3D11Buffer* vertexBuffers[] = { batch.vertexBufferPos };
			d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
			d3d.deviceContext->Draw(batch.vertexCount, 0);
		}
	}

	void drawVertexBuffersNoTex(D3d d3d, vector<MeshBatch> batches)
	{
		UINT strides[] = { sizeof(VERTEX_POS), sizeof(VERTEX_OTHER) };
		UINT offsets[] = { 0, 0 };
		for (auto& batch : batches) {
			ID3D11Buffer* vertexBuffers[] = { batch.vertexBufferPos, batch.vertexBufferOther };
			d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
			d3d.deviceContext->Draw(batch.vertexCount, 0);
		}
	}

	void drawVertexBuffersFull(D3d d3d, vector<MeshBatch> batches)
	{
		UINT strides[] = { sizeof(VERTEX_POS), sizeof(VERTEX_OTHER), sizeof(TEX_INDEX) };
		UINT offsets[] = { 0, 0, 0 };
		for (auto& batch : batches) {
			auto* resourceView = batch.texColorArray;
			d3d.deviceContext->PSSetShaderResources(0, 1, &resourceView);

			ID3D11Buffer* vertexBuffers[] = { batch.vertexBufferPos, batch.vertexBufferOther, batch.vertexBufferTexIndices };
			d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
			d3d.deviceContext->Draw(batch.vertexCount, 0);
		}
	}

	void drawPrepass(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		Shader* shader = shaders->getShader("depthPrepass");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(nullptr, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);

		drawVertexBuffersPosOnly(d3d, world.prepassMeshes);
	}

	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		Shader* shader = shaders->getShader("wireframe");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);

		if (worldSettings.drawWorld) {
			drawVertexBuffersNoTex(d3d, world.meshBatchesWorld);
		}
		if (worldSettings.drawStaticObjects) {
			drawVertexBuffersNoTex(d3d, world.meshBatchesObjects);
		}
	}

	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
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

		if (worldSettings.drawWorld) {
			drawVertexBuffersFull(d3d, world.meshBatchesWorld);
		}
		if (worldSettings.drawStaticObjects) {
			drawVertexBuffersFull(d3d, world.meshBatchesObjects);
		}
	}

	void clean()
	{
		release(lightmapTexArray);
		release(linearSamplerState);
		releaseClearMeshData();

		for (auto& tex : debugTextures) {
			delete tex;
		}
		for (auto& tex : textureCache) {
			delete tex.second;
		}
	}
}