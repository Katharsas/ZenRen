#include "stdafx.h"
#include "AssetFinder.h"

#include "DebugTextures.h"
#include "Util.h"

#include "zenkit/Logger.hh"
#include "magic_enum.hpp"


namespace assets
{
	namespace fs = std::filesystem;
	using render::FileData;
	using std::byte;
	using std::unordered_map;
	using std::unordered_set;
	using std::vector;
	using std::string;
	using std::string_view;

	// name is lowercase
	unordered_map<string, const fs::path> assetNamesToPaths;
	unordered_map<string, bool> zensFound;

	unordered_set<AssetsIntern> assetsIntern = {
		AssetsIntern::DEFAULT_TEXTURE,
	};
	unordered_map<AssetsIntern, FileHandle> assetsInternHandles;
	unordered_map<AssetsIntern, const fs::path> assetsInternPaths;

	const std::string DEFAULT_TEXTURE = "default_texture.png";
	const fs::path DEFAULT_TEXTURE_PATH = fs::path("./" + DEFAULT_TEXTURE);

	struct Vfs {
		zenkit::Vfs* zkit = nullptr;

		bool initialized() const
		{
			return zkit != nullptr;
		}
	} vfs;


	void walkFilesRecursively(fs::path& rootDir, const std::function<void(const fs::path&, const string&)> onFile)
	{
		for (const auto& dirEntry : fs::recursive_directory_iterator(rootDir)) {
			if (!std::filesystem::is_directory(dirEntry)) {
				const auto& path = dirEntry.path();
				auto filename = util::toString(path.filename());
				util::asciiToLowerMut(filename);

				onFile(path, filename);
			}
		}
	}

	void walkVfsNodesRecursively(const zenkit::VfsNode& parent, const std::function<void(const zenkit::VfsNode&)> onFile)
	{
		for (const auto& node : parent.children()) {
			if (node.type() == zenkit::VfsNodeType::DIRECTORY) {
				walkVfsNodesRecursively(node, onFile);
			}
			else {
				onFile(node);
			}
		}
	}

	FileHandle getInternal(const AssetsIntern asset)
	{
		auto it = assetsInternHandles.find(asset);
		assert(it != assetsInternHandles.end());
		return it->second;
	}

	std::optional<FileHandle> getIfExistsAsFile(const string_view assetNameLower)
	{
		// for tex we want to first look for -c suffix
		if (util::endsWith(assetNameLower, ".tex") && !util::endsWith(assetNameLower, "-c.tex")) {
			string assetNameNoExtLower = util::replaceExtension(assetNameLower, "");
			auto result = getIfExistsAsFile(assetNameNoExtLower + "-c.tex");
			if (result.has_value()) {
				return result;
			}
		}

		auto assetNameStr = string(assetNameLower);
		auto it = assetNamesToPaths.find(assetNameStr);
		if (it != assetNamesToPaths.end()) {
			return FileHandle{
				.name = std::move(assetNameStr),
				.path = &(it->second),
				.node = nullptr,
			};
		}
		else {
			return std::nullopt;
		}
	}

	std::optional<FileHandle> getIfExistsInVfs(const string_view assetNameLower)
	{
		assert(vfs.initialized());

		// for tex we want to first look for -c suffix
		if (util::endsWith(assetNameLower, ".tex") && !util::endsWith(assetNameLower, "-c.tex")) {
			string assetNameNoExtLower = util::replaceExtension(assetNameLower, "");
			auto result = getIfExistsInVfs(assetNameNoExtLower + "-c.tex");
			if (result.has_value()) {
				return result;
			}
		}

		const zenkit::VfsNode* node = vfs.zkit->find(assetNameLower);
		if (node != nullptr) {
			return FileHandle{
				.name = std::string(assetNameLower),
				.path = nullptr,
				.node = node,
			};
		}
		else {
			return std::nullopt;
		}
	}

	std::optional<FileHandle> getIfExists(const string_view assetNameLower)
	{
		auto handle = getIfExistsAsFile(assetNameLower);
		if (handle.has_value()) {
			return handle;
		}
		return getIfExistsInVfs(assetNameLower);
	}

	std::optional<std::pair<FileHandle, util::FileExt>> getIfAnyExists(const string_view assetNameAnyCase, const AssetFormats& formats)
	{
		// remove extension if any
		string assetNameNoExtLower = util::replaceExtension(assetNameAnyCase, "");
		util::asciiToLowerMut(assetNameNoExtLower);

		for (auto& ext : formats.formatsFile) {
			auto handle = getIfExistsAsFile(assetNameNoExtLower + ext.extLower);
			if (handle.has_value()) {
				return std::pair { handle.value(), ext };
			}
		}
		for (auto& ext : formats.formatsVfs) {
			auto handle = getIfExistsInVfs(assetNameNoExtLower + ext.extLower);
			if (handle.has_value()) {
				return std::pair{ handle.value(), ext };
			}
		}
		return std::nullopt;
	}

	bool exists(const string_view assetName)
	{
		return getIfExists(assetName).has_value();
	}

