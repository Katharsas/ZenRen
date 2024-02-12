#include "stdafx.h"
#include "GameArgs.h"

namespace game
{
	using std::string;

	std::unordered_map<string, string> parseOptions(const std::vector<string> args, const std::unordered_map<string, bool> options) {
		std::unordered_map<string, string> result;
		for (uint32_t i = 0; i < (args.size() - 1); i++) {
			auto& arg = args[i];
			auto optionIt = options.find(util::asciiToLower(arg));
			if (optionIt != options.end()) {
				auto& value = args[i + 1];
				if (optionIt->second) {
					// not a flag, so a value following the option is expected
					if (options.contains(util::asciiToLower(value))) {
						LOG(WARNING) << "Args: Expected value after option '" << arg << "' but no value was provided!";
					}
					else {
						result.insert({ util::asciiToLower(arg), value });
						i++;
					}
				}
				else {
					result.insert({ util::asciiToLower(arg), "" });
				}
			}
			else {
				LOG(WARNING) << "Args: Skipping unknown option '" << arg << "'!";
			}
		}
		return result;
	}

	void getOptionFlag(const string& option, bool* target, const std::unordered_map<string, string> optionsToValues) {
		auto it = optionsToValues.find(util::asciiToLower(option));
		bool exists = it != optionsToValues.end();
		LOG(INFO) << "Args: '" << option << "' -> '" << (exists ? "true" : "false") << "'";
		*target = exists;
	}

	void getOptionString(const string& option, std::optional<string>* target, const std::unordered_map<string, string> optionsToValues) {
		auto it = optionsToValues.find(util::asciiToLower(option));
		if (it != optionsToValues.end()) {
			LOG(INFO) << "Args: '" << option << "' -> '" << it->second << "'";
			*target = it->second;
		}
		else {
			LOG(INFO) << "Args: No value provided for option '" << option << "'";
			*target = std::nullopt;
		}
	}

	void getOptionPath(const string& option, std::optional<std::filesystem::path>* target, const std::unordered_map<string, string> optionsToValues) {
		auto it = optionsToValues.find(util::asciiToLower(option));
		if (it != optionsToValues.end()) {
			LOG(INFO) << "Args: '" << option << "' -> '" << it->second << "'";
			*target = std::filesystem::absolute(it->second).lexically_normal();
		}
		else {
			LOG(INFO) << "Args: No value provided for option '" << option << "'";
			*target = std::nullopt;
		}
	}
}