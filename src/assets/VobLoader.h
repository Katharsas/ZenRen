#pragma once

#include "Util.h"
#include "render/basic/Common.h"
#include "render/Loader.h"
#include "assets/LookupTrees.h"

namespace assets
{
    // TODO rename to VobLighting
    Color interpolateColorFromFaceXZ(const Vec3& pos, const render::MatToChunksToVertsBasic& meshData, const render::VertKey& vertKey);
    render::VobLighting calculateStaticVobLighting(
        std::array<DirectX::XMVECTOR, 2> bbox, const FaceLookupContext& worldMesh, const LightLookupContext& lightsStatic, bool isOutdoorLevel,
        LoadDebugFlags debug);
}