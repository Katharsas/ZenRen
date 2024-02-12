#pragma once
#include "Util.h"

#include <filesystem>

namespace game
{
	const std::string ARG_LEVEL = "--level";
	const std::string ARG_ASSET_DIR = "--assetDir";
	const std::string ARG_VDF_DIR = "--vdfDir";

	const std::unordered_set<std::string> options = {
		util::asciiToLower(ARG_LEVEL),
		util::asciiToLower(ARG_ASSET_DIR),
		util::asciiToLower(ARG_VDF_DIR)
	};

	struct Arguments {
		std::optional<std::filesystem::path> vdfFilesRoot;
		std::optional<std::filesystem::path> assetFilesRoot;
		std::optional<std::string> level;
	};

	std::unordered_map<std::string, std::string> parseOptions(const std::vector<std::string> args, const std::unordered_set<std::string> options);
	void getOptionString(const std::string& option, std::optional<std::string>* target, const std::unordered_map<std::string, std::string> optionsToValues);
	void getOptionPath(const std::string& option, std::optional<std::filesystem::path>* target, const std::unordered_map<std::string, std::string> optionsToValues);
}

