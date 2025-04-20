#include "stdafx.h"
#include "AssetCache.h"

#include "Util.h"

#include <limits>

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

	vector<string> cacheNames;
	unordered_map<int32_t, uint32_t> nameHashToId;
	vector<uint32_t> texIdToNameIndex;

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
			cacheMrm.clear();// right now we only cache at most one model per type

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


	template <typename T, vector<uint32_t>& idToName>
	T getOrCreateId(const std::string& name) {
		std::hash<string> stringHasher;
		int32_t nameHash = stringHasher(name);

		uint32_t nextId = idToName.size();
		auto [it, wasInserted] = nameHashToId.try_emplace(nameHash, nextId);
		if (wasInserted) {
			assert(nextId <= std::numeric_limits<TexId>::max());
			uint32_t nextNameIndex = cacheNames.size();
			idToName.push_back(nextNameIndex);
			cacheNames.push_back(name);
		}
		return (TexId) it->second;
	}

	// TODO this should probably also lowercase everything, not callers
	TexId getTexId(const std::string& texName) {
		return getOrCreateId<TexId, texIdToNameIndex>(texName);
	}
	std::string_view getTexName(TexId texId) {
		return cacheNames.at(texIdToNameIndex.at(texId));
	}
}