#include "stdafx.h"
#include "PassWorldLoader.h"

#include "render/d3d/TextureBuffer.h"
#include "render/d3d/GeometryBuffer.h"
#include "render/PerfStats.h"
#include "render/Loader.h"
#include "PassWorldChunkGrid.h"

#include "assets/AssetCache.h"
#include "assets/AssetFinder.h"
#include "assets/ZenLoader.h"
//#include "assets/ObjLoader.h"
#include "assets/TexLoader.h"

#include "Util.h"
#include "Win.h"
#include "render/d3d/Buffer.h"

namespace render::pass::world
{
	using namespace render;
	using namespace DirectX;
	using ::std::string;
	using ::std::vector;
	using ::std::unordered_map;
	using ::std::pair;
	using ::std::array;

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
	const uint32_t vertCountPerBatch = (20 * 1024 * 1024) / sizeof(VertexBasic);// 20 MB divided by biggest buffer element size

	World world;

	// Texture Ownership
	// BaseColor textures are owned by textureCache
	// Lightmap textures are owned by world
	//   - debugTextures -> single textures used by ImGUI
	//   - lightmapTexArray -> array used by forward renderer

	unordered_map<TexId, Texture*> textureCache;

	Texture* getOrCreateTexture(D3d d3d, TexId texId, bool srgb)
	{
		return ::util::createOrGet<TexId, Texture*>(textureCache, texId, [&](Texture*& ref) {
			std::string texName(::assets::getTexName(texId));
			ref = assets::createTextureOrDefault(d3d, texName, srgb);
		});
	}

