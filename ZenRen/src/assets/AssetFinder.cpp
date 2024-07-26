#include "stdafx.h"
#include "AssetFinder.h"

#include "DebugTextures.h"
#include "../Util.h"

namespace assets
{
	namespace fs = std::filesystem;
	using namespace ::ZenLib;
	using std::string;
	using std::string_view;

	/*
	struct path_hash {
		std::size_t operator()(const fs::path& path) const {
			return hash_value(path);
		}
	};
	*/

	std::unordered_map<string, const fs::path> assetNamesToPaths;
	std::unordered_set<string> zensFoundInVdfs;
	VDFS::FileIndex* vdf = nullptr;

	string filenameWithTga(const string& filename) {
		return util::replaceExtension(filename, ".tga");
	}

	void walkFilesRecursively(fs::path& rootDir, const std::function<void(const fs::path&, const string&)> onFile) {
		for (const auto& dirEntry : fs::recursive_directory_iterator(rootDir)) {
			if (!std::filesystem::is_directory(dirEntry)) {
				const auto& path = dirEntry.path();
				auto filename = util::toString(path.filename());
				util::asciiToLowerMut(filename);

				onFile(path, filename);
			}
		}
	}

	std::optional<VDFS::FileIndex*> getVdfIndex() {
		if (vdf != nullptr) {
			return vdf;
		}
		else {
			return std::nullopt;
		}
	}

	bool existsInVdf(VDFS::FileIndex* vdf, string& assetName) {
		return vdf->hasFile(assetName);
	}

	std::optional<const fs::path*> existsAsFile(const string_view assetName) {
		auto it = assetNamesToPaths.find(string(assetName));
		if (it != assetNamesToPaths.end()) {
			return &(it->second);
		}
		else {
			return std::nullopt;
		}
	}

	void initFileAssetSourceDir(fs::path& rootDir)
	{
		assetNamesToPaths.clear();
		LOG(INFO) << "Scanning dir for asset files: " << util::toString(rootDir);

		walkFilesRecursively(rootDir, [](const fs::path& path, const string& filename) -> void {
			// textures
			if (util::endsWith(filename, ".dds")) {
				assetNamesToPaths.insert(std::pair(filenameWithTga(filename), path));
			}
			if (util::endsWith(filename, ".png")) {
				assetNamesToPaths.insert(std::pair(filenameWithTga(filename), path));
			}
			if (util::endsWith(filename, ".tga")) {
				assetNamesToPaths.insert(std::pair(filename, path));
			}

			// meshes
			if (util::endsWith(filename, ".3ds")) {
				assetNamesToPaths.insert(std::pair(filename, path));
			}
		});

		assets::initDebugTextures(&assetNamesToPaths);
	}

	void initVdfAssetSourceDir(fs::path& rootDir) {

		VDFS::FileIndex::initVDFS("Foo");
		if (vdf != nullptr) {
			delete vdf;
		}
		vdf = new VDFS::FileIndex();
		LOG(INFO) << "Scanning dir for VDF files: " << util::toString(rootDir);

		walkFilesRecursively(rootDir, [](const fs::path& path, const std::string& filename) -> void {
			if (util::endsWith(filename, ".vdf")) {
				LOG(DEBUG) << "Loading VDF: " << filename;
				vdf->loadVDF(path.string());
			}
			if (util::endsWith(filename, ".mod")) {
				LOG(DEBUG) << "Loading MOD: " << filename;
				vdf->loadVDF(path.string());
			}
		});

		vdf->finalizeLoad();

		std::vector<string> vdfFileList = vdf->getKnownFiles();
		for (auto& filename : vdfFileList) {
			util::asciiToLowerMut(filename);
			if (util::endsWith(filename, ".zen")) {
				zensFoundInVdfs.insert(filename);
			}
		}
	}
}