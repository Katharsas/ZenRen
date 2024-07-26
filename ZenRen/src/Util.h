#pragma once

#include <string_view>
#include <filesystem>

namespace util
{
	const std::wstring utf8ToWide(const std::string& string);
	const std::string wideToUtf8(const std::wstring& string);
	const std::string toString(DirectX::XMVECTOR vector);

	bool endsWith(const std::string_view str, const std::string_view suffix);
	bool startsWith(const std::string_view str, const std::string_view prefix);
	void asciiToLowerMut(std::string& str);
	std::string asciiToLower(const std::string_view str);

	std::string leftPad(const std::string& string, const uint32_t count, const char paddingChar = ' ');
	std::string join(const std::vector<std::string> strings, const std::string& delimiter);
	std::string fromU8(const std::u8string& u8string);
	std::string toString(const std::filesystem::path& path);

	struct FileExt {
		std::string extUpper;
		std::string extLower;

		static FileExt create(std::string_view upper) {
			return { std::string(upper), asciiToLower(upper) };
		}
		bool isExtOf(std::string_view str) const {
			return endsWith(str, extUpper) || endsWith(str, extLower);
		}
		const std::string& str() const {
			return extUpper;
		}
	};
	bool endsWithEither(std::string_view str, std::initializer_list<FileExt> extensions);

	std::string getUserFolderPath();
	std::string replaceExtension(const std::string_view filename, const std::string_view extension);

	bool warnOnError(const HRESULT& hr, const std::string& message);
	bool throwOnError(const HRESULT& hr, const std::string& message);

	void throwError(const std::string& message);

	template<typename Item>
	void insert(std::vector<Item>& target, const std::vector<Item>& source) {
		target.insert(target.end(), source.begin(), source.end());
	}

	template<typename Key, typename Value, typename Func>
	Value& getOrSet(std::unordered_map<Key, Value>& map, const Key& key, const Func& setValue)
	{
		auto [it, wasInserted] = map.insert({ key, Value() });// first lookup, not sure if this is a copy
		auto& valueRef = it->second;
		if (wasInserted) {
			setValue(valueRef);// copy
		}
		return valueRef;
	}

	template<typename Key, typename Value>
	[[deprecated("Use getOrSet variant instead!")]]
	Value& getOrCreate(std::unordered_map<Key, Value>& map, const Key& key, std::function<Value()>& createValue)
	{
		auto it = map.find(key);// first lookup
		if (it == map.end()) {
			const Value value = createValue();// first copy
			const auto [insertedIt, __] = map.insert(std::make_pair(key, value));// second lookup, second copy
			return insertedIt->second;
		}
		else {
			return it->second;
		}
	}

	template<typename Key, typename Value>
	Value& getOrCreate(std::unordered_map<Key, Value>& map, const Key& key)
	{
		auto it = map.find(key);
		if (it == map.end()) {
			Value value;
			const auto [insertedIt, __] = map.insert(std::make_pair(key, value));
			return insertedIt->second;
		}
		else {
			return it->second;
		}
	}

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
