#pragma once

#include "render/Common.h"
#include "LookupTrees.h"

namespace assets
{
    struct DirectionalLight {
        Color color;
        DirectX::XMVECTOR dirInverted;
    };

    std::optional<DirectionalLight> getLightAtPos(
            DirectX::XMVECTOR posXm,
            const std::vector<render::Light>& lights,
            const LightLookupTree& lightLookup,
            const render::MatToChunksToVertsBasic& worldMeshData,
            const VertLookupTree& worldFaceLookup,
            bool disableVisibilityRayChecks);

    // debug stuff

    extern uint32_t vobLightWorldIntersectChecks;

    struct DebugLine {
        Vec3 posStart;
        Vec3 posEnd;
        Color color;
    };
    extern std::vector<DebugLine> debugLightToVobRays;
}