#include "stdafx.h"
#include "PassWorld.h"

#include <filesystem>
#include <stdexcept>

#include "PassWorldLoader.h"
#include "PassWorldChunkGrid.h"
#include "PassSky.h"

#include "render/Camera.h"
#include "render/Sky.h"
#include "render/PerfStats.h"
#include "render/d3d/GeometryBuffer.h"

#include "Util.h"

// TODO move to RenderDebugGui
#include "../Gui.h"
#include <imgui.h>
#include "imgui/imgui_custom.h"

namespace render::pass::world
{
	using namespace DirectX;
	using ::std::unordered_map;
	using ::std::vector;
	using ::std::array;

	extern World world;

	WorldSettings worldSettings;

	ID3D11SamplerState* linearSamplerState = nullptr;

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

		render::gui::addSettings("World", {
			[&]() -> void {
				ImGui::PushItemWidth(gui::constants().elementWidth);
				// Lables starting with ## are hidden
				float timeOfDay = worldSettings.timeOfDay;
				bool changed = ImGui::DragFloat("##TimeOfDay", &timeOfDay, .002f, 0, 0, "%.3f TimeOfDay");
				if (changed) {
					updateTimeOfDay(timeOfDay);
				}
				ImGui::SliderFloat("##TimeOfDayChangeSpeed", &worldSettings.timeOfDayChangeSpeed, 0, 1, "%.3f Time Speed", ImGuiSliderFlags_Logarithmic);
				ImGui::PopItemWidth();
				ImGui::VerticalSpacing();

				ImGui::PushStyleColorDebugText();
				ImGui::Checkbox("Draw World", &worldSettings.drawWorld);
				ImGui::Checkbox("Draw VOBs/MOBs", &worldSettings.drawStaticObjects);
				ImGui::Checkbox("Draw Sky", &worldSettings.drawSky);
				ImGui::VerticalSpacing();
				ImGui::Checkbox("Chunked Rendering", &worldSettings.chunkedRendering);
				ImGui::VerticalSpacing();
				ImGui::Checkbox("LOD Low Only", &worldSettings.lowLodOnly);
				ImGui::BeginDisabled(!worldSettings.chunkedRendering);
				ImGui::Checkbox("LOD Enabled", &worldSettings.enableLod);
				ImGui::SliderFloat("##LodCrossover", &worldSettings.lodRadius, 0, 3000, "%.0f LOD Crossover");
				ImGui::VerticalSpacing();
				ImGui::Checkbox("Enable Frustum Culling", &worldSettings.enableFrustumCulling);
				ImGui::Checkbox("Update Frustum Culling", &worldSettings.updateFrustumCulling);
				ImGui::EndDisabled();
				ImGui::VerticalSpacing();
				ImGui::Checkbox("Debug Shader", &worldSettings.debugWorldShaderEnabled);
				ImGui::Checkbox("Debug Single Draw", &worldSettings.debugSingleDrawEnabled);
				ImGui::BeginDisabled(!worldSettings.debugSingleDrawEnabled);
				ImGui::SliderInt("Debug Single Draw Index", &worldSettings.debugSingleDrawIndex, 0, maxDrawCalls - 1);
				ImGui::EndDisabled();
				ImGui::PopStyleColor();
			}
		});

		render::gui::addSettings("World Draw Filter", {
			[&]() -> void {
				ImGui::BeginDisabled(!worldSettings.chunkedRendering);
				ImGui::PushItemWidth(gui::constants().elementWidth);
				ImGui::PushStyleColorDebugText();

				ImGui::Checkbox("Filter Chunks X", &worldSettings.chunkFilterXEnabled);
				ImGui::Checkbox("Filter Chunks Y", &worldSettings.chunkFilterYEnabled);

				auto [min, max] = chunkgrid::getIndexMinMax();

				ImGui::BeginDisabled(!worldSettings.chunkFilterXEnabled);
				ImGui::SliderScalar("ChunkIndex X", ImGui::dataType(min.x), &worldSettings.chunkFilterX, &min.x, &max.x);
				ImGui::EndDisabled();
				ImGui::BeginDisabled(!worldSettings.chunkFilterYEnabled);
				ImGui::SliderScalar("ChunkIndex Y", ImGui::dataType(min.y), &worldSettings.chunkFilterY, &min.y, &max.y);
				ImGui::EndDisabled();

				ImGui::PopStyleColor();
				ImGui::PopItemWidth();
				ImGui::EndDisabled();
			}
			});

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

