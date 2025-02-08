#include "stdafx.h"
#include "PassSky.h"

#include "render/WinDx.h"
#include "render/d3d/ConstantBuffer.h"
#include "render/d3d/GeometryBuffer.h"
#include "render/Renderer.h"
#include "render/MeshUtil.h"
#include "render/Camera.h"

#include "assets/AssetFinder.h"
#include "assets/TexLoader.h"

// Sky is always rendered in relation to player camera with position 0,0,0 (origin).
// This means the sky is always centered above the player.
// It is rendered without writing depth, so always behind other parts of the world.
// Without fog its poly border could be seen over long distances (toward ocean) in G1 Vanilla.

namespace render::pass::sky
{
    using std::vector;
    using std::array;
    using std::unordered_map;
    using DirectX::XMVECTOR;

    struct LAYER_UVS {
        UV uvBase;
        UV uvOverlay;
    };

    typedef VEC3 VertexPos;
    struct VEC_VERTEX_DATA {
        std::vector<VertexPos> vecPos;
        std::vector<LAYER_UVS> vecOther;
    };

    __declspec(align(16))
    struct CbSkyLayer {
        COLOR light;// original only uses values other than 1 on overlay
        float alpha;
        uint32_t blurDisabled;
    };

    __declspec(align(16))
    struct CbSkyLayerSettings {
        // Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing
        COLOR colBackground;
        CbSkyLayer texLayers[2];
    };

    float lastTimeOfDay = defaultTime;
    array<Texture*, 2> layerTextures;
    bool lastWasSwapped = true;
    array<UV, 2> lastUvMin = { UV { 0, 0 }, UV { 0, 0 } };
    array<UV, 2> lastUvMax = { UV { 0, 0 }, UV { 0, 0 } };

    struct SkyMesh
    {
        int32_t vertexCount = 0;
        VertexBuffer vbPos = { sizeof(VertexPos) };
        VertexBuffer vbUvs = { sizeof(LAYER_UVS) };

        void release()
        {
            render::release(vbPos.buffer);
            render::release(vbUvs.buffer);
        }
    } mesh;

    struct SkyTexture {
        std::string name;
        Texture* tex;
    };
    array<SkyTexture, 4> skyTextures;

    ID3D11SamplerState* linearSamplerState = nullptr;
    ID3D11Buffer* skyLayerSettingsCb = nullptr;

    vector<array<XMVECTOR, 3>> createSkyVerts()
    {
        float height = 10; // 10
        auto a = toXM4Pos(VEC3 { -500, height, -500 });
        auto b = toXM4Pos(VEC3 { 500, height, -500 });
        auto c = toXM4Pos(VEC3 { 500, height, 500 });
        auto d = toXM4Pos(VEC3 { -500, height, 500 });

        vector result = {
            array { a, b, c },
            array { a, c, d },
        };
        return result;
    }

    vector<array<UV, 3>> createSkyUvs(UV min, UV max) {
        auto a = UV{ min.u, min.v };
        auto b = UV{ max.u, min.v };
        auto c = UV{ max.u, max.v };
        auto d = UV{ min.u, max.v };
        return {
            array { a, b, c },
            array { a, c, d },
        };
    }

