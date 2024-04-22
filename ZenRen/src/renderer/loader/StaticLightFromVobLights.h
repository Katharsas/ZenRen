#pragma once

#include "../RendererCommon.h"
#include "LookupTrees.h"

namespace renderer::loader
{
    struct DirectionalLight {
        D3DXCOLOR color;
        DirectX::XMVECTOR dirInverted;
    };

    std::optional<DirectionalLight> getLightAtPos(
            DirectX::XMVECTOR posXm,
            const std::vector<Light>& lights,
            const LightLookupTree& lightLookup,
            const std::unordered_map<Material, VEC_VERTEX_DATA>& worldMeshData,
            const VertLookupTree& worldFaceLookup);

    // debug stuff

    extern int32_t vobLightWorldIntersectChecks;

    struct DebugLine {
        VEC3 posStart;
        VEC3 posEnd;
        D3DXCOLOR color;
    };
    extern std::vector<DebugLine> debugLightToVobRays;
}