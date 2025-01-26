#include "stdafx.h"
#include "PassWorldLoader.h"

#include "render/d3d/TextureBuffer.h"
#include "render/PerfStats.h"
#include "render/Loader.h"
#include "PassWorldChunkGrid.h"

#include "assets/AssetCache.h"
#include "assets/AssetFinder.h"
#include "assets/ZenLoader.h"
#include "assets/ZenKitLoader.h"
#include "assets/ObjLoader.h"
#include "assets/TexLoader.h"

#include "Util.h"
#include "Win.h"
#include "render/d3d/Buffer.h"

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
		std::string texName(::assets::getTexName(texId));

		// texture name is alwyas TGA, and assetFinder assumes we lookup with TGA extension even if file is not TGA (TODO change this maybe?)
		// only if file could not be found as source file, we have to check for compiled variant because asset finder does not handle that
		auto sourceAssetOpt = assets::getIfExists(texName);
		if (sourceAssetOpt.has_value()) {
			auto fileData = assets::getData(sourceAssetOpt.value());
			return assets::createTextureFromImageFormat(d3d, fileData);
		}
		texName = ::util::replaceExtension(texName, "-c.tex");// compiled textures have -C suffix
		auto compiledAssetOpt = assets::getIfExists(texName);
		if (compiledAssetOpt.has_value()) {
			FileData file = assets::getData(compiledAssetOpt.value());
			return assets::createTextureFromGothicTex(d3d, file);
		}
		return assets::createDefaultTexture(d3d);
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
		d3d::createVertexBuf(d3d, mesh.vbPos, allVerts);
		target.push_back(mesh);

		return { 1, vertCount };
	}

	void createTexArray(D3d d3d, ID3D11ShaderResourceView** targetSrv, const TexInfo& info, const vector<TexId>& texIds)
	{
		vector<ID3D11Texture2D*> sourceBuffers;
		for (const auto texId : texIds) {
			sourceBuffers.push_back(nullptr);
			const Texture* tex = getOrCreateTexture(d3d, texId);
			tex->GetResourceView()->GetResource((ID3D11Resource**)&sourceBuffers.back());
		}

		ID3D11Texture2D* texArrayBuf = nullptr;
		d3d::createTexture2dArrayBufByCopy(d3d, &texArrayBuf, sourceBuffers, BufferUsage::WRITE_GPU);
		d3d::createTexture2dArraySrv(d3d, targetSrv, texArrayBuf);

		release(texArrayBuf);
		for (auto& sourceBuffer : sourceBuffers) {
			release(sourceBuffer);
		}
	}

	void loadRenderBatch(D3d d3d, vector<MeshBatch>& target, TexInfo batchInfo, const VEC_VERTEX_DATA_BATCH& batchData)
	{
		MeshBatch batch;
		batch.vertClusters = batchData.vertClusters;
		batch.vertexCount = batchData.vecPos.size();
		d3d::createVertexBuf(d3d, batch.vbPos, batchData.vecPos);
		d3d::createVertexBuf(d3d, batch.vbOther, batchData.vecOther);
		d3d::createVertexBuf(d3d, batch.vbTexIndices, batchData.texIndices);
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

	void clearZenLevel()
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

	LoadWorldResult loadZenLevel(D3d d3d, const string& levelStr)
	{
		auto samplerTotal = render::stats::TimeSampler();
		samplerTotal.start();
		auto sampler = render::stats::TimeSampler();
		sampler.start();

		bool levelDataFound = false;
		RenderData data;
		string level = ::util::asciiToLower(levelStr);

		if (!::util::endsWith(level, ".obj") && !::util::endsWith(level, ".zen")) {
			LOG(WARNING) << "Level file format not supported: " << level;
		}

		auto levelFileOpt = assets::getIfExists(level);
		if (levelFileOpt.has_value()) {
			if (::util::endsWith(level, ".zen")) {
				auto levelFile = levelFileOpt.value();
				if (levelFile.node != nullptr) {
					assets::loadZen2(data, levelFile);
				}
				else {
					assets::loadZen(data, levelFile, assets::getVfsLegacy());
				}
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
			return { .loaded = false };
		}

		sampler.logMillisAndRestart("Level: Loaded all data");

		clearZenLevel();
		world.isOutdoorLevel = data.isOutdoorLevel;
		{
			vector<Texture*> lightmaps = assets::createTexturesFromLightmaps(d3d, std::move(data.worldMeshLightmaps));
			for (auto& lightmap : lightmaps) {
				world.debugTextures.push_back(lightmap);
			}
			if (!lightmaps.empty()) {
				vector<ID3D11Texture2D*> buffers;
				for (auto * texture : lightmaps) {
					ID3D11Resource* resoucePtr;
					texture->GetResourceView()->GetResource(&resoucePtr);
					buffers.push_back((ID3D11Texture2D*)resoucePtr);
				}
				ID3D11Texture2D* texArray = nullptr;
				d3d::createTexture2dArrayBufByCopy(d3d, &texArray, buffers, BufferUsage::WRITE_GPU);
				for (auto * buffer : buffers) {
					release(buffer);
				}
				d3d::createTexture2dArraySrv(d3d, &world.lightmapTexArray, texArray);
				release(texArray);
			}
		}
		sampler.logMillisAndRestart("Level: Uploaded lightmap textures");

		// chunk grid
		chunkgrid::updateSize(data.worldMesh);
		chunkgrid::updateSize(data.staticMeshes);
		uint32_t cellCount = chunkgrid::finalizeSize();
		chunkgrid::updateMesh(data.worldMesh);
		chunkgrid::updateMesh(data.staticMeshes);
		LOG(INFO) << "Level: Computed Chunk Grid       - Cells: " << cellCount;
		sampler.logMillisAndRestart("Level: Computed chunk grid");

		LoadResult loadResult;
		// TODO fix prepass
		//loadResult = loadPrepassData(d3d, world.prepassMeshes, data.worldMesh);
		LOG(INFO) << "Level: Uploaded depth prepass    - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		sampler.logMillisAndRestart("Level: Uploaded depth prepass");

		loadResult = loadVertexDataBatched(d3d, world.meshBatchesWorld, data.worldMesh, texturesPerBatch);
		sortByVertCount(world.meshBatchesWorld);// improves performance
		LOG(INFO) << "Level: Uploaded world mesh       - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		sampler.logMillisAndRestart("Level: Uploaded world mesh");

		loadResult = loadVertexDataBatched(d3d, world.meshBatchesObjects, data.staticMeshes, texturesPerBatch);
		LOG(INFO) << "Level: Uploaded static instances - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		sampler.logMillisAndRestart("Level: Uploaded static instances");

		// since single textures have been copied to texture arrays, we can release them
		for (auto& tex : textureCache) {
			delete tex.second;
		}
		textureCache.clear();

		samplerTotal.logMillisAndRestart("Level complete");

		return {
			.loaded = true,
			.isOutdoorLevel = world.isOutdoorLevel
		};
	}
}