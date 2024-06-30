#include "stdafx.h"
#include "StaticLightFromVobLights.h"

#include "render/MeshUtil.h"

namespace assets
{
    using namespace render;
    using namespace DirectX;
    using ::std::vector;
    using ::std::unordered_map;

    // debug option for faster loading
    bool disableIntersectChecks = false;

    uint32_t vobLightWorldIntersectChecks = 0;

    float debugStaticLightRaysMaxDist = 50;
    vector<DebugLine> debugLightToVobRays;

    bool rayIntersectsWorldFaces(XMVECTOR rayStart, XMVECTOR rayEnd, float maxDistance, const unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertLookupTree& vertLookup)
    {
        XMVECTOR direction = DirectX::XMVector3Normalize(rayEnd - rayStart);
        vector<VertKey> vertKeys = rayIntersected(vertLookup, rayStart, rayEnd);
        for (auto& vertKey : vertKeys) {
            auto& face = vertKey.getPos(meshData);
            XMVECTOR faceA = toXM4Pos(face[0]);
            XMVECTOR faceB = toXM4Pos(face[1]);
            XMVECTOR faceC = toXM4Pos(face[2]);
            float distance;
            bool intersects = TriangleTests::Intersects(rayStart, direction, faceA, faceB, faceC, distance);
            if (intersects && distance <= maxDistance) {
                return true;
            }
        }
        return false;
    }

    std::optional<DirectionalLight> getLightAtPos(
            XMVECTOR posXm,
            const vector<Light>& lights,
            const LightLookupTree& lightLookup,
            const BATCHED_VERTEX_DATA& worldMeshData,
            const VertLookupTree& worldFaceLookup)
    {
        auto pos = toVec3(posXm);
        float rayIntersectTolerance = 0.1f;
        auto searchBox = OrthoBoundingBox3D{
           { pos.x - rayIntersectTolerance, pos.y - rayIntersectTolerance, pos.z - rayIntersectTolerance },
           { pos.x + rayIntersectTolerance, pos.y + rayIntersectTolerance, pos.z + rayIntersectTolerance }
        };

        constexpr bool shouldFullyContain = false;
        auto intersectedBoxes = lightLookup.tree.RangeSearch<shouldFullyContain>(searchBox);

        int32_t contributingLightCount = 0;
        D3DXCOLOR color = D3DXCOLOR(0.f, 0.f, 0.f, 1.f);
        XMVECTOR candidateLightDir = XMVectorSet(0, 0, 0, 0);// TODO this is actually not necessary, but fixes warnings
        float candidateScore = 0;

        for (auto boxIndex : intersectedBoxes) {
            auto& light = lights[boxIndex];
            XMVECTOR lightPos = toXM4Pos(light.pos);
            float dist = XMVectorGetX(XMVector3Length(lightPos - posXm));
            if (dist < (light.range * 1.0f)) {
                bool intersectedWorld = false;
                if (!disableIntersectChecks) {
                    vobLightWorldIntersectChecks++;
                    intersectedWorld = rayIntersectsWorldFaces(lightPos, posXm, dist * 0.85f, worldMeshData, worldFaceLookup);
                }
                if (!intersectedWorld) {
                    contributingLightCount++;
                    float weight = 1.f - (dist / (light.range * 1.0f));
                    weight = fromSRGB(weight);
                    color += (light.color * weight);
                    float perceivedLum = (light.color.r * 0.299f + light.color.g * 0.587f + light.color.b * 0.114f);
                    float score = weight * perceivedLum;
                    if (score > candidateScore) {
                        candidateLightDir = toXM4Pos(light.pos) - posXm;
                        candidateScore = score;
                    }
                }
                if (dist < debugStaticLightRaysMaxDist) {
                    debugLightToVobRays.push_back({ light.pos, pos, intersectedWorld ? D3DXCOLOR(0.f, 0.f, 1.f, 0.5f) : D3DXCOLOR(1.f, 0.f, 0.f, 0.5f) });
                }
            }
        }

        if (contributingLightCount == 0) {
            return std::nullopt;
        }
        else {
            return DirectionalLight{
                color,
                candidateLightDir,
            };
        }
    }
}