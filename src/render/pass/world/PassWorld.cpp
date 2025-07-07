#include "stdafx.h"
#include "PassWorld.h"

#include <filesystem>
#include <stdexcept>

#include "render/pass/world/WorldSettingsGui.h"
#include "render/pass/world/WorldLoader.h"
#include "render/pass/world/WorldGrid.h"

#include "render/Camera.h"
#include "render/Sky.h"
#include "render/PerfStats.h"
#include "render/d3d/ConstantBuffer.h"
#include "render/d3d/GeometryBuffer.h"
#include "render/d3d/Sampler.h"
#include "render/pass/PassSky.h"

#include "Util.h"

#include "render/Gui.h"

#include "imgui/imgui_custom.h"
#include <imgui.h>

namespace render::pass::world
{
	using namespace DirectX;
	using ::std::unordered_map;
	using ::std::vector;
	using ::std::array;

	extern World world;

	WorldSettings worldSettings;

	ID3D11SamplerState* samplerState = nullptr;


	enum CbLodRangeType {
		NONE,
		MAX, // maxRange, clip/fade farther pixels
		MIN, // minRange, clip/fade nearer pixels
	};

	__declspec(align(16)) struct CbLodRange {
		static uint16_t slot() {
			return 5;
		}

		uint32_t rangeType;
		int32_t ditherEnabled;
		float rangeBegin;
		float rangeEnd;
	};

	d3d::ConstantBuffer<CbLodRange> lodRangeCb = {};

	struct DrawRange {
		uint32_t start;
		uint32_t count;
	};

	struct MergedDrawsBuilder {
	private:
		bool currentRangeActive = false;
		uint32_t currentRangeStart = 0;

	public:
		std::vector<DrawRange> draws;

		void reinit(uint32_t maxDraws)
		{
			assert(!currentRangeActive);
			currentRangeStart = 0;
			draws.clear();
			draws.reserve(maxDraws);
		}

		void finalizeRange(uint32_t drawEndExlusive)
		{
			assert(drawEndExlusive >= currentRangeStart);
			if (currentRangeActive) {
				// range end
				currentRangeActive = false;
				uint32_t drawCount = drawEndExlusive - currentRangeStart;
				if (drawCount > 0) {
					draws.push_back({ currentRangeStart, drawCount });
				}
			}
		}

		void createOrFinalizeRange(bool isActive, uint32_t drawStart)
		{
			// TODO chunks with very few verts should be allowed to be enabled if range is active currently, if that helps joining ranges (test!)
			uint32_t ignoreChunkVertThreshold = 500;// TODO

			assert(drawStart >= currentRangeStart);
			if (!currentRangeActive && isActive) {
				// range start
				currentRangeStart = drawStart;
				currentRangeActive = true;
			}
			if (!isActive) {
				finalizeRange(drawStart);
			}
		}
	};

	float minLodStart = 0.00001f;
	float minLodWidth = 0.001f;// since this is added to potentially bigger number, we have less precision

	int32_t selectedDebugTexture = 0;

	uint32_t currentDrawCall = 0;
	uint32_t maxDrawCalls = 0;

	struct DrawStats {
		uint32_t stateChanges = 0;
		uint32_t draws = 0;
		uint32_t verts = 0;

		auto operator+=(const DrawStats& rhs)
		{
			stateChanges += rhs.stateChanges;
			draws += rhs.draws;
			verts += rhs.verts;
		};
	};

	struct DrawStatSamplers {
		stats::SamplerId stateChanges;
		stats::SamplerId draws;
		stats::SamplerId verts;
	} samplers;

	void updateTimeOfDay(float timeOfDay) {
		// clamp, wrap around if negative
		float __;
		worldSettings.timeOfDay = std::modf(std::modf(timeOfDay, &__) + 1, &__);
	}

