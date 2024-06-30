#pragma once
#include "Util.h"

#include <filesystem>

namespace viewer
{
	const std::string ARG_NO_LOG = "--noLog";
	const std::string ARG_LEVEL = "--level";
	const std::string ARG_ASSET_DIR = "--assetDir";
	const std::string ARG_VDF_DIR = "--vdfDir";

	// If false: flag, If true: single value option
	const std::unordered_map<std::string, bool> options = {
		{ util::asciiToLower(ARG_NO_LOG), false },
		{ util::asciiToLower(ARG_LEVEL), true },
		{ util::asciiToLower(ARG_ASSET_DIR), true },
		{ util::asciiToLower(ARG_VDF_DIR), true },
	};

	struct Arguments {
		std::optional<std::filesystem::path> vdfFilesRoot;
		std::optional<std::filesystem::path> assetFilesRoot;
		std::optional<std::string> level;
	};

	std::unordered_map<std::string, std::string> parseOptions(const std::vector<std::string> args, const std::unordered_map<std::string, bool> options);

	void getOptionFlag(const std::string& option, bool* target, const std::unordered_map<std::string, std::string>& optionsToValues);
	void getOptionString(const std::string& option, std::optional<std::string>* target, const std::unordered_map<std::string, std::string>& optionsToValues);
	void getOptionPath(const std::string& option, std::optional<std::filesystem::path>* target, const std::unordered_map<std::string, std::string>& optionsToValues);
}

