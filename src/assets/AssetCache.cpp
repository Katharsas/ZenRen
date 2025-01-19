#include "stdafx.h"
#include "AssetCache.h"

#include "Util.h"

#include <string>
#include <unordered_map>

namespace assets
{
	using namespace DirectX;
	using namespace zenkit;
	using ::render::TexId;
	using ::std::string;
	using ::std::vector;
	using ::std::unordered_map;
	using ::util::createOrGet;

	unordered_map<string, MultiResolutionMesh> cacheMeshesZkit;
	unordered_map<string, ModelHierarchy> cacheMdh;
	unordered_map<string, ModelMesh> cacheModelMeshesZkit;
	unordered_map<string, Model> cacheModelsZkit;

	vector<string> cacheTexNames;
	unordered_map<int32_t, TexId> cacheTexNameHashToIds;

	// TODO T must have load method
	template<typename T>
	bool loadVisualZkit(T& parseTarget, const string& assetName)
	{
		//LOG(DEBUG) << "Loading: " << assetName;
		const auto& assetFileOpt = assets::getIfExists(assetName);
		if (assetFileOpt.has_value()) {
			auto visualData = assets::getData(assetFileOpt.value());
			auto read = zenkit::Read::from(visualData.data, visualData.size);
			parseTarget.load(read.get());
			return true;
		}
		return false;
	}

	std::optional<const MultiResolutionMesh*> getOrParseMrm(const string& assetName)
	{
		auto [it, wasInserted] = cacheMeshesZkit.try_emplace(assetName);
		MultiResolutionMesh& result = it->second;
		if (wasInserted) {
			if (!loadVisualZkit(result, assetName)) {
				return std::nullopt;
			}
		}
		return &result;
	}

	std::optional<const ModelHierarchy*> getOrParseMdh(const string& assetName)
	{
		auto [it, wasInserted] = cacheMdh.try_emplace(assetName);
		ModelHierarchy& result = it->second;
		if (wasInserted) {
			if (!loadVisualZkit(result, assetName)) {
				return std::nullopt;
			}
		}
		return &result;
	}

	std::optional<const ModelMesh*> getOrParseMdm(const string& assetName)
	{
		auto [it, wasInserted] = cacheModelMeshesZkit.try_emplace(assetName);
		ModelMesh& result = it->second;
		if (wasInserted) {
			if (!loadVisualZkit(result, assetName)) {
				return std::nullopt;
			}
		}
		return &result;
	}

	std::optional<const Model*> getOrParseMdl(const string& assetName)
	{
		auto [it, wasInserted] = cacheModelsZkit.try_emplace(assetName);
		Model& result = it->second;
		if (wasInserted) {
			if (!loadVisualZkit(result, assetName)) {
				return std::nullopt;
			}
		}
		return &result;
	}

	// TODO this should probably also lowercase everything, not callers
	TexId getTexId(const std::string& texName) {
		std::string copy = texName;
		std::hash<string> hasher;
		int32_t hash = hasher(copy);
		return createOrGet<int32_t, TexId>(cacheTexNameHashToIds, hash, [&](TexId& ref) {
			TexId currentIndex = (TexId) cacheTexNames.size();
			cacheTexNames.push_back(texName);
			ref = currentIndex;
		});
	}

	std::string_view getTexName(TexId texId) {
		return cacheTexNames[texId];
	}
}