	std::unique_ptr<zenkit::Read> getDataStream(FileHandle handle)
	{
		if (handle.path != nullptr) {
			return zenkit::Read::from(*handle.path);
		}
		if (handle.node != nullptr) {
			return handle.node->open_read();
		}
	}

	const FileData getData(const FileHandle handle)
	{
		if (handle.path != nullptr) {
			auto mmap = zenkit::Mmap(*handle.path);
			return FileData(handle.name, mmap.data(), mmap.size(), std::move(mmap));
		}
		if (handle.node != nullptr) {
			phoenix::buffer buffer_view = handle.node->open();
			return FileData(handle.name, buffer_view.array(), buffer_view.limit());
		}
	}

	void initAssetsIntern()
	{
		for (auto& enumVal : assetsIntern) {
			std:string assetFileName = util::asciiToLower(magic_enum::enum_name(enumVal)) + ".png";
			auto [ it, wasInserted ] = assetsInternPaths.insert({ enumVal , fs::path("./" + assetFileName) });
			assetsInternHandles.insert({ enumVal, FileHandle {
					.name = std::move(assetFileName),
					.path = &(it->second),
					.node = nullptr,
				}
			});
		}
	}

	void initFileAssetSourceDir(fs::path& rootDir)
	{
		assetNamesToPaths.clear();

		LOG(INFO) << "Scanning dir for asset files: " << util::toString(rootDir);
		std::initializer_list<util::FileExt> extensions = {
			FormatsSource::TGA,
			FormatsSource::PNG,
			FormatsCompiled::ZEN,
			FormatsCompiled::TEX,
			FormatsCompiled::MRM,
			FormatsCompiled::MDM,
			FormatsCompiled::MDH,
			FormatsCompiled::MDL,
			FormatsCompiled::MAN,
		};

		walkFilesRecursively(rootDir, [&](const fs::path& path, const string& filenameLower) -> void {
			if (util::endsWithEither(filenameLower, extensions)) {
				if (util::endsWith(filenameLower, ".zen")) {
					auto [nameAndFoundInVfs, wasInserted] = zensFound.try_emplace(filenameLower);
					nameAndFoundInVfs->second = false;// overwrite ZENs from VFS
				}
				assetNamesToPaths.insert(std::pair(filenameLower, path));
			}
		});

		assets::initDebugTextures(&assetNamesToPaths);
	}

	void initVdfAssetSourceDir(fs::path& rootDir)
	{
		LOG(INFO) << "Scanning dir for VDF files: " << util::toString(rootDir);
		
		zenkit::Logger::use_logger([&](zenkit::LogLevel logLevel, std::string const& message) -> void {
			switch (logLevel) {
			case zenkit::LogLevel::TRACE: LOG(DEBUG) << "zenkit [TRACE]: " << message; break;
			case zenkit::LogLevel::DEBUG: LOG(DEBUG) << "zenkit: " << message; break;
			case zenkit::LogLevel::INFO: LOG(INFO) << "zenkit: " << message; break;
			case zenkit::LogLevel::WARNING: LOG(WARNING) << "zenkit: " << message; break;
			case zenkit::LogLevel::ERROR: LOG(FATAL) << "zenkit: " << message; break;
			}
		});

		if (vfs.zkit != nullptr) {
			delete vfs.zkit;
		}
		vfs.zkit = new zenkit::Vfs();
		zenkit::VfsOverwriteBehavior overwrite = zenkit::VfsOverwriteBehavior::NONE;
			
		walkFilesRecursively(rootDir, [&](const fs::path& path, const std::string& filename) -> void {
			try {
				if (util::endsWith(filename, ".vdf")) {
					vfs.zkit->mount_disk(path, overwrite);
					LOG(DEBUG) << "Loaded VDF: " << filename;
				}
				if (util::endsWith(filename, ".mod")) {
					vfs.zkit->mount_disk(path, overwrite);
					LOG(DEBUG) << "Loaded MOD: " << filename;
				}
			}
			catch (...) {
				LOG(WARNING) << "Skipped unsupported VDF: " << filename;
			}
		});

		walkVfsNodesRecursively(vfs.zkit->root(), [](const zenkit::VfsNode& fileNode) -> void {
			const auto filenameLower = util::asciiToLower(fileNode.name());
			if (util::endsWith(filenameLower, ".zen")) {
				auto [nameAndFoundInVfs, wasInserted] = zensFound.try_emplace(filenameLower);
				if (wasInserted) { // don't overwrite ZENs from files
					nameAndFoundInVfs->second = true;
				}
			}
		});
	}

	void cleanAssetSources()
	{
		if (vfs.zkit != nullptr) {
			delete vfs.zkit;
			vfs.zkit = nullptr;
		}
		// TODO
	}

	void printFoundZens()
	{
		if (zensFound.empty()) {
			LOG(WARNING) << "    No level files found!";
		}
		for (auto& [filename, foundInVfs] : zensFound) {
			LOG(INFO) <<  "    " << (foundInVfs ? "VFS" : "FILE") << ": " << filename;
		}
	}
}