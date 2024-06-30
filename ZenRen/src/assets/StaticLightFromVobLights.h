#pragma once

#include "render/Common.h"
#include "LookupTrees.h"

namespace assets
{
    struct DirectionalLight {
        D3DXCOLOR color;
        DirectX::XMVECTOR dirInverted;
    };

    std::optional<DirectionalLight> getLightAtPos(
            DirectX::XMVECTOR posXm,
            const std::vector<render::Light>& lights,
            const LightLookupTree& lightLookup,
            const render::VERTEX_DATA_BY_MAT& worldMeshData,
            const VertLookupTree& worldFaceLookup);

    // debug stuff

    extern uint32_t vobLightWorldIntersectChecks;

    struct DebugLine {
        VEC3 posStart;
        VEC3 posEnd;
        D3DXCOLOR color;
    };
    extern std::vector<DebugLine> debugLightToVobRays;
}