	void updateCameraFrustum(const BoundingFrustum& cameraFrustum, bool hasCameraMoved)
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

	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		if (world.isOutdoorLevel && worldSettings.drawSky) {
			const auto layers = getSkyLayers(worldSettings.timeOfDay);
			bool swapLayers = getSwapLayers(worldSettings.timeOfDay);
			sky::updateSkyLayers(d3d, layers, getSkyColor(worldSettings.timeOfDay), worldSettings.timeOfDay, swapLayers);
			sky::drawSky(d3d, shaders, cbs, linearSamplerState);
		}
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

	DrawStats drawMeshClusters(D3d d3d, const vector<ChunkVertCluster>& vertClusters, uint32_t drawCount, bool indexed, bool isLowLod, bool ignoreLodRadius)
	{
		// TODO select closest chunks (any that overlap a "min render box" around camera) and render them first for early-z discards
		// TODO fix view-coordinate normal lighting and move to pixel shader so lighting can profit from early-z
		DrawStats stats;

		uint32_t drawOffset = vertClusters.at(0).vertStartIndex;

		// TODO cutoff should be adjustable in GUI
		uint32_t ignoreAllChunksVertThreshold = 1000;

		if (!worldSettings.chunkedRendering || drawCount <= ignoreAllChunksVertThreshold) {
			if (isLowLod && !ignoreLodRadius) return stats;
			// for small number of verts per batch we ignore the clusters to save draw calls
			stats += draw(d3d, drawCount, drawOffset, indexed);
		}
		else {
			// TODO LOD Rendering:
			// enableLod - enable reduced LOD drawing
			// lodRadius - camera-to-chunkcenter distance where full LOD crosses over to reduced LOD
			// enablePerVertexLod - instead of just rendering each chunk with either LOD (resulting in per-chunk LOD switches),
			//     render all chunks that touch lodRadiusWidth with both LODs and let shader select or dither LOD
			// enableLodDither - requires enablePerVertexLod, select dither pattern instead of just switching LOD at radius
			// lodRadiusWidth - requires enableLodDither, splits lodRadius into lodRadiusStart and lodRadiusEnd, dithering is interpolated over this range
			// reducedLodOnly - debug option to only render all chunks with reduced LOD, ignored all other options
			// 
			// Split draws into per-LOD draws since we need to potentially bind constant buffers to tell shader which LOD it is drawing
			// so it can select dither pattern, also should make code more straightforward

			float lodRadiusSq = std::pow(worldSettings.lodRadius, 2);

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
				bool isInsideLodRadius = ignoreLodRadius || (isLowLod == camera.distanceToCenterSq > lodRadiusSq);
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

			if (!indexed || (!worldSettings.lowLodOnly || !hasLod)) {
				d3d.annotation->BeginEvent(L"LOD High");
				// TODO set constant buffer for LOD if shader LOD is enabled
				bool ignoreLodRadius = !indexed || !hasLod || !worldSettings.enableLod;
				stats += drawMeshClusters(d3d, mesh.vertClusters, mesh.drawCount, indexed, false, ignoreLodRadius);
				d3d.annotation->EndEvent();
			}
			if (indexed && (worldSettings.lowLodOnly || (hasLod && worldSettings.enableLod))) {
				d3d.annotation->BeginEvent(L"LOD Low");
				// TODO set constant buffer for LOD if shader LOD is enabled
				//stats.stateChanges++;
				stats += drawMeshClusters(d3d, mesh.vertClustersLod, mesh.drawLodCount, indexed, true, worldSettings.lowLodOnly);
				d3d.annotation->EndEvent();
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

	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, BlendType pass)
	{
		Shader* shader = shaders->getShader("forward/wireframe");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		drawVertexBuffersWorld(d3d, false, pass, batchGetVbPos<VertexBasic>, batchGetVbPos<VertexBasic>);
	}

	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, BlendType pass)
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
	}
}