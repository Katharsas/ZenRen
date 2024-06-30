#include "stdafx.h"
#include "FindGroundFace.h"

#include "render/MeshUtil.h"

namespace assets
{
    using namespace render;
    using namespace DirectX;
    using ::std::string;
    using ::std::array;
    using ::std::vector;
    using ::std::unordered_map;

    const bool useNaiveSlowGroundFaceSearch = false;
    const bool tintVobStaticLight = false;// make it possible to distinguish vobs from level

    __inline bool triangleXZContainsPointXZ(const VEC3& point, const VEC3& a, const VEC3& b, const VEC3& c) {
        // From https://math.stackexchange.com/a/4624564
        // Conceptually, we create new triangles of P with each side of given triangle (barycentric approach).
        // If those new triangles have identical winding orders with original triangle, point is inside triangle.
        // We get winding order by taking third term of cross product for each triangle.

        float AP_x = point.x - a.x;
        float AP_z = point.z - a.z;

        float AB_x = b.x - a.x;
        float AB_z = b.z - a.z;
        bool thirdTermABxAPisPositive = (AB_x * AP_z) - (AB_z * AP_x) > 0;// true if PAB has clockwise winding

        float AC_x = c.x - a.x;
        float AC_z = c.z - a.z;
        bool thirdTermACxAPisPositive = (AC_x * AP_z) - (AC_z * AP_x) > 0;// true if PAC has clockwise winding / PCA has counter-clockwise winding

        if (thirdTermACxAPisPositive == thirdTermABxAPisPositive) return false;// PAB and PCA must have same winding (inverted since we check PAC)

        float BC_x = c.x - b.x;
        float BC_z = c.z - b.z;
        float BP_x = point.x - b.x;
        float BP_z = point.z - b.z;
        bool thirdTermBCxBPisPositive = (BC_x * BP_z) - (BC_z * BP_x) > 0;

        if (thirdTermBCxBPisPositive != thirdTermABxAPisPositive) return false;// PAB and PBC must have same winding

        return true;
    }

    void logPosAndGroundPolyCandidatesJson(const VEC3& pos, const vector<VERTEX_POS>& verts) {
        // https://jsfiddle.net/fzdb6j17/53/
        LOG(INFO) << "";
        LOG(INFO) << "POINT    { x: " + std::to_string(pos.x) + ", y: " + std::to_string(pos.z) + " }";
        LOG(INFO) << "";
        string json = "";
        for (uint32_t vertIndex = 0; vertIndex < verts.size(); vertIndex += 3) {
            auto& a = verts[vertIndex];
            auto& b = verts[vertIndex + 1];
            auto& c = verts[vertIndex + 2];
            json += "{ ";
            json += "a: { x: " + std::to_string(a.x) + ", y: " + std::to_string(a.z) + " }, ";
            json += "b: { x: " + std::to_string(b.x) + ", y: " + std::to_string(b.z) + " }, ";
            json += "c: { x: " + std::to_string(c.x) + ", y: " + std::to_string(c.z) + " } ";
            json += "}, ";
        }
        LOG(INFO) << json;
        LOG(INFO) << "";
    }

    __inline int closestGroundFace(const VEC3& pos, const vector<VERTEX_POS>& verts) {
        //logPosAndGroundPolyCandidatesJson(pos, verts);

        // TODO there must still be a bug here because VOBs that have multiple faces below & above them are not lit correctly
        float currentBestYDist = FLT_MAX;
        //float currentBestY = FLT_MAX;
        int32_t currentBestIndex = -1;
        for (uint32_t vertIndex = 0; vertIndex < verts.size(); vertIndex += 3) {
            bool isInsideTriangle2D = triangleXZContainsPointXZ(pos, verts[vertIndex], verts[vertIndex + 1], verts[vertIndex + 2]);
            if (!isInsideTriangle2D) {
                // filter out faces that don't intersect with pos in 2D 
                continue;
            }
            float yAverage = 0;
            for (uint32_t i = vertIndex; i < vertIndex + 3; i++) {
                auto& vert = verts[i];
                yAverage += vert.y;
            }
            yAverage /= 3;
            float yDist = std::abs(pos.y - yAverage);

            // TODO implement better ground face choice
            // - get exact height of potential ground face at 2D pos so we know how high the ground actually is at pos (but this would not work for wall-mounted vobs)!
            // - ideally we would also know bounding box of object instead of just pos so we can make Y tolerance for objects based on their size
            bool isBetterGroundCandidate = false;
            float toleranceY = 3;

            // TODO to properly debug this we need to show selected ground faces in the world of get debugger to stop at problematic vobs
            if (yDist < currentBestYDist) {
                isBetterGroundCandidate = true;
            }

            /*
            // if nearer and below object, we take it
            if (yDist < currentBestYDist && yAverage < (pos.y + toleranceY)) {
                isBetterGroundCandidate = true;
            }
            // if substantially nearer, we take it
            if (yDist < (currentBestYDist + 20)) {
                isBetterGroundCandidate = true;
            }
            // if almost as near and below object, we take it
            float distanceDiff = std::abs(yDist - currentBestYDist);
            if (distanceDiff < 10 && currentBestY > (pos.y + toleranceY) && yAverage < (pos.y + toleranceY)) {
                isBetterGroundCandidate = true;
            }
            */

            if (isBetterGroundCandidate) {
                currentBestYDist = yDist;
                //currentBestY = yAverage;
                currentBestIndex = (int32_t) vertIndex / 3;
            }
        }
        return currentBestIndex;
    }

    std::optional<VertKey> getGroundFaceAtPos(const XMVECTOR pos, const unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertLookupTree& vertLookup)
    {
        VEC3 pos3 = toVec3(pos);
        vector<VertKey> vertKeys;
        if (useNaiveSlowGroundFaceSearch) {
            vertKeys = rayDownIntersectedNaive(meshData, pos3, 100);
        }
        else {
            vertKeys = rayDownIntersected(vertLookup, pos3, 100);
        }
        vector<VERTEX_POS> belowVerts;
        for (auto& vertKey : vertKeys) {
            auto& vecPos = vertKey.get(meshData).vecPos;
            belowVerts.push_back(vecPos[vertKey.vertIndex]);
            belowVerts.push_back(vecPos[vertKey.vertIndex + 1]);
            belowVerts.push_back(vecPos[vertKey.vertIndex + 2]);
        }
        int closestIndex = closestGroundFace(pos3, belowVerts);
        if (closestIndex == -1) {
            return std::nullopt;
        }
        else {
            return vertKeys[closestIndex];
        }
    }
}