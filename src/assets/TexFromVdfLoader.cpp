#include "stdafx.h"
#include "TexFromVdfLoader.h"

#include "render/WinDx.h"
#include "zenload/ztex2dds.h"

namespace assets
{
    using namespace ZenLib;
    using ::render::InMemoryTexFile;
    using ::std::string;
    using ::std::vector;

    std::optional<InMemoryTexFile> loadTex(const FileHandle& file)
    {
        FileData fileRaw = getData(file);

        InMemoryTexFile tex;
        int32_t message;
        if (fileRaw.buffer.has_value()) {
            // if we have loaded from zenlib VFS we are lucky and don't need to copy
            vector<uint8_t>* fileRawVec = fileRaw.buffer.value().get();
            message = ZenLoad::convertZTEX2DDS(*fileRawVec, tex.ddsRaw, false, &tex.width, &tex.height);
        }
        else {
            // sadly, convertZTEX2DDS takes a vector and not a span<uint8_t>, so in this case we have to copy into vector
            vector<uint8_t> fileRawVec = vector<uint8_t>((std::uint8_t*) fileRaw.data, ((std::uint8_t*) fileRaw.data) + fileRaw.size);

            // TODO maybe make pr to switch to span?
            message = ZenLoad::convertZTEX2DDS(fileRawVec, tex.ddsRaw, false, &tex.width, &tex.height);
        }
        if (message != 0) {
            LOG(WARNING) << "Failed to convert zTex '" << "unknown" << "' to DDS: Error code '" << message << "'!";
            return std::nullopt;
        }
        return tex;
    }

    vector<InMemoryTexFile> loadZenLightmaps(const ZenLoad::zCMesh* worldMesh) {
        vector<InMemoryTexFile> result;

        for (auto& lightmap : worldMesh->getLightmapTextures()) {
            InMemoryTexFile tex;
            int32_t message = ZenLoad::convertZTEX2DDS(lightmap, tex.ddsRaw, false, &tex.width, &tex.height);
            if (message != 0) {
                LOG(FATAL) << "Failed to convert lightmap zTex to DDS: Error code '" << message << "'!";
            }
            result.push_back(tex);
        }
        return result;
    }

    vector<InMemoryTexFile> loadLightmapsZkit(const zenkit::Mesh& worldMesh) {
        vector<InMemoryTexFile> result;

        for (auto& lightmap : worldMesh.lightmap_textures) {
            InMemoryTexFile tex;
            int32_t message = ZenLoad::convertZTEX2DDS(lightmap, tex.ddsRaw, false, &tex.width, &tex.height);
            if (message != 0) {
                LOG(FATAL) << "Failed to convert lightmap zTex to DDS: Error code '" << message << "'!";
            }
            result.push_back(tex);
        }
        return result;
    }
}
