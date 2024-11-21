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

	bool useZenkit = false;
	Vfs vfs;

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

	std::optional<Vfs> getVfsIndex() {
		if (vfs.initialized()) {
			return vfs;
		}
		else {
			return std::nullopt;
		}
	}

	//bool existsInVfs(Vfs vfs, string& assetName) {
	//	if (vfs.zlib != nullptr) {
	//		return vfs.zlib->hasFile(assetName);
	//	}
	//	else {
	//		return vfs.zkit->find(assetName) != nullptr;
	//	}
	//}

	//bool existsInVfs(VDFS::FileIndex* vfs, string& assetName) {
	//	return vfs->hasFile(assetName);
	//}

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

	void initVdfAssetSourceDir(fs::path& rootDir)
	{
		LOG(INFO) << "Scanning dir for VDF files: " << util::toString(rootDir);
		{
			if (vfs.zkit != nullptr) {
				delete vfs.zkit;
			}
			vfs.zkit = new zenkit::Vfs();
			
			//walkFilesRecursively(rootDir, [](const fs::path& path, const std::string& filename) -> void {
			//	if (util::endsWith(filename, ".vdf")) {
			//		LOG(DEBUG) << "Loading VDF: " << filename;
			//		vfs.zkit->mount_disk(path);
			//	}
			//	if (util::endsWith(filename, ".mod")) {
			//		LOG(DEBUG) << "Loading MOD: " << filename;
			//		vfs.zkit->mount_disk(path);
			//	}
			//});

			//using ChildContainer = std::set<zenkit::VfsNode, zenkit::VfsNodeComparator>;

			const auto& vdfFileList = vfs.zkit->root().children();
			for (const auto& node : vdfFileList) {
				LOG(INFO) << "VFS:  " << node.name();
				//util::asciiToLowerMut(filename);
				//if (util::endsWith(filename, ".zen")) {
				//	zensFoundInVdfs.insert(filename);
				//}
			}
		}
		{
			VDFS::FileIndex::initVDFS("Foo");
			if (vfs.zlib != nullptr) {
				delete vfs.zlib;
			}
			vfs.zlib = new VDFS::FileIndex();
			

			walkFilesRecursively(rootDir, [](const fs::path& path, const std::string& filename) -> void {
				if (util::endsWith(filename, ".vdf")) {
					LOG(DEBUG) << "Loading VDF: " << filename;
					vfs.zlib->loadVDF(path.string());
				}
				if (util::endsWith(filename, ".mod")) {
					LOG(DEBUG) << "Loading MOD: " << filename;
					vfs.zlib->loadVDF(path.string());
				}
			});

			vfs.zlib->finalizeLoad();

			std::vector<string> vdfFileList = vfs.zlib->getKnownFiles();
			for (auto& filename : vdfFileList) {
				util::asciiToLowerMut(filename);
				if (util::endsWith(filename, ".zen")) {
					zensFoundInVdfs.insert(filename);
				}
			}
		}
	}
}