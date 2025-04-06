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

	unordered_map<string, MultiResolutionMesh> cacheMrm;
	unordered_map<string, ModelHierarchy> cacheMdh;
	unordered_map<string, ModelMesh> cacheMdm;
	unordered_map<string, Model> cacheMdl;

	vector<string> cacheTexNames;
	unordered_map<int32_t, TexId> cacheTexNameHashToIds;


	template<HAS_LOAD T>
	unordered_map<string, T>& getCache() {
		if constexpr (std::is_same_v<T, MultiResolutionMesh>) {
			return cacheMrm;
		}
		if constexpr (std::is_same_v<T, ModelHierarchy>) {
			return cacheMdh;
		}
		if constexpr (std::is_same_v<T, ModelMesh>) {
			return cacheMdm;
		}
		if constexpr (std::is_same_v<T, Model>) {
			return cacheMdl;
		}
	}

	template<HAS_LOAD T>
	std::optional<const T*> getOrParse(const string& assetName)
	{
		auto& cache = getCache<T>();
		auto it = cache.find(assetName);
		if (it == cache.end()) {
			const auto& assetFileOpt = assets::getIfExists(assetName);
			if (assetFileOpt.has_value()) {
				auto [itNew, wasInserted] = cache.try_emplace(assetName);
				auto visualData = assets::getData(assetFileOpt.value());
				auto read = zenkit::Read::from(visualData.data, visualData.size);
				itNew->second.load(read.get());
				return &itNew->second;
			}
			else {
				return std::nullopt;
			}
		}
		else {
			return &it->second;
		}
	}

	template std::optional<const MultiResolutionMesh*> getOrParse(const string& assetName);
	template std::optional<const ModelHierarchy*> getOrParse(const string& assetName);
	template std::optional<const ModelMesh*> getOrParse(const string& assetName);
	template std::optional<const Model*> getOrParse(const string& assetName);


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