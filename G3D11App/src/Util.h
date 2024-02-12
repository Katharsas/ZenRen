#pragma once

#include <string_view>
#include <filesystem>

namespace util {

	const std::wstring utf8ToWide(const std::string& string);
	const std::string wideToUtf8(const std::wstring& string);
	const std::string toString(DirectX::XMVECTOR vector);

	bool endsWith(std::string_view str, std::string_view suffix);
	bool startsWith(std::string_view str, std::string_view prefix);
	void asciiToLower(std::string& string);
	std::string leftPad(const std::string& string, const uint32_t count, const char paddingChar = ' ');
	std::string join(const std::vector<std::string> strings, const std::string& delimiter);
	std::string fromU8(const std::u8string& u8string);
	std::string toString(const std::filesystem::path& path);

	std::string getUserFolderPath();
	bool warnOnError(const HRESULT& hr, const std::string& message);


	template<typename Key, typename Item, typename ItemAlloc>
	std::vector<Item>& getOrCreate(std::unordered_map<Key, std::vector<Item, ItemAlloc>>& map, const Key& key)
	{
		auto it = map.find(key);
		if (it == map.end()) {
			std::vector<Item, ItemAlloc> items;
			const auto [insertedIt, __] = map.insert(std::make_pair(key, items));
			return insertedIt->second;
		}
		else {
			return it->second;
		}
	}

	template<typename Key, typename Item, typename ItemAlloc>
	std::list<Item>& getOrCreate(std::map<Key, std::list<Item, ItemAlloc>>& map, const Key& key)
	{
		auto it = map.find(key);
		if (it == map.end()) {
			std::list<Item, ItemAlloc> items;
			const auto [insertedIt, __] = map.insert(std::make_pair(key, items));
			return insertedIt->second;
		}
		else {
			return it->second;
		}
	}
}
