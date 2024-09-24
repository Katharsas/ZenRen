#include "stdafx.h"
#include "PassWorldLoader.h"

#include "PassWorldChunkGrid.h"

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
	using ::std::pair;

	struct LoadResult {
		uint32_t states = 0;
		uint32_t draws = 0;
		uint32_t verts = 0;

		auto operator+=(const LoadResult& rhs)
		{
			states += rhs.states;
			draws += rhs.draws;
			verts += rhs.verts;
		};
	};

	const TEX_INDEX texturesPerBatch = 512;
	const uint32_t vertCountPerBatch = (20 * 1024 * 1024) / sizeof(VERTEX_OTHER);// 20 MB divided by biggest buffer element size

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

	struct TextureCreator {
		D3d d3d;
		TexId texId;
		void operator()(Texture*& ref) {
			ref = createTexture(d3d, texId);
		}
	};

	Texture* getOrCreateTexture(D3d d3d, TexId texId)
	{
		return ::util::createOrGet<TexId, Texture*>(textureCache, texId, [&](Texture*& ref) { ref = createTexture(d3d, texId); });
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

	void loadRenderBatch(D3d d3d, vector<MeshBatch>& target, TexInfo batchInfo, const VEC_VERTEX_DATA_BATCH& batchData)
	{
		MeshBatch batch;
		batch.vertClusters = batchData.vertClusters;
		batch.vertexCount = batchData.vecPos.size();
		util::createVertexBuffer(d3d, batch.vbPos, batchData.vecPos);
		util::createVertexBuffer(d3d, batch.vbOther, batchData.vecOther);
		util::createVertexBuffer(d3d, batch.vbTexIndices, batchData.texIndices);
		createTexArray(d3d, &batch.texColorArray, batchInfo, batchData.texIndexedIds);
		target.push_back(batch);
	}

	vector<pair<TexInfo, vector<pair<Material, const VERT_CHUNKS *>>>> groupByTexId(D3d d3d, const VERT_CHUNKS_BY_MAT& meshData, TEX_INDEX maxTexturesPerBatch)
	{
		// load and bucket all materials so textures that are texture-array-compatible are grouped in a single bucket
		unordered_map<TexInfo, vector<Material>> texBuckets;
		for (const auto& [mat, data] : meshData) {
			Texture* texture = getOrCreateTexture(d3d, mat.texBaseColor);
			auto& vec = ::util::getOrCreateDefault(texBuckets, texture->getInfo());
			vec.push_back(mat);
		}

		vector<pair<TexInfo, vector<pair<Material, const VERT_CHUNKS*>>>> result;

		// create batches
		for (const auto& [texInfo, textures] : texBuckets) {

			pair<TexInfo, vector<pair<Material, const VERT_CHUNKS*>>> batch = { texInfo, {} };
			TEX_INDEX currentIndex = 0;

			for (const auto mat : textures) {
				const VERT_CHUNKS& currentMatData = meshData.find(mat)->second;
				batch.second.push_back(pair{ mat, &currentMatData });

				// create and start new batch because we reached max texture size per batch
				if (currentIndex + 1 >= maxTexturesPerBatch) {
					result.push_back(batch);
					batch = { texInfo, {} };

					currentIndex = 0;
				}
				else {
					currentIndex++;
				}
			}

			// create batch for leftover textures from this bucket
			if (!batch.second.empty()) {
				result.push_back(batch);
				batch = { texInfo, {} };
			}
		}

		return result;
	}

	bool compareByChunkIndex(pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>> const& lhs, pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>> const& rhs) {
		const ChunkIndex& lhsIndex = lhs.first;
		const ChunkIndex& rhsIndex = rhs.first;
		if (lhsIndex.y == rhsIndex.y) {
			return lhsIndex.x < rhsIndex.x;
		}
		return lhsIndex.y < rhsIndex.y;
	}

	vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>> groupAndSortByChunkIndex(const vector<pair<Material, const VERT_CHUNKS*>>& batchData)
	{
		// use unordered_map to group
		unordered_map<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>> chunkBuckets;

		for (const auto& [mat, chunkData] : batchData) {
			for (const auto& [chunkIndex, chunkVerts] : *chunkData) {

				auto& vec = ::util::getOrCreateDefault(chunkBuckets, chunkIndex);
				vec.push_back({ mat, chunkVerts });
			}
		}

		// convert unordered_map to vector for sorting
		vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>> result;
		result.reserve(chunkBuckets.size());

		for (const auto& [chunkIndex, chunkData] : chunkBuckets) {
			result.push_back({chunkIndex, chunkData});
		}

		// ideally we would maybe sort by morton code or something like that (implement "uint32_t getMortonIndex(ChunkIndex)" in ChunkGrid or similar)
		// for now we just sort by y then x which already reduces number of draw calls significantly (due to vert range merging of continuous active grid cells)
		std::sort(result.begin(), result.end(), &compareByChunkIndex);

		return result;
	}

	vector<pair<uint32_t, vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>>>> splitByVertCount(
		const vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>>& batchData, uint32_t maxVertCount)
	{
		vector<pair<uint32_t, vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>>>> result;

		vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>> currentBatch;
		uint32_t currentBatchVertCount = 0;

		for (const auto& [chunkIndex, vertDataByMat] : batchData) {

			uint32_t chunkVertCount = 0;
			for (const auto& [material, vertData] : vertDataByMat) {
				chunkVertCount += vertData.vecPos.size();
			}

			// we never split a single chunk, so if the first chunk of a batch has more than maxVertCount verts we accept that
			if (currentBatchVertCount != 0 && (currentBatchVertCount + chunkVertCount) > maxVertCount) {
				result.push_back({ currentBatchVertCount, currentBatch });
				currentBatch.clear();
				currentBatchVertCount = 0;
			}
			currentBatch.push_back({ chunkIndex, vertDataByMat });
			currentBatchVertCount += chunkVertCount;
		}

		if (currentBatchVertCount != 0) {
			result.push_back({ currentBatchVertCount, currentBatch });
		}

		return result;
	}

	pair<VEC_VERTEX_DATA_BATCH, LoadResult> flattenIntoBatch(const vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>>& batchData)
	{
		LoadResult result;
		result.states = 1;
		VEC_VERTEX_DATA_BATCH target;

		uint32_t clusterCount = batchData.size();
		result.draws = clusterCount;
		target.vertClusters.reserve(clusterCount);

		unordered_map<Material, TEX_INDEX> materialIndices;
		vector<Material> materials;

		// reserve to avoid over-allocation (because the resulting vert vectors are going to be very big)
		uint32_t vertCount = 0;
		for (const auto& [chunkIndex, vertDataByMat] : batchData) {
			for (const auto& [material, vertData] : vertDataByMat) {
				vertCount += vertData.vecPos.size();
			}
		}
		target.vecPos.reserve(vertCount);
		target.vecOther.reserve(vertCount);
		target.texIndices.reserve(vertCount);

		for (const auto& [chunkIndex, vertDataByMat] : batchData) {
			// set vertex data
			uint32_t currentVertIndex = target.vecPos.size();
			target.vertClusters.push_back({ chunkIndex, currentVertIndex });

			for (const auto& [material, vertData] : vertDataByMat) {
				::util::insert(target.vecPos, vertData.vecPos);
				::util::insert(target.vecOther, vertData.vecOther);

				TEX_INDEX texIndex = ::util::getOrCreate<Material, TEX_INDEX>(materialIndices, material, [&]() -> TEX_INDEX {
					materials.push_back(material);
					return (TEX_INDEX) materials.size() - 1;
				});
				target.texIndices.insert(target.texIndices.end(), vertData.vecPos.size(), texIndex);
			}
		}

		for (const Material& material : materials) {
			target.texIndexedIds.push_back(material.texBaseColor);
		}

		result.verts = target.vecPos.size();
		return { target, result };
	}

	LoadResult loadVertexDataBatched(D3d d3d, vector<MeshBatch>& target, const VERT_CHUNKS_BY_MAT& meshData, TEX_INDEX maxTexturesPerBatch)
	{
		vector<pair<TexInfo, vector<pair<Material, const VERT_CHUNKS*>>>> batchedMeshData = groupByTexId(d3d, meshData, maxTexturesPerBatch);

		LoadResult result;
		for (const auto& [texInfo, batchData] : batchedMeshData) {

			vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>> batchDataByChunk = groupAndSortByChunkIndex(batchData);
			
			// split current batch into multiple smaller batches along chunk boundaries if it contains too many verts to prevent OOM crashes
			vector<pair<uint32_t, vector<pair<ChunkIndex, vector<pair<Material, VEC_VERTEX_DATA>>>>>> batchDataSplit =
				splitByVertCount(batchDataByChunk, vertCountPerBatch);

			batchDataByChunk.clear();// lots of memory that are no longer needed

			for (const auto& [vertCount, batchData] : batchDataSplit) {
				const auto [batchDataFlat, batchLoadResult] = flattenIntoBatch(batchData);

				assert(vertCount == batchLoadResult.verts);
				result += batchLoadResult;
				loadRenderBatch(d3d, target, texInfo, batchDataFlat);
			}
		}

		return result;
	}

	bool compareByVertCount(MeshBatch const& lhs, MeshBatch const& rhs) {
		return lhs.vertexCount > rhs.vertexCount;
	}

	void sortByVertCount(vector<MeshBatch>& target)
	{
		std::sort(target.begin(), target.end(), &compareByVertCount);
	}


	// TODO
	// This is pretty useless until we actually have dynamic objects that cannot be pre-loaded into grid cells,
	// and having those would require a mesh type that does not have/support ChunkIndices at all (and logic in draw to account for that)
	LoadResult loadVertexData(D3d d3d, vector<MeshBatch>& target, const VERTEX_DATA_BY_MAT& meshData)
	{
		LoadResult result;

		for (const auto& pair : meshData) {
			auto& material = pair.first;
			auto& matData = pair.second;

			if (!matData.vecPos.empty()) {
				VEC_VERTEX_DATA_BATCH batchData;

				uint32_t vertCount = matData.vecPos.size();
				batchData.vertClusters.push_back({ {0, 0}, 0 });
				batchData.vecPos = matData.vecPos;
				batchData.vecOther = matData.vecOther;
				batchData.texIndices.insert(batchData.texIndices.end(), vertCount, 0);
				batchData.texIndexedIds = { material.texBaseColor };

				Texture* texture = getOrCreateTexture(d3d, material.texBaseColor);
				loadRenderBatch(d3d, target, texture->getInfo(), batchData);
				result.states++;
				result.draws++;
				result.verts += batchData.vecPos.size();
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
				//data = { true, assets::loadObj(::util::toString(*optionalFilepath.value())) };
				levelDataFound = true;
			}
			else {
				// TODO support loading ZEN files
				LOG(WARNING) << "Loading from single level file not supported: " << level;
			}
		}
		else if (optionalVdfIndex.has_value()) {
			if (::util::endsWith(level, ".zen")) {
				assets::loadZen(data, level, optionalVdfIndex.value());
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

		// chunk grid
		chunkgrid::updateSize(data.worldMesh);
		uint32_t cellCount = chunkgrid::finalizeSize();
		chunkgrid::updateMesh(data.worldMesh);
		LOG(INFO) << "Level Loaded - Chunk Grid       - Cells: " << cellCount;

		LoadResult loadResult;
		// TODO fix prepass
		//loadResult = loadPrepassData(d3d, world.prepassMeshes, data.worldMesh);
		LOG(INFO) << "Level Loaded - Depth Prepass    - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		
		loadResult = loadVertexDataBatched(d3d, world.meshBatchesWorld, data.worldMesh, texturesPerBatch);
		sortByVertCount(world.meshBatchesWorld);// improves performance
		LOG(INFO) << "Level Loaded - World Mesh       - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		
		loadResult = loadVertexDataBatched(d3d, world.meshBatchesObjects, data.staticMeshes, texturesPerBatch);
		LOG(INFO) << "Level Loaded - Static Instances - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;

		// since single textures have been copied to texture arrays, we can release them
		for (auto& tex : textureCache) {
			delete tex.second;
		}
		textureCache.clear();
	}
}