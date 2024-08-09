#include "stdafx.h"
#include "PassWorldLoader.h"

#include "assets/AssetCache.h"
#include "assets/AssetFinder.h"
#include "assets/ZenLoader.h"
#include "assets/ObjLoader.h"
#include "assets/TexFromVdfLoader.h"

#include "Util.h"
#include "render/RenderUtil.h"

namespace render::pass::world
{
	using namespace render;
	using namespace DirectX;
	using namespace ZenLib;
	using ::std::string;
	using ::std::vector;
	using ::std::unordered_map;

	struct LoadResult {
		uint32_t loadedDraws = 0;
		uint32_t loadedVerts = 0;
	};

	const TEX_INDEX texturesPerBatch = 256;

	World world;

	// Texture Ownership
	// BaseColor textures are owned by textureCache
	// Lightmap textures are owned by world
	//   - debugTextures -> single textures used by ImGUI
	//   - lightmapTexArray -> array used by forward renderer

	unordered_map<TexId, Texture*> textureCache;

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

	Texture* getOrCreateTexture(D3d d3d, TexId texId)
	{
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
		util::createVertexBuffer(d3d, mesh.vbPos, allVerts);
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

	void loadBatch(D3d d3d, vector<MeshBatch>& target, TexInfo batchInfo, const VEC_VERTEX_DATA_BATCH& batchData)
	{
		MeshBatch batch;
		batch.vertexCount = batchData.vecPos.size();
		util::createVertexBuffer(d3d, batch.vbPos, batchData.vecPos);
		util::createVertexBuffer(d3d, batch.vbOther, batchData.vecNormalUv);
		util::createVertexBuffer(d3d, batch.vbTexIndices, batchData.texIndices);
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
			if (!batchData.vecPos.empty()) {
				loadBatch(d3d, target, info, batchData);
				result.loadedDraws++;
				result.loadedVerts += batchData.vecPos.size();
			}
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

	void clearLevel()
	{
		for (auto& mesh : world.meshBatchesWorld) {
			mesh.release();
		}
		for (auto& mesh : world.meshBatchesObjects) {
			mesh.release();
		}
		for (auto& mesh : world.prepassMeshes) {
			mesh.release();
		}
		for (auto& tex : world.debugTextures) {
			delete tex;
		}
		world.meshBatchesWorld.clear();
		world.meshBatchesObjects.clear();
		world.prepassMeshes.clear();
		world.debugTextures.clear();

		release(world.lightmapTexArray);
	}

	void loadLevel(D3d d3d, const string& levelStr)
	{
		bool levelDataFound = false;
		RenderData data;
		string level = ::util::asciiToLower(levelStr);

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

		clearLevel();
		world.isOutdoorLevel = data.isOutdoorLevel;
		{
			vector<vector<uint8_t>> ddsRaws;
			for (auto& lightmap : data.worldMeshLightmaps) {
				ddsRaws.push_back(lightmap.ddsRaw);

				Texture* texture = new Texture(d3d, lightmap.ddsRaw, false, "lightmap_xxx");
				world.debugTextures.push_back(texture);
			}
			world.lightmapTexArray = createShaderTexArray(d3d, ddsRaws, 256, 256, true);
		}

		LoadResult loadResult;
		loadResult = loadPrepassData(d3d, world.prepassMeshes, data.worldMesh);
		LOG(INFO) << "Render Data Loaded - Depth Prepass    - Draws: " << loadResult.loadedDraws << " Verts: " << loadResult.loadedVerts;

		loadResult = loadVertexDataBatched(d3d, world.meshBatchesWorld, data.worldMesh);
		LOG(INFO) << "Render Data Loaded - World Mesh       - Draws: " << loadResult.loadedDraws << " Verts: " << loadResult.loadedVerts;

		loadResult = loadVertexDataBatched(d3d, world.meshBatchesObjects, data.staticMeshes);
		LOG(INFO) << "Render Data Loaded - Static Instances - Draws: " << loadResult.loadedDraws << " Verts: " << loadResult.loadedVerts;

		// since single textures have been copied to texture arrays, we can release them
		for (auto& tex : textureCache) {
			delete tex.second;
		}
		textureCache.clear();
	}
}