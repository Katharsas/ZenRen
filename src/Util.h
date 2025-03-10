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
	bool endsWithEither(std::string_view str, const std::initializer_list<FileExt>& extensions);

	// extension must include dot
	std::pair<std::string, std::string> replaceExtensionAndGetOld(const std::string_view filename, const std::string_view extension);
	std::string replaceExtension(const std::string_view filename, const std::string_view extension);

	// the boost hash_combine function
	template <class Hashable>
	inline void hashCombine(std::size_t& hash, const Hashable& value)
	{
		std::hash<Hashable> hasher;
		hash ^= hasher(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	}

	void throwError(const std::string& message);

	template<typename Item>
	void insert(std::vector<Item>& target, const std::vector<Item>& source) {
		target.insert(target.end(), source.begin(), source.end());
	}

	template<typename Key, typename Value>
	bool hasKey(std::unordered_map<Key, Value>& map, const Key& key)
	{
		return map.find(key) != map.end();
	}

	template<typename Key, typename Value>
	Value& getOrCreateDefault(std::unordered_map<Key, Value>& map, const Key& key)
	{
		auto [it, wasInserted] = map.try_emplace(key);// first lookup, default constructor only called if not found
		return it->second;
	}

	template<typename Key, typename Value>
	Value& getOrCreateDefault(std::map<Key, Value>& map, const Key& key)
	{
		auto [it, wasInserted] = map.try_emplace(key);// first lookup, default constructor only called if not found
		return it->second;
	}

	// good when creation is more likely than already existing
	// 1 lookup
	// 1 copy, 2 if created
	// setValue can be lamda but not inlined
	template<typename Key, typename Value>
	Value& createOrGet(std::unordered_map<Key, Value>& map, const Key& key, const std::function<void(Value&)>& setValue)
	{
		static Value dummyValue;// default constructor called only once globally because static
		auto [it, wasInserted] = map.insert({ key, dummyValue });// first lookup, first copy
		auto& valueRef = it->second;
		if (wasInserted) {
			setValue(valueRef);// second copy happens inside setValue, not here
		}
		return valueRef;
	}

	// good when creation is less likely than already existing
	// 1 lookup, 2 if created
	// 0 copies, 2 if created
	// createValue can be lamda but not inlined
	template<typename Key, typename Value>
	Value& getOrCreate(std::unordered_map<Key, Value>& map, const Key& key, const std::function<Value()>& createValue)
	{
		auto it = map.find(key);// first lookup
		if (it == map.end()) {
			const Value& value = createValue();// first copy
			const auto [insertedIt, __] = map.insert({ key, value });// second lookup, second copy
			return insertedIt->second;
		}
		else {
			return it->second;
		}
	}
}