	LoadResult loadPrepassData(D3d d3d, vector<PrepassMeshes>& target, const MatToVertsBasic& meshData)
	{
		uint32_t vertCount = 0;
		vector<VertexPos> allVerts;

		for (const auto& pair : meshData) {
			auto& vecPos = pair.second.vecPos;
			if (!vecPos.empty()) {
				Texture* texture = getOrCreateTexture(d3d, pair.first.texBaseColor, true);
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
			const Texture* tex = getOrCreateTexture(d3d, texId, info.srgb);
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

	template <VERTEX_FEATURE F>
	void loadRenderBatch(D3d d3d, vector<MeshBatch<F>>& target, TexInfo batchInfo, const VertsBatch<F>& batchData)
	{
		MeshBatch<F> batch;
		batch.vertClusters = batchData.vertClusters;
		batch.useIndices = !batchData.vecIndex.empty();
		if (batch.useIndices) {
			batch.vertexCount = batchData.vecIndex.size();
			d3d::createIndexBuf(d3d, batch.vbIndices, batchData.vecIndex);
		}
		else {
			batch.vertexCount = batchData.vecPos.size();
		}
		d3d::createVertexBuf(d3d, batch.vbPos, batchData.vecPos);
		d3d::createVertexBuf(d3d, batch.vbOther, batchData.vecOther);
		d3d::createVertexBuf(d3d, batch.vbTexIndices, batchData.texIndices);
		createTexArray(d3d, &batch.texColorArray, batchInfo, batchData.texIndexedIds);
		target.push_back(batch);
	}

	template <typename VERT_DATA>
	vector<pair<TexInfo, vector<pair<Material, const VERT_DATA *>>>> groupByTexId(D3d d3d, const unordered_map<Material, const VERT_DATA *>& meshData, TEX_INDEX maxTexturesPerBatch)
	{
		// load and bucket all materials so textures that are texture-array-compatible are grouped in a single bucket
		unordered_map<TexInfo, vector<Material>> texBuckets;
		for (const auto& [mat, data] : meshData) {
			bool srgb = mat.colorSpace == ColorSpace::SRGB;
			Texture* texture = getOrCreateTexture(d3d, mat.texBaseColor, srgb);
			auto& vec = ::util::getOrCreateDefault(texBuckets, texture->getInfo());
			vec.push_back(mat);
		}

		vector<pair<TexInfo, vector<pair<Material, const VERT_DATA *>>>> result;

		// create batches
		for (const auto& [texInfo, textures] : texBuckets) {

			pair<TexInfo, vector<pair<Material, const VERT_DATA *>>> batch = { texInfo, {} };
			TEX_INDEX currentIndex = 0;

			for (const auto mat : textures) {
				const VERT_DATA* currentMatData = meshData.find(mat)->second;
				batch.second.push_back(pair{ mat, currentMatData });

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

	template <VERTEX_FEATURE F>
	bool compareByChunkIndex(pair<ChunkIndex, vector<pair<Material, Verts<F>>>> const& lhs, pair<ChunkIndex, vector<pair<Material, Verts<F>>>> const& rhs) {
		const ChunkIndex& lhsIndex = lhs.first;
		const ChunkIndex& rhsIndex = rhs.first;
		if (lhsIndex.y == rhsIndex.y) {
			return lhsIndex.x < rhsIndex.x;
		}
		return lhsIndex.y < rhsIndex.y;
	}

	template <VERTEX_FEATURE F>
	vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>> groupAndSortByChunkIndex(const vector<pair<Material, const ChunkToVerts<F> *>>& batchData)
	{
		// use unordered_map to group
		unordered_map<ChunkIndex, vector<pair<Material, Verts<F>>>> chunkBuckets;

		for (const auto& [mat, chunkData] : batchData) {
			for (const auto& [chunkIndex, chunkVerts] : *chunkData) {

				auto& vec = ::util::getOrCreateDefault(chunkBuckets, chunkIndex);
				vec.push_back({ mat, chunkVerts });
			}
		}

		// convert unordered_map to vector for sorting
		vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>> result;
		result.reserve(chunkBuckets.size());

		for (const auto& [chunkIndex, chunkData] : chunkBuckets) {
			result.push_back({chunkIndex, chunkData});
		}

		// ideally we would maybe sort by morton code or something like that (implement "uint32_t getMortonIndex(ChunkIndex)" in ChunkGrid or similar)
		// for now we just sort by y then x which already reduces number of draw calls significantly (due to vert range merging of continuous active grid cells)
		std::sort(result.begin(), result.end(), &compareByChunkIndex<F>);

		return result;
	}

	template <VERTEX_FEATURE F>
	vector<pair<uint32_t, vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>>>> splitByVertCount(
		const vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>>& batchData, uint32_t maxVertCount)
	{
		vector<pair<uint32_t, vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>>>> result;

		vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>> currentBatch;
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

	template <VERTEX_FEATURE F>
	pair<VertsBatch<F>, LoadResult> flattenIntoBatch(const vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>>& batchData)
	{
		LoadResult result;
		result.states = 1;
		VertsBatch<F> target;

		uint32_t clusterCount = batchData.size();
		result.draws = clusterCount;
		target.vertClusters.reserve(clusterCount);

		unordered_map<Material, TEX_INDEX> materialIndices;
		vector<Material> materials;

		// reserve to avoid over-allocation (because the resulting vert vectors are going to be very big)
		bool useIndices = !batchData.empty() && batchData.at(0).second.at(0).second.useIndices;
		uint32_t indexCount = 0;
		uint32_t vertCount = 0;
		for (const auto& [chunkIndex, vertDataByMat] : batchData) {
			for (const auto& [material, vertData] : vertDataByMat) {
				indexCount += vertData.vecIndex.size();
				vertCount += vertData.vecPos.size();
			}
		}
		target.vecIndex.reserve(indexCount);
		target.vecPos.reserve(vertCount);
		target.vecOther.reserve(vertCount);
		target.texIndices.reserve(vertCount);

		result.verts = useIndices ? indexCount : vertCount;

		for (const auto& [chunkIndex, vertDataByMat] : batchData) {
			// set vertex data
			uint32_t currentVertIndex = useIndices ? target.vecIndex.size() : target.vecPos.size();
			target.vertClusters.push_back({ chunkIndex, currentVertIndex });

			for (const auto& [material, vertData] : vertDataByMat) {
				// rewrite indices
				uint32_t currentVertCount = target.vecPos.size();
				for (VertexIndex index : vertData.vecIndex) {
					target.vecIndex.push_back(currentVertCount + index);
				}

				// copy vertex data
				::util::insert(target.vecPos, vertData.vecPos);
				::util::insert(target.vecOther, vertData.vecOther);

				// set batch-dependent vertex data
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

		return { target, result };
	}

	template <typename VERT_DATA>
	array<unordered_map<Material, const VERT_DATA *>, BLEND_TYPE_COUNT> splitByPass(const std::unordered_map<Material, VERT_DATA>& meshData)
	{
		array<unordered_map<Material, const VERT_DATA *>, BLEND_TYPE_COUNT> result;
		for (auto& [material, vertData] : meshData) {
			uint8_t blendTypeIndex = (uint8_t)material.blendType;
			auto& passData = result.at(blendTypeIndex);
			passData.insert({ material, &vertData });
		}
		return result;
	}

	template <VERTEX_FEATURE F>
	bool compareByVertCount(MeshBatch<F> const& lhs, MeshBatch<F> const& rhs) {
		return lhs.vertexCount > rhs.vertexCount;
	}

	template <VERTEX_FEATURE F>
	void sortByVertCount(vector<MeshBatch<F>>& target)
	{
		std::sort(target.begin(), target.end(), &compareByVertCount<F>);
	}

	template <VERTEX_FEATURE F>
	LoadResult loadBatchVertexData(
		D3d d3d, MeshBatches<F>& targetAllPasses, const MatToChunksToVerts<F>& meshDataAllPasses, TEX_INDEX maxTexturesPerBatch)
	{
		LoadResult result;
		array<unordered_map<Material, const ChunkToVerts<F>* >, BLEND_TYPE_COUNT> perPassMeshData = splitByPass(meshDataAllPasses);

		for (uint16_t passIndex = 0; passIndex < BLEND_TYPE_COUNT; passIndex++) {
			const auto& meshData = perPassMeshData.at(passIndex);
			auto& target = targetAllPasses.passes.at(passIndex);

			vector<pair<TexInfo, vector<pair<Material, const ChunkToVerts<F>*>>>> batchedMeshData = groupByTexId(d3d, meshData, maxTexturesPerBatch);

			for (const auto& [texInfo, batchData] : batchedMeshData) {

				vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>> batchDataByChunk = groupAndSortByChunkIndex(batchData);

				// split current batch into multiple smaller batches along chunk boundaries if it contains too many verts to prevent OOM crashes
				vector<pair<uint32_t, vector<pair<ChunkIndex, vector<pair<Material, Verts<F>>>>>>> batchDataSplit =
					splitByVertCount(batchDataByChunk, vertCountPerBatch);

				batchDataByChunk.clear();// lots of memory that are no longer needed

				for (const auto& [vertCount, batchData] : batchDataSplit) {
					const auto [batchDataFlat, batchLoadResult] = flattenIntoBatch(batchData);

					//assert(vertCount == batchLoadResult.verts);
					result += batchLoadResult;
					loadRenderBatch(d3d, target, texInfo, batchDataFlat);
				}
			}

			sortByVertCount(target);// improves performance
		}

		return result;
	}

	void clearZenLevel()
	{
		world.meshBatchesWorld.release();
		world.meshBatchesObjects.release();
		for (auto& mesh : world.prepassMeshes) {
			mesh.release();
		}
		for (auto& tex : world.debugTextures) {
			delete tex;
		}
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

		LOG(INFO);
		LOG(INFO) << "    #########################################";
		LOG(INFO) << "    Loading data";
		LOG(INFO) << "    #########################################";

		assets::LoadDebugFlags debugFlags {};
		debugFlags.disableVertexIndices = false;

		bool levelDataFound = false;
		RenderData data;
		string level = ::util::asciiToLower(levelStr);

		if (::util::endsWith(level, ".zen")) {
			auto levelFileOpt = assets::getIfExists(level);
			if (levelFileOpt.has_value()) {
				auto levelFile = levelFileOpt.value();
				assets::loadZen(data, levelFile, debugFlags);
				levelDataFound = true;
			}
			else {
				LOG(WARNING) << "Failed to find level file '" << level << "'!";
			}
		}
		else {
			LOG(WARNING) << "Level file format not supported: " << level;
		}

		if (!levelDataFound) {
			return { .loaded = false };
		}

		sampler.logMillisAndRestart("Level: Loaded all data");

		LOG(INFO);
		LOG(INFO) << "    #########################################";
		LOG(INFO) << "    Uploading GPU data";
		LOG(INFO) << "    #########################################";

		clearZenLevel();
		world.isOutdoorLevel = data.isOutdoorLevel;
		{
			LOG(INFO) << "Level: Lightmap count: " << data.worldMeshLightmaps.size();
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
		//chunkgrid::updateSize(data.staticMeshesBlend);
		uint32_t cellCount = chunkgrid::finalizeSize();
		chunkgrid::updateMesh(data.worldMesh);
		chunkgrid::updateMesh(data.staticMeshes);
		//chunkgrid::updateMesh(data.staticMeshesBlend);
		LOG(INFO) << "Level: Computed Chunk Grid       - Cells: " << cellCount;
		sampler.logMillisAndRestart("Level: Computed chunk grid");

		LoadResult loadResult;
		// TODO fix prepass
		//loadResult = loadPrepassData(d3d, world.prepassMeshes, data.worldMesh);
		//LOG(INFO) << "Level: Uploaded depth prepass  - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		//sampler.logMillisAndRestart("Level: Uploaded depth prepass");

		loadResult = loadBatchVertexData(d3d, world.meshBatchesWorld, data.worldMesh, texturesPerBatch);
		LOG(INFO) << "Level: Uploaded world mesh       - States: " << loadResult.states << " Draws: " << loadResult.draws << " Verts: " << loadResult.verts;
		sampler.logMillisAndRestart("Level: Uploaded world mesh");

		loadResult = loadBatchVertexData(d3d, world.meshBatchesObjects, data.staticMeshes, texturesPerBatch);
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