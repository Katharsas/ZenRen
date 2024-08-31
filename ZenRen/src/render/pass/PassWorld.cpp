#include "stdafx.h"
#include "PassWorld.h"

#include <filesystem>
#include <stdexcept>

#include "PassWorldLoader.h"
#include "PassWorldChunkGrid.h"
#include "PassSky.h"
#include "../Camera.h"
#include "../Sky.h"
#include "../RenderUtil.h"
#include "../PerfStats.h"
#include "Util.h"

// TODO move to RenderDebugGui
#include "../Gui.h"
#include "imgui/imgui.h"
#include "imgui/imgui_custom.h"

namespace render::pass::world
{
	using namespace DirectX;
	using ::std::unordered_map;
	using ::std::vector;
	using ::std::array;

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

		render::addInfo("World", {
			[]() -> void {
				uint32_t stateChanges = render::stats::getSamplerStats(samplers.stateChanges).average;
				uint32_t draws = render::stats::getSamplerStats(samplers.draws).average;
				uint32_t verts = render::stats::getSamplerStats(samplers.verts).average / 1000;

				std::stringstream buffer;
				buffer << "States/Draws: " << stateChanges << " / " << draws << std::endl;
				buffer << "Verts: " << verts << "k" << std::endl;
				ImGui::Text(buffer.str().c_str());
			}
		});

		addSettings("World", {
			[&]() -> void {
				ImGui::PushItemWidth(GUI_ELEMENT_WIDTH);
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
				ImGui::Checkbox("Enable Frustum Culling", &worldSettings.enableFrustumCulling);
				ImGui::Checkbox("Update Frustum Culling", &worldSettings.updateFrustumCulling);
				ImGui::VerticalSpacing();
				ImGui::Checkbox("Debug Shader", &worldSettings.debugWorldShaderEnabled);
				ImGui::Checkbox("Debug Single Draw", &worldSettings.debugSingleDrawEnabled);
				ImGui::BeginDisabled(!worldSettings.debugSingleDrawEnabled);
				ImGui::SliderInt("Debug Single Draw Index", &worldSettings.debugSingleDrawIndex, 0, maxDrawCalls - 1);
				ImGui::EndDisabled();
				ImGui::PopStyleColor();
			}
		});