    Texture* loadSkyTexture(D3d d3d, std::string texName)
    {
        // TODO centralize code duplication with world, move into separate texture cache?
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

    void loadSky(D3d d3d)
    {
        {
            vector<VertexPos> facesPos;
            const auto& quadFacesXm = createSkyVerts();
            for (auto& posXm : quadFacesXm) {
                for (int32_t i = 0; i < 3; i++) {
                    facesPos.push_back(toVec3(posXm[i]));
                }
            }

            mesh.vertexCount = facesPos.size();
            d3d::createVertexBuf(d3d, &mesh.vbPos.buffer, facesPos);
        } {
            // textures
            // TODO original game has (and mods could have even more) sky variants (A0, A1, ..)
            skyTextures = {
                // base day / night
                "SKYDAY_LAYER1", loadSkyTexture(d3d, "SKYDAY_LAYER1_A0.TGA"),
                "SKYNIGHT_LAYER0", loadSkyTexture(d3d, "SKYNIGHT_LAYER0_A0.TGA"),
                // overlay day / night
                "SKYDAY_LAYER0", loadSkyTexture(d3d, "SKYDAY_LAYER0_A0.TGA"),
                "SKYNIGHT_LAYER1", loadSkyTexture(d3d, "SKYNIGHT_LAYER1_A0.TGA"),
            };
        }
    }

    void updateSkyUvs(D3d d3d)
    {
        vector<LAYER_UVS> facesOther;
        const auto& baseUvs = createSkyUvs(lastUvMin[0], lastUvMax[0]);
        const auto& overlayUvs = createSkyUvs(lastUvMin[1], lastUvMax[1]);
        auto it = overlayUvs.begin();
        for (auto& baseFace : baseUvs) {
            auto& overlayFace = *it;
            for (int32_t i = 0; i < 3; i++) {
                facesOther.push_back({ baseFace[i], overlayFace[i]});
            }
            it++;
        }
        
        assert(mesh.vertexCount == facesOther.size());
        d3d::createVertexBuf(d3d, mesh.vbUvs, facesOther);// TODO we should probably update the buffer instead of recreating it
    }

    void swapUvsIfSwapOccured(bool swapLayers) {
        bool swapUvs = lastWasSwapped != swapLayers;
        lastWasSwapped = swapLayers;
        if (swapUvs) {
            std::swap(lastUvMin[0], lastUvMin[1]);
            std::swap(lastUvMax[0], lastUvMax[1]);
        }
    }

    void updateSkyLayers(D3d d3d, const array<SkyTexState, 2>& layerStates, const COLOR& skyBackground, float timeOfDay, bool swapLayers)
    {
        float delta1 = timeOfDay - lastTimeOfDay;
        float delta2 = timeOfDay + 1 - lastTimeOfDay;
        float delta1Abs = std::abs(delta1);
        float delta2Abs = std::abs(delta2);
        bool delta1LessThanEquals = delta1Abs <= delta2Abs;
        float timeDelta = copysign(delta1LessThanEquals ? delta1Abs : delta2Abs, delta1LessThanEquals ? delta1 : delta2);
        lastTimeOfDay = timeOfDay;

        int32_t layerIndex[2] = { swapLayers ? 1 : 0, swapLayers ? 0 : 1 };
        swapUvsIfSwapOccured(swapLayers);

        float timeDeltaUv = 4 * timeDelta;
        for (int32_t i = 0; i < 2; i++) {
            SkyTexState state = layerStates[layerIndex[i]];
            lastUvMin[i] = add(lastUvMin[i], mul(state.uvSpeed, timeDeltaUv));
            lastUvMax[i] = add(lastUvMin[i], mul(UV { 1, 1 }, state.tex.uvScale));
            for (auto tex : skyTextures) {
                if (state.tex.name == tex.name) {
                    layerTextures[i] = tex.tex;
                    break;
                }
            }
            assert(layerTextures[i] != nullptr);
        }
        updateSkyUvs(d3d);

        CbSkyLayerSettings layerSettings;
        layerSettings.colBackground = skyBackground;
        for (int32_t i = 0; i < 2; i++) {
            SkyTexState state = layerStates[layerIndex[i]];
            layerSettings.texLayers[i].light = state.texlightColor;
            layerSettings.texLayers[i].alpha = state.alpha;
            layerSettings.texLayers[i].blurDisabled = state.tex.blurDisabled;
        }

        d3d::updateConstantBuf(d3d, skyLayerSettingsCb, layerSettings);
    }

    void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, ID3D11SamplerState* layerSampler)
    {
        d3d.annotation->BeginEvent(L"sky");

        // set the shader objects avtive
        Shader* shader = shaders->getShader("sky");
        d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
        d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
        d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

        // constant buffers
        d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbs.settingsCb);
        d3d.deviceContext->PSSetConstantBuffers(0, 1, &cbs.settingsCb);
        d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbs.cameraCb);
        d3d.deviceContext->PSSetConstantBuffers(1, 1, &cbs.cameraCb);
        d3d.deviceContext->VSSetConstantBuffers(2, 1, &skyLayerSettingsCb);
        d3d.deviceContext->PSSetConstantBuffers(2, 1, &skyLayerSettingsCb);

        // textures
        d3d.deviceContext->PSSetSamplers(0, 1, &layerSampler);
        for (uint32_t i = 0; i < layerTextures.size(); i++) {
            auto* resourceView = layerTextures[i]->GetResourceView();
            d3d.deviceContext->PSSetShaderResources(i, 1, &resourceView);
        }
        
        // vertex buffer
        d3d::setVertexBuffers(d3d, array { mesh.vbPos , mesh.vbUvs });
        d3d.deviceContext->Draw(mesh.vertexCount, 0);

        d3d.annotation->EndEvent();
    }

    void initConstantBuffers(D3d d3d)
    {
        // TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
        d3d::createConstantBuf<CbSkyLayerSettings>(d3d, &skyLayerSettingsCb, BufferUsage::WRITE_GPU);
    }

    void clean()
    {
        mesh.release();
        for (auto skyTexture : skyTextures) {
            delete skyTexture.tex;
        }
        release(linearSamplerState);
        release(skyLayerSettingsCb);
    }
}
