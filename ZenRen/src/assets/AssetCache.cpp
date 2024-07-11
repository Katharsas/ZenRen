#include "stdafx.h"
#include "AssetCache.h"

#include "Util.h"

#include <string>
#include <unordered_map>

namespace assets
{
	using namespace DirectX;
	using namespace ZenLib;
	using namespace ZenLib::ZenLoad;
	using ::render::TexId;
	using ::std::string;
	using ::std::vector;
	using ::std::unordered_map;
	using ::util::getOrSet;

	unordered_map<string, MeshData> cacheMeshes;
	unordered_map<string, MeshLibData> cacheMeshLibs;

	vector<string> cacheTexNames;
	unordered_map<int32_t, TexId> cacheTexNameHashToIds;

	const MeshData& getOrParseMesh(VDFS::FileIndex& vdf, const string& meshName, bool pack)
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

	const MeshLibData& getOrParseMeshLib(VDFS::FileIndex& vdf, const string& meshName, bool pack)
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

	// TODO this should probably also lowercase everything, not callers
	TexId getTexId(const std::string& texName) {
		std::string copy = texName;
		std::hash<string> hasher;
		int32_t hash = hasher(copy);
		return getOrSet(cacheTexNameHashToIds, hash, [&](TexId& ref) {
			TexId currentIndex = (TexId) cacheTexNames.size();
			cacheTexNames.push_back(texName);
			ref = currentIndex;
		});
	}

	std::string_view getTexName(TexId texId) {
		return cacheTexNames[texId];
	}
}