	void initGui()
	{
		samplers = {
			stats::createSampler(), stats::createSampler(), stats::createSampler()
		};

		render::gui::addInfo("World", {
			[&]() -> void {
				float hourTime = (worldSettings.timeOfDay * 24);
				uint8_t hour = hourTime + 12;
				if (hour >= 24) {
					hour -= 24;
				}
				uint8_t minute = std::fmod(hourTime * 60, 60);

				uint32_t stateChanges = render::stats::getSamplerStats(samplers.stateChanges).average;
				uint32_t draws = render::stats::getSamplerStats(samplers.draws).average;
				uint32_t verts = render::stats::getSamplerStats(samplers.verts).average / 1000;

				std::stringstream buffer;
				buffer << "TimeOfDay: " << std::format("{:02}:{:02}", hour, minute) << '\n';
				buffer << "States/Draws: " << stateChanges << " / " << draws << '\n';
				buffer << "Verts: " << verts << "k" << '\n';
				ImGui::Text(buffer.str().c_str());
			}
			});

		gui::init(
			worldSettings,
			maxDrawCalls,
			[&]() -> void { updateTimeOfDay(worldSettings.timeOfDay); },
			[&]() -> auto { return chunkgrid::getIndexMinMax(); });

		// TODO move to RenderDebugGui
		render::gui::addWindow("Lightmaps", {
			[&]() -> void {
				ImGui::PushItemWidth(60);
				ImGui::InputInt("Lightmap Index", &selectedDebugTexture, 0);
				ImGui::PopItemWidth();

				float zoom = 2.0f;
				if (world.debugTextures.size() >= 1) {
					if (selectedDebugTexture >= 0 && selectedDebugTexture < (int32_t)world.debugTextures.size()) {
						ImGui::Image((ImTextureID)world.debugTextures.at(selectedDebugTexture)->GetResourceView(), { 256 * zoom, 256 * zoom });
					}
				}
			}
			});
	}

	LoadWorldResult loadWorld(D3d d3d, const std::string& level) {
		return loadZenLevel(d3d, level);
	}

	void updateObjects(float deltaTime)
	{
		updateTimeOfDay(worldSettings.timeOfDay + (worldSettings.timeOfDayChangeSpeed * deltaTime));
	}

	void updatePrepareDraws(D3d d3d, const BoundingFrustum& cameraFrustum, bool hasCameraChanged)
	{
		if (worldSettings.chunkedRendering) {
			bool updateCulling = worldSettings.enableFrustumCulling && worldSettings.updateFrustumCulling && hasCameraChanged;

			static float previousCloseRadius = 0;
			bool closeRadiusChanged = previousCloseRadius != worldSettings.renderCloseRadius;
			previousCloseRadius = worldSettings.renderCloseRadius;
			bool updateCloseIntersect = (worldSettings.renderCloseFirst && hasCameraChanged) || closeRadiusChanged;

			bool updateDistances = hasCameraChanged;

			chunkgrid::updateCamera(camera::getFrustum(), updateCulling, updateCloseIntersect, worldSettings.renderCloseRadius, updateDistances);
		}
	}

	WorldSettings& getWorldSettings() {
		return worldSettings;
	}

	void notifyGameSwitch(RenderSettings& settings)
	{
		setSkyGothicVersion(settings.isG2);
	}

	void initLinearSampler(D3d d3d, RenderSettings& settings)
	{
		d3d::createSampler(d3d, &samplerState, settings.anisotropicLevel);
	}

	void init(D3d d3d)
	{
		initGui();

		// select which primtive type we are using, TODO this should be managed more centrally, because otherwise changing this affects other parts of pipeline
		d3d.deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//d3d::createConstantBuf(d3d, worldSettingsCb, BufferUsage::WRITE_GPU);
		d3d::createConstantBuf(d3d, lodRangeCb, BufferUsage::WRITE_GPU);

		sky::initConstantBuffers(d3d);
	}

	Color getBackgroundColor() {
		if (world.isOutdoorLevel) {
			return getSkyColor(worldSettings.timeOfDay);
		}
		else {
			return Color(0.0f, 0.2f, 0.4f, 1.0f);// deep blue
		}
	}