		// TODO move to RenderDebugGui
		addWindow("Lightmaps", {
			[&]() -> void {
				ImGui::PushItemWidth(60);
				ImGui::InputInt("Lightmap Index", &selectedDebugTexture, 0);
				ImGui::PopItemWidth();

				float zoom = 2.0f;
				if (world.debugTextures.size() >= 1) {
					if (selectedDebugTexture >= 0 && selectedDebugTexture < (int32_t) world.debugTextures.size()) {
						ImGui::Image(world.debugTextures.at(selectedDebugTexture)->GetResourceView(), { 256 * zoom, 256 * zoom });
					}
				}
			}
		});
	}

	void loadWorld(D3d d3d, const std::string& level) {
		loadLevel(d3d, level);
	}

	void updateObjects(float deltaTime)
	{
		updateTimeOfDay(worldSettings.timeOfDay + (worldSettings.timeOfDayChangeSpeed * deltaTime));
	}

	void updateCameraFrustum(const BoundingFrustum& cameraFrustum)
	{
		if (worldSettings.enableFrustumCulling && worldSettings.updateFrustumCulling) {
			chunkgrid::updateCamera(camera::getFrustum());
		}
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
		if (world.isOutdoorLevel) {
			return getSkyColor(worldSettings.timeOfDay);
		}
		else {
			return D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f);// deep blue
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

	// Let the template madness begin!
	// Note: What should really happen here is that i accept that it is ok to have dynamic number of VertexBuffers per mesh at runtime.
	
	template <typename Mesh>
	using GetTexSrv = ID3D11ShaderResourceView* (*) (const Mesh&);

	template <typename Mesh, int VbCount>
	using GetVertexBuffers = array<VertexBuffer, VbCount> (*) (const Mesh&);

	ID3D11ShaderResourceView* batchGetTex(const MeshBatch& mesh) { return mesh.texColorArray; }

	array<VertexBuffer, 1> prepassGetVbPos(const PrepassMeshes& mesh) { return { mesh.vbPos }; }
	array<VertexBuffer, 1> batchGetVbPos   (const MeshBatch& mesh) { return { mesh.vbPos }; }
	array<VertexBuffer, 2> batchGetVbPosTex(const MeshBatch& mesh) { return { mesh.vbPos, mesh.vbTexIndices }; }
	array<VertexBuffer, 3> batchGetVbAll   (const MeshBatch& mesh) { return { mesh.vbPos, mesh.vbOther, mesh.vbTexIndices }; }

	DrawStats draw(D3d d3d, uint32_t count, uint32_t startIndex)
	{
		DrawStats stats; 
		if (!worldSettings.debugSingleDrawEnabled || worldSettings.debugSingleDrawIndex == currentDrawCall) {
			d3d.deviceContext->Draw(count, startIndex);

			stats.verts += count;
			stats.draws++;
		}

		currentDrawCall++;
		return stats;
	}

	// Mesh - the type of the mesh, usually MeshBatch
	// VbCount - how many vertex buffer slots are used by a mesh
	// GetVbs - function that returns each of those vertex buffers per mesh
	// GetTex - function that returns texture per mesh
	// clusteredMesh - true if meshes should be frustum culled based on grid cell index (requires "vertClusters" member)
	template<
		typename Mesh, int VbCount,
		GetVertexBuffers<Mesh, VbCount> GetVbs,
		GetTexSrv<Mesh> GetTex,
		bool clusteredMesh
	>
	DrawStats drawVertexBuffers(D3d d3d, const vector<Mesh>& meshes)
	{
		DrawStats stats;

		for (auto& mesh : meshes) {
			if (GetTex != nullptr) {
				ID3D11ShaderResourceView* srv = GetTex(mesh);
				d3d.deviceContext->PSSetShaderResources(0, 1, &srv);
			}
			util::setVertexBuffers(d3d, GetVbs(mesh));

			// TODO cutoff should be adjustable in GUI
			uint32_t ignoreAllChunksVertThreshold = 1000;
			if (!clusteredMesh || !worldSettings.enableFrustumCulling || mesh.vertexCount <= ignoreAllChunksVertThreshold) {
				// for small number of verts per batch we ignore the clusters to save draw calls
				stats += draw(d3d, mesh.vertexCount, 0);
			}
			else {
				uint32_t ignoreChunkVertThreshold = 500;
				const vector<ChunkVertCluster>& vertClusters = mesh.vertClusters;

				// TODO since max number of cells is known, we could probably re-use a single static vector for all meshes
				vector<std::pair<uint32_t, uint32_t>> vertRanges;
				vertRanges.reserve(vertClusters.size());
				
				bool rangeActive = false;
				uint32_t rangeStart = 0;

				// TODO chunks with very few verts should be allowed to be enabled if range is active currently, if that helps joining ranges (test!)

				for (uint32_t i = 0; i < vertClusters.size(); i++) {
					const ChunkIndex& chunkIndex = vertClusters[i].pos;
					bool currentChunkActive = chunkgrid::intersectsCamera(chunkIndex);

					if (!rangeActive && currentChunkActive) {
						// range start
						rangeStart = vertClusters[i].vertStartIndex;
						rangeActive = true;
					}
					if (rangeActive && !currentChunkActive) {
						// range end
						rangeActive = false;
						vertRanges.push_back({ rangeStart, vertClusters[i].vertStartIndex });
					}
				}
				// end last range
				if (rangeActive) {
					rangeActive = false;
					vertRanges.push_back({ rangeStart, mesh.vertexCount });
				}

				for (const auto [startIncl, endExcl] : vertRanges) {
					uint32_t vertCount = endExcl - startIncl;

					vertCount = (uint32_t)(vertCount * worldSettings.debugDrawVertAmount);
					vertCount -= (vertCount % 3);

					stats += draw(d3d, vertCount, startIncl);
				}
			}

			stats.stateChanges++;
		}

		return stats;
	};

	template<int VbCount, GetVertexBuffers<MeshBatch, VbCount> GetVbs>
	DrawStats drawVertexBuffersWorld(D3d d3d, bool bindTex)
	{
		DrawStats stats;
		if (worldSettings.drawWorld) {
			d3d.annotation->BeginEvent(L"World");
			if (bindTex) {
				stats += drawVertexBuffers<MeshBatch, VbCount, GetVbs, batchGetTex, true>(d3d, world.meshBatchesWorld);
			}
			else {
				stats += drawVertexBuffers<MeshBatch, VbCount, GetVbs, nullptr, true>(d3d, world.meshBatchesWorld);
			}
			d3d.annotation->EndEvent();
		}
		if (worldSettings.drawStaticObjects) {
			d3d.annotation->BeginEvent(L"Objects");
			if (bindTex) {
				stats += drawVertexBuffers<MeshBatch, VbCount, GetVbs, batchGetTex, false>(d3d, world.meshBatchesObjects);
			}
			else {
				stats += drawVertexBuffers<MeshBatch, VbCount, GetVbs, nullptr, false>(d3d, world.meshBatchesObjects);
			}
			d3d.annotation->EndEvent();
		}
		return stats;
	};

	void drawPrepass(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		d3d.annotation->BeginEvent(L"Prepass");
		Shader* shader = shaders->getShader("depthPrepass");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(nullptr, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);

		drawVertexBuffers<PrepassMeshes, 1, prepassGetVbPos, nullptr, true>(d3d, world.prepassMeshes);
		d3d.annotation->EndEvent();
	}

	void drawWireframe(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		Shader* shader = shaders->getShader("wireframe");
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);
		
		drawVertexBuffersWorld<1, batchGetVbPos>(d3d, false);
	}

	void drawWorld(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs)
	{
		d3d.annotation->BeginEvent(L"Main");

		// set the shader objects avtive
		Shader* shader;
		if (worldSettings.debugWorldShaderEnabled) {
			shader = shaders->getShader("mainPassTexOnly");
		}
		else {
			shader = shaders->getShader("mainPass");
		}
		
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
		d3d.deviceContext->PSSetShaderResources(1, 1, &world.lightmapTexArray);

		maxDrawCalls = currentDrawCall;
		currentDrawCall = 0;

		DrawStats stats;
		if (worldSettings.debugWorldShaderEnabled) {
			stats = drawVertexBuffersWorld<3, batchGetVbAll>(d3d, true);
		}
		else {
			stats = drawVertexBuffersWorld<3, batchGetVbAll>(d3d, true);
		}
		stats::takeSample(samplers.stateChanges, stats.stateChanges);
		stats::takeSample(samplers.draws, stats.draws);
		stats::takeSample(samplers.verts, stats.verts);

		d3d.annotation->EndEvent();
	}

	void clean()
	{
		clearLevel();
		release(linearSamplerState);
	}
}