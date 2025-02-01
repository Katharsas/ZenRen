#pragma once

#include "Util.h"
#include "render/Common.h"
#include "render/Loader.h"
#include "assets/LookupTrees.h"

namespace assets
{
    namespace FormatsSource
    {
        const ::util::FileExt __3DS = ::util::FileExt::create(".3DS");
        const ::util::FileExt ASC = ::util::FileExt::create(".ASC");
        const ::util::FileExt MDS = ::util::FileExt::create(".MDS");
        const ::util::FileExt PFX = ::util::FileExt::create(".PFX");
        const ::util::FileExt MMS = ::util::FileExt::create(".MMS");
        const ::util::FileExt TGA = ::util::FileExt::create(".TGA");
    }

    // see https://github.com/Try/OpenGothic/wiki/Gothic-file-formats
    // MRM = single mesh (multiresolution)
    // MDM = single mesh
    // MDH = model hierarchy
    // MDL = model hierarchy including all used single meshes
    namespace FormatsCompiled
    {
        const ::util::FileExt TEX = ::util::FileExt::create(".TEX");
        const ::util::FileExt MRM = ::util::FileExt::create(".MRM");
        const ::util::FileExt MDM = ::util::FileExt::create(".MDM");
        const ::util::FileExt MDH = ::util::FileExt::create(".MDH");
        const ::util::FileExt MDL = ::util::FileExt::create(".MDL");
        const ::util::FileExt MAN = ::util::FileExt::create(".MAN");
    }

    COLOR interpolateColorFromFaceXZ(const VEC3& pos, const render::VERT_CHUNKS_BY_MAT& meshData, const render::VertKey& vertKey);
    render::VobLighting calculateStaticVobLighting(
        std::array<DirectX::XMVECTOR, 2> bbox, const FaceLookupContext& worldMesh, const LightLookupContext& lightsStatic, bool isOutdoorLevel,
        LoadDebugFlags debug);
}