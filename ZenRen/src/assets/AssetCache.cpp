#include "stdafx.h"
#include "AssetCache.h"

#include "render/Common.h"
#include "Util.h"

namespace assets
{
	using namespace DirectX;
	using namespace ZenLib;
	using namespace ZenLib::ZenLoad;
	using ::std::string;
	using ::std::unordered_map;
	using ::util::getOrSet;

	unordered_map<string, MeshData> cacheMeshes;
	unordered_map<string, MeshLibData> cacheMeshLibs;

	const MeshData& getOrParseMesh(VDFS::FileIndex& vdf, string meshName, bool pack)
	{
		return getOrSet(cacheMeshes, meshName, [&](MeshData& ref) {
			//LOG(DEBUG) << "Loading: " << meshName;
			ref.hasPacked = pack;
			ref.mesh = ZenLoad::zCProgMeshProto(meshName, vdf);
			if (pack) {
				ref.mesh.packMesh(ref.packed, render::G_ASSET_RESCALE);
			}
		});
	}

	const MeshLibData& getOrParseMeshLib(VDFS::FileIndex& vdf, std::string meshName, bool pack)
	{
		return getOrSet(cacheMeshLibs, meshName, [&](MeshLibData& ref) {
			//LOG(DEBUG) << "Loading: " << meshName;
			ref.hasPacked = pack;
			ref.meshLib = ZenLoad::zCModelMeshLib(meshName, vdf, render::G_ASSET_RESCALE);
			if (pack) {
				{
					uint32_t count = ref.meshLib.getMeshes().size();
					ref.meshesPacked.resize(count);
					for (uint32_t i = 0; i < count; ++i) {
						ref.meshLib.getMeshes()[i].getMesh().packMesh(ref.meshesPacked[i], render::G_ASSET_RESCALE);
					}
				} {
					uint32_t count = ref.meshLib.getAttachments().size();
					ref.attachementsPacked.resize(count);
					for (uint32_t i = 0; i < count; ++i) {
						ref.meshLib.getAttachments()[i].second.packMesh(ref.attachementsPacked[i], render::G_ASSET_RESCALE);
					}
				}
			}
		});
	}
}