	void drawSky(D3d d3d, ShaderManager* shaders)
	{
		if (world.isOutdoorLevel && worldSettings.drawSky) {
			const auto layers = getSkyLayers(worldSettings.timeOfDay);
			bool swapLayers = getSwapLayers(worldSettings.timeOfDay);
			sky::updateSkyLayers(d3d, layers, getSkyColor(worldSettings.timeOfDay), worldSettings.timeOfDay, swapLayers);
			sky::drawSky(d3d, shaders, samplerState);
		}
	}

	std::pair<float, float> updateLodRangeCb(D3d d3d, bool ignoreLodRadius, bool isLodNear)
	{
		float radius = worldSettings.lodRadius;

		CbLodRange cbLodRange;
		cbLodRange.rangeType = (uint32_t)CbLodRangeType::NONE;
		cbLodRange.ditherEnabled = false;
		cbLodRange.rangeBegin = radius;
		cbLodRange.rangeEnd = radius;

		if (worldSettings.enablePerPixelLod) {
			if (!ignoreLodRadius) {
				cbLodRange.rangeType = (uint32_t)(isLodNear ? CbLodRangeType::MAX : CbLodRangeType::MIN);
			}
			if (worldSettings.enableLodDithering) {
				cbLodRange.ditherEnabled = true;

				float radiusHalfWidth = worldSettings.lodRadiusDitherWidth * 0.5f;
				cbLodRange.rangeBegin = std::max(0.f, radius - radiusHalfWidth);
				cbLodRange.rangeEnd = std::max(cbLodRange.rangeBegin + minLodWidth, radius + radiusHalfWidth);
			}
		}
		d3d::updateConstantBuf(d3d, lodRangeCb, cbLodRange);
		return { cbLodRange.rangeBegin, cbLodRange.rangeEnd };
	}

	template <VERTEX_FEATURE F>
	using GetVertexBuffers = vector<VertexBuffer>(*) (const MeshBatch<F>&);

	template <VERTEX_FEATURE F> vector<VertexBuffer> batchGetVbPos(const MeshBatch<F>& mesh) { return { mesh.vbPos }; }
	template <VERTEX_FEATURE F> vector<VertexBuffer> batchGetVbPosTex(const MeshBatch<F>& mesh) { return { mesh.vbPos, mesh.vbTexIndices }; }
	template <VERTEX_FEATURE F> vector<VertexBuffer> batchGetVbAll(const MeshBatch<F>& mesh) { return { mesh.vbPos, mesh.vbOther, mesh.vbTexIndices }; }

	
	DrawStats draw(D3d d3d, DrawRange range, bool indexed)
	{
		DrawStats stats;
		if (!worldSettings.debugSingleDrawEnabled || worldSettings.debugSingleDrawIndex == currentDrawCall) {
			if (indexed) {
				d3d.deviceContext->DrawIndexed(range.count, range.start, 0);
			}
			else {
				d3d.deviceContext->Draw(range.count, range.start);
			}

			stats.verts += range.count;
			stats.draws++;
		}

		currentDrawCall++;
		return stats;
	}

	DrawStats drawAllMeshClusters(D3d d3d, const vector<ChunkVertCluster>& vertClusters, uint32_t drawCount, bool indexed)
	{
		CbLodRange cbLodRange;
		cbLodRange.rangeType = CbLodRangeType::NONE;

		d3d::updateConstantBuf(d3d, lodRangeCb, cbLodRange);
		d3d.deviceContext->PSSetConstantBuffers(CbLodRange::slot(), 1, &lodRangeCb.buffer);

		return draw(d3d, { vertClusters.at(0).vertStartIndex, drawCount }, indexed);
	}

	DrawStats draw(D3d d3d, const std::vector<DrawRange>& drawList, bool indexed)
	{
		DrawStats stats;

		for (const DrawRange& drawRange : drawList) {
			stats += draw(d3d, drawRange, indexed);
		}

		return stats;
	}

	void createMeshClusterDraws(
		D3d d3d,
		MergedDrawsBuilder& drawsBuilder,
		MergedDrawsBuilder& drawsBuilderClose,
		const vector<ChunkVertCluster>& vertClusters,
		uint32_t drawCount,
		bool isLodNear,
		bool ignoreLodRadius)
	{
		// TODO fix view-coordinate normal lighting and move to pixel shader so lighting can profit from early-z

		uint32_t drawOffset = vertClusters.at(0).vertStartIndex;

		auto [lodRadiusBegin, lodRadiusEnd] = updateLodRangeCb(d3d, ignoreLodRadius, isLodNear);
		d3d.deviceContext->PSSetConstantBuffers(CbLodRange::slot(), 1, &lodRangeCb.buffer);

		float lodRadiusSq = std::pow(worldSettings.lodRadius, 2);
		float lodRadiusBeginSq = std::pow(lodRadiusBegin, 2);
		float lodRadiusEndSq = std::pow(lodRadiusEnd, 2);

		float closeRadiusSq = std::pow(worldSettings.renderCloseRadius, 2);

		drawsBuilder.reinit(vertClusters.size());
		drawsBuilderClose.reinit(0);

		for (uint32_t i = 0; i < vertClusters.size(); i++) {
			const GridPos& gridPos = vertClusters[i].gridPos;
			uint32_t vertStartIndex = vertClusters[i].vertStartIndex;

			auto gridIndex = chunkgrid::getIndex(gridPos);
			auto camera = chunkgrid::getCameraInfoInner(gridIndex.innerIndex);

			bool isInsideLodRadius = false;
			if (worldSettings.enablePerPixelLod) {
				auto cell = chunkgrid::getCellInfoInner(gridIndex.innerIndex);
				float cellHalfWidthSq = cell.bbox.Extents.x * cell.bbox.Extents.x;

				isInsideLodRadius = ignoreLodRadius || (isLodNear ?
					(camera.distanceCornerNearSq < lodRadiusEndSq || camera.distanceCenter2dSq <= cellHalfWidthSq)
					: camera.distanceCornerFarSq > lodRadiusBeginSq);
			} else {
				isInsideLodRadius = ignoreLodRadius || (isLodNear == camera.distanceCenter2dSq <= lodRadiusSq);
			}

			bool isInsideFrustum = !worldSettings.enableFrustumCulling || camera.intersectsFrustum;
			bool currentChunkActive = isInsideFrustum && isInsideLodRadius;

			if (worldSettings.chunkFilterXEnabled) {
				currentChunkActive = currentChunkActive && gridPos.x == worldSettings.chunkFilterX;
			}
			if (worldSettings.chunkFilterYEnabled) {
				currentChunkActive = currentChunkActive && gridPos.y == worldSettings.chunkFilterY;
			}

			bool isClose = worldSettings.renderCloseFirst && camera.intersectsFrustumClose;

			drawsBuilder.createOrFinalizeRange(currentChunkActive && !isClose, vertStartIndex);
			drawsBuilderClose.createOrFinalizeRange(currentChunkActive && isClose, vertStartIndex);
		}
		// end last range
		drawsBuilder.finalizeRange(drawOffset + drawCount);
		drawsBuilderClose.finalizeRange(drawOffset + drawCount);
	}

