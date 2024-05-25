#include "stdafx.h"
#include "PipelineSky.h"

#include "loader/MeshUtil.h"
#include "Renderer.h"
#include "Camera.h"
#include "loader/AssetFinder.h";
#include "loader/TexFromVdfLoader.h"
#include "RenderUtil.h";

namespace renderer::sky
{
    using std::vector;
    using std::array;
    using std::unordered_map;
    using DirectX::XMVECTOR;
    using loader::toXM4Pos;
    using loader::calcFlatFaceNormal;
    using loader::toVec3;
    using loader::insert;

    __declspec(align(16))
    struct CbSkyLayerSettings {
        // Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing
        float texAlphaBase;
        float texAlphaOverlay;
        UV uvScaleBase;
        UV uvScaleOverlay;
        float translBase;
        float translOverlay;
        D3DXCOLOR lightOverlay;// original only uses values other than 1 on overlay
    };

    VEC_VERTEX_DATA skyVertData;
    Mesh mesh;
    array<Texture*, 4> skyTextures;

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

    Texture* loadSkyTexture(D3d d3d, std::string texName)
    {
        // TODO centralize code duplication with world, move into separate texture cache?
        Texture* texture = nullptr;
        auto optionalVdfIndex = loader::getVdfIndex();
        if (optionalVdfIndex.has_value()) {
            std::string zTexName = ::util::replaceExtension(texName, "-c.tex");// compiled textures have -C suffix

            if (optionalVdfIndex.value()->hasFile(zTexName)) {
                InMemoryTexFile tex = loader::loadTex(zTexName, optionalVdfIndex.value());
                texture = new Texture(d3d, tex.ddsRaw, true, zTexName);
            }
        }
        if (!texture) {
            texture = new Texture(d3d, ::util::toString(loader::DEFAULT_TEXTURE));
        }
        return texture;
    }

    void loadSky(D3d d3d) {
        {
            // vert data
            vector<VERTEX_POS> facesPos;
            vector<VERTEX_OTHER> facesOther;
            const auto& bboxFacesXm = createSkyVerts();
            for (auto& posXm : bboxFacesXm) {
                const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
                for (int32_t i = 0; i < 3; i++) {
                    facesPos.push_back(toVec3(posXm[i]));
                    facesOther.push_back({
                        faceNormal,
                        UV { 0, 0 },
                        ARRAY_UV { 0, 0, -1 },
                        D3DXCOLOR(0.01, 0, 0, 1),
                        VEC3(-100, -100, -100),
                        0,
                        });
                }
            }
            skyVertData = { facesPos, facesOther };
        } {
            // vertex buffers
            mesh.vertexCount = skyVertData.vecPos.size();
            util::createVertexBuffer(d3d, &mesh.vertexBufferPos, skyVertData.vecPos);
            util::createVertexBuffer(d3d, &mesh.vertexBufferNormalUv, skyVertData.vecNormalUv);
        } {
            // textures
            // TODO original game has (and mods could have even more) sky variants (A0, A1, ..)
            skyTextures = {
                // base day / night
                loadSkyTexture(d3d, "SKYDAY_LAYER1_A0.TGA"),
                loadSkyTexture(d3d, "SKYNIGHT_LAYER0_A0.TGA"),
                // overlay day / night
                loadSkyTexture(d3d, "SKYDAY_LAYER0_A0.TGA"),
                loadSkyTexture(d3d, "SKYNIGHT_LAYER1_A0.TGA"),
            };
        }
    }

    void updateSkyLayers(D3d d3d, const array<SkyTexState, 2>& layerStates) {
        //LOG(INFO) << "BASE " << layerStates[0];
        //LOG(INFO) << "OVER " << layerStates[1];

        CbSkyLayerSettings layerSettings;
        layerSettings.texAlphaBase = layerStates[0].alpha;
        layerSettings.texAlphaOverlay = layerStates[1].alpha;
        layerSettings.uvScaleBase = { 1, 1 };
        layerSettings.uvScaleOverlay = { 1, 1 };
        layerSettings.translBase = 0;
        layerSettings.translOverlay = 0;
        layerSettings.lightOverlay = layerStates[1].texlightColor;
        d3d.deviceContext->UpdateSubresource(skyLayerSettingsCb, 0, nullptr, &layerSettings, 0, 0);
    }

    // Sky is always rendered in relation to player camera with position 0,0,0 (origin).
    // This means the sky is always centered above the player.
    // It is rendered without writing depth, so always behind other parts of the world.
    // Without fog its poly border could be seen over long distances (toward ocean) in G1 Vanilla.

    void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, ID3D11SamplerState* layerSampler, const array<SkyTexState, 2>& layerStates)
    {
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
        array textures = {
            layerStates[0].type == SkyTexType::DAY ? skyTextures[0] : skyTextures[1],
            layerStates[1].type == SkyTexType::DAY ? skyTextures[2] : skyTextures[3],
        };
        for (int i = 0; i < textures.size(); i++) {
            auto* resourceView = textures[i]->GetResourceView();
            d3d.deviceContext->PSSetShaderResources(i, 1, &resourceView);
        }
        
        // vertex buffer
        UINT strides[] = { sizeof(VERTEX_POS), sizeof(VERTEX_OTHER) };
        UINT offsets[] = { 0, 0 };
        ID3D11Buffer* vertexBuffers[] = { mesh.vertexBufferPos, mesh.vertexBufferNormalUv };

        d3d.deviceContext->IASetVertexBuffers(0, std::size(vertexBuffers), vertexBuffers, strides, offsets);
        d3d.deviceContext->Draw(mesh.vertexCount, 0);
    }

    void initConstantBuffers(D3d d3d)
    {
        // TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
        util::createConstantBuffer<CbSkyLayerSettings>(d3d, &skyLayerSettingsCb, D3D11_USAGE_DEFAULT);
    }
}
