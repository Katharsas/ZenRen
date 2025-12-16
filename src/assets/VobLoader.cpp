#include "stdafx.h"
#include "VobLoader.h"

#include "assets/AssetFinder.h"
#include "assets/FindGroundFace.h"
#include "assets/StaticLightFromVobLights.h"

#include "render/basic/MeshUtil.h"

namespace assets
{
    // TODO rename to VobLighting or similar?

    using namespace render;
    using namespace DirectX;
    using std::string;
    using std::optional;
    using std::vector;
    using ::util::FileExt;

    Color interpolateColorFromFaceXZ(const Vec3& pos, const MatToChunksToVertsBasic& meshData, const VertKey& vertKey)
    {
        auto facePos = vertKey.getPos(meshData);
        float v0Distance = std::sqrt(std::pow(facePos[0].x - pos.x, 2.f) + std::pow(facePos[0].z - pos.z, 2.f));
        float v1Distance = std::sqrt(std::pow(facePos[1].x - pos.x, 2.f) + std::pow(facePos[1].z - pos.z, 2.f));
        float v2Distance = std::sqrt(std::pow(facePos[2].x - pos.x, 2.f) + std::pow(facePos[2].z - pos.z, 2.f));
        float totalDistance = v0Distance + v1Distance + v2Distance;
        float v0Contrib = 1 - (v0Distance / totalDistance);
        float v1Contrib = 1 - (v1Distance / totalDistance);
        float v2Contrib = 1 - (v2Distance / totalDistance);

        auto faceOther = vertKey.getOther(meshData);
        Color colorAverage = mul(
            add(add(
                mul(faceOther[0].colLight(), v0Contrib),
                mul(faceOther[1].colLight(), v1Contrib)),
                mul(faceOther[2].colLight(), v2Contrib)),
            0.5f);

        return colorAverage;
    }

    VobLighting calculateStaticVobLighting(
        std::array<XMVECTOR, 2> bbox, const FaceLookupContext& worldMesh, const LightLookupContext& lightsStatic, bool isOutdoorLevel,
        LoadDebugFlags debug)
    {
        VobLighting result;
        result.direction = -1 * XMVectorSet(1, -0.5, -1.0, 0);

        const XMVECTOR center = render::bboxCenter(bbox);
        const std::optional<VertKey> vertKey = getGroundFaceAtPos(center, worldMesh.data, worldMesh.spatialTree);

        bool hasLightmap = false;
        if (!isOutdoorLevel) {
            hasLightmap = true;
        }
        else if (vertKey.has_value()) {
            const auto& other = vertKey.value().getOther(worldMesh.data);
            if (other[0].type() == VertexLightType::WORLD_LIGHTMAP) {
                hasLightmap = true;
            }
        }
        result.receiveLightSun = !hasLightmap;

        if (hasLightmap) {
            // TODO
            // The more lights get summed, the bigger the SRGB summing error is. 
            // More light additions -> brighter in SRGB than linar; less lights -> closer brightness
            // The final weight (0.71f) in Vanilla Gothic might be there to counteract this error, but should lead to objects
            // hit by less lights to be overly dark. Maybe adjust weight to lower value? Check low hit objects.

            auto optLight = getLightAtPos(center, lightsStatic.data, lightsStatic.spatialTree, worldMesh.data, worldMesh.spatialTree, debug.disableVobToLightVisibilityRayChecks);
            if (optLight.has_value()) {
                result.color = optLight.value().color;
                result.color = multiplyColor(result.color, fromSRGB(0.85f));

                result.direction = optLight.value().dirInverted;
            }
            else {
                result.color = Color(0, 0, 0, 1);// no lights reached this vob, so its black
                if (debug.staticLightTintUnreached) {
                    result.color = Color(0, 1, 0, 1);
                }
            }
            result.color.a = 0;// indicates that this VOB receives no sky light
        }
        else {
            if (vertKey.has_value()) {
                result.color = interpolateColorFromFaceXZ(toVec3(center), worldMesh.data, vertKey.value());

                // outdoor vobs get additional fixed ambient
                if (isOutdoorLevel) {
                    // TODO
                    // Original game knows if a ground poly is in a portal room (without lightmap, like a cave) or actually outdoors
                    // from per-ground-face flag. Find out how this is calculated or shoot some rays towards the sky.
                    bool isVobIndoor = false;

                    float minLight = isVobIndoor ? fromSRGB(0.2f) : fromSRGB(0.5f);
                    result.color = add(mul(result.color, (1.f - minLight)), greyscale(minLight));
                    // currently this results in light values between between 0.22 and 0.99 
                }
            }
            else {
                if (debug.vobsTintUnlit) {
                    result.color = Color{ 0.5, 0.f, 0.f, 1.f };
                }
                else {
                    result.color = fromSRGB(greyscale(0.63f));// fallback lightness of (160, 160, 160)
                }
            }
            result.color.a = 1;// indicates that this VOB receives full sky light
        }
        return result;
    }
}