	template<VERTEX_FEATURE F>
	DrawStats drawMeshBatches(D3d d3d, const vector<MeshBatch<F>>& meshes, bool bindTexColor, bool hasLod, GetVertexBuffers<F> getVertexBuffers)
	{
		DrawStats stats;
		for (auto& mesh : meshes) {
			bool indexed = mesh.useIndices;
			if (indexed) {
				d3d::setIndexBuffer(d3d, mesh.vbIndices);
			}

			d3d::setVertexBuffers(d3d, getVertexBuffers(mesh));
			if (bindTexColor) {
				d3d.deviceContext->PSSetShaderResources(0, 1, &mesh.texColorArray);
			}

			stats.stateChanges++;

			uint32_t ignoreAllChunksVertThreshold = 1000; // TODO cutoff should be adjustable in GUI
			bool drawAllChunks = !worldSettings.chunkedRendering || mesh.drawCount <= ignoreAllChunksVertThreshold;

			if (drawAllChunks) {
				// for small number of verts per batch we ignore the clusters to save draw calls / LOD overhead
				if (worldSettings.lodDisplayMode != LodMode::FAR) {
					stats += drawAllMeshClusters(d3d, mesh.vertClusters, mesh.drawCount, indexed);
				}
			}
			else {
				bool drawLod = indexed && hasLod && worldSettings.enableLod;

				// use two so we can render close geometry before far geometry for better early-z, re-used to prevent allocations
				static MergedDrawsBuilder builder;
				static MergedDrawsBuilder builderClose;

				if (worldSettings.lodDisplayMode != LodMode::FAR) {
					d3d.annotation->BeginEvent(L"LOD High");
					createMeshClusterDraws(d3d, builder, builderClose, mesh.vertClusters, mesh.drawCount, true, !drawLod);
					stats += draw(d3d, builderClose.draws, indexed);
					stats += draw(d3d, builder.draws, indexed);
					d3d.annotation->EndEvent();
				}
				if (drawLod && worldSettings.lodDisplayMode != LodMode::NEAR) {
					if (worldSettings.lodDisplayMode == LodMode::FULL) {
						// drawMeshClusters sets a constant buffer -> state change between near and far LOD drawing
						stats.stateChanges++; 
					}
					d3d.annotation->BeginEvent(L"LOD Low");
					createMeshClusterDraws(d3d, builder, builderClose, mesh.vertClustersLod, mesh.drawLodCount, false, false);
					stats += draw(d3d, builderClose.draws, indexed);
					stats += draw(d3d, builder.draws, indexed);
					d3d.annotation->EndEvent();
				}
			}
		}
		return stats;
	}

	DrawStats drawVertexBuffersWorld(D3d d3d, bool bindTexColor, BlendType pass, GetVertexBuffers<VertexBasic> getVbsWorld, GetVertexBuffers<VertexBasic> getVbsObject)
	{
		DrawStats stats;
		if (worldSettings.drawWorld) {
			d3d.annotation->BeginEvent(L"Worldmesh");
			auto& batches = world.meshBatchesWorld.getBatches(pass);
			stats += drawMeshBatches(d3d, batches, bindTexColor, false, getVbsWorld);
			d3d.annotation->EndEvent();
		}
		if (worldSettings.drawStaticObjects) {
			d3d.annotation->BeginEvent(L"Objects");
			auto& batches = world.meshBatchesObjects.getBatches(pass);
			stats += drawMeshBatches(d3d, batches, bindTexColor, true, getVbsObject);
			d3d.annotation->EndEvent();
		}
		return stats;
	};

	void drawWireframe(D3d d3d, ShaderManager* shaders, BlendType pass)
	{
		Shader* shader = shaders->getShader("forward/wireframe");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		drawVertexBuffersWorld(d3d, false, pass, batchGetVbPos<VertexBasic>, batchGetVbPos<VertexBasic>);
	}

	void drawWorld(D3d d3d, ShaderManager* shaders, BlendType pass)
	{
		d3d.annotation->BeginEvent(L"Main");

		// set the shader objects avtive
		Shader* shader;
		if (pass == BlendType::MULTIPLY || worldSettings.debugWorldShaderEnabled) {
			shader = shaders->getShader("forward/mainTexColorOnly");
		}
		else {
			shader = shaders->getShader("forward/main");
		}
		
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		d3d.deviceContext->PSSetSamplers(0, 1, &samplerState);

		// lightmaps
		d3d.deviceContext->PSSetShaderResources(1, 1, &world.lightmapTexArray);

		maxDrawCalls = currentDrawCall;
		currentDrawCall = 0;

		DrawStats stats = drawVertexBuffersWorld(d3d, true, pass, batchGetVbAll<VertexBasic>, batchGetVbAll<VertexBasic>);

		stats::takeSample(samplers.stateChanges, stats.stateChanges);
		stats::takeSample(samplers.draws, stats.draws);
		stats::takeSample(samplers.verts, stats.verts);

		d3d.annotation->EndEvent();
	}

	void clean()
	{
		sky::clean();
		clearZenLevel();
		release(samplerState);
		lodRangeCb.release();
	}
}