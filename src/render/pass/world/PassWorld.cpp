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

	ID3D11SamplerState* linearSamplerState = nullptr;


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
					if (selectedDebugTexture >= 0 && selectedDebugTexture < (int32_t) world.debugTextures.size()) {
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

	void updatePrepareDraws(D3d d3d, const BoundingFrustum& cameraFrustum, bool hasCameraMoved)
	{
		if (worldSettings.chunkedRendering) {
			bool updateCulling = worldSettings.enableFrustumCulling && worldSettings.updateFrustumCulling && hasCameraMoved;
			bool updateDistances = hasCameraMoved;
			chunkgrid::updateCamera(camera::getFrustum(), updateCulling, updateDistances);
		}
	}

	WorldSettings& getWorldSettings() {
		return worldSettings;
	}

	void initLinearSampler(D3d d3d, RenderSettings& settings)
	{
		// TODO use Other.h
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
			sky::drawSky(d3d, shaders, linearSamplerState);
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
	using GetVertexBuffers = vector<VertexBuffer> (*) (const MeshBatch<F>&);

	template <VERTEX_FEATURE F> vector<VertexBuffer> batchGetVbPos   (const MeshBatch<F>& mesh) { return { mesh.vbPos }; }
	template <VERTEX_FEATURE F> vector<VertexBuffer> batchGetVbPosTex(const MeshBatch<F>& mesh) { return { mesh.vbPos, mesh.vbTexIndices }; }
	template <VERTEX_FEATURE F> vector<VertexBuffer> batchGetVbAll   (const MeshBatch<F>& mesh) { return { mesh.vbPos, mesh.vbOther, mesh.vbTexIndices }; }

	DrawStats draw(D3d d3d, uint32_t count, uint32_t start, bool indexed)
	{
		DrawStats stats; 
		if (!worldSettings.debugSingleDrawEnabled || worldSettings.debugSingleDrawIndex == currentDrawCall) {
			if (indexed) {
				d3d.deviceContext->DrawIndexed(count, start, 0);
			}
			else {
				d3d.deviceContext->Draw(count, start);
			}

			stats.verts += count;
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

		return draw(d3d, drawCount, vertClusters.at(0).vertStartIndex, indexed);
	}

	DrawStats drawMeshClusters(D3d d3d, const vector<ChunkVertCluster>& vertClusters, uint32_t drawCount, bool indexed, bool isLodNear, bool ignoreLodRadius)
	{
		// TODO select closest chunks (any that overlap a "min render box" around camera) and render them first for early-z discards
		// TODO fix view-coordinate normal lighting and move to pixel shader so lighting can profit from early-z
		DrawStats stats;

		uint32_t drawOffset = vertClusters.at(0).vertStartIndex;

		auto [lodRadiusBegin, lodRadiusEnd] = updateLodRangeCb(d3d, ignoreLodRadius, isLodNear);
		d3d.deviceContext->PSSetConstantBuffers(CbLodRange::slot(), 1, &lodRangeCb.buffer);

		float lodRadiusSq = std::pow(worldSettings.lodRadius, 2);
		float lodRadiusBeginSq = std::pow(lodRadiusBegin, 2);
		float lodRadiusEndSq = std::pow(lodRadiusEnd, 2);

		uint32_t ignoreChunkVertThreshold = 500;// TODO

		// TODO since max number of cells is known, we could probably re-use a single static vector for all meshes
		vector<std::pair<uint32_t, uint32_t>> vertRanges;
		vertRanges.reserve(vertClusters.size());

		bool rangeActive = false;
		uint32_t rangeStart = drawOffset;

		// TODO chunks with very few verts should be allowed to be enabled if range is active currently, if that helps joining ranges (test!)

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

			if (!rangeActive && currentChunkActive) {
				// range start
				rangeStart = vertStartIndex;
				rangeActive = true;
			}
			if (rangeActive && !currentChunkActive) {
				// range end
				rangeActive = false;
				vertRanges.push_back({ rangeStart, vertStartIndex });
			}
		}
		// end last range
		if (rangeActive) {
			rangeActive = false;
			vertRanges.push_back({ rangeStart, drawOffset + drawCount });
		}

		for (const auto [startIncl, endExcl] : vertRanges) {
			uint32_t vertCount = endExcl - startIncl;

			vertCount = (uint32_t)(vertCount * worldSettings.debugDrawVertAmount);
			vertCount -= (vertCount % 3);

			stats += draw(d3d, vertCount, startIncl, indexed);
		}

		return stats;
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

				if (worldSettings.lodDisplayMode != LodMode::FAR) {
					d3d.annotation->BeginEvent(L"LOD High");
					stats += drawMeshClusters(d3d, mesh.vertClusters, mesh.drawCount, indexed, true, !drawLod);
					d3d.annotation->EndEvent();
				}
				if (drawLod && worldSettings.lodDisplayMode != LodMode::NEAR) {
					if (worldSettings.lodDisplayMode == LodMode::FULL) {
						// drawMeshClusters sets a constant buffer -> state change between near and far LOD drawing
						stats.stateChanges++; 
					}
					d3d.annotation->BeginEvent(L"LOD Low");
					stats += drawMeshClusters(d3d, mesh.vertClustersLod, mesh.drawLodCount, indexed, false, false);
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

		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

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
		release(linearSamplerState);
		lodRangeCb.release();
	}
}