#include "stdafx.h"
#include "Util.h"

#include <iomanip>
#include <sstream>
#include <codecvt>
#include <locale>
#include <numeric>
#include <exception>

namespace util
{
	using std::string;

	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

	const std::wstring utf8ToWide(const std::string& string)
	{
		return converter.from_bytes(string);
	}

	const std::string wideToUtf8(const std::wstring& string)
	{
		return converter.to_bytes(string);
	}

	const std::string toString(const DirectX::XMVECTOR& vector)
	{
		auto x = DirectX::XMVectorGetX(vector);
		auto y = DirectX::XMVectorGetY(vector);
		auto z = DirectX::XMVectorGetZ(vector);
		auto w = DirectX::XMVectorGetW(vector);

		std::stringstream ss;
		ss << std::fixed << std::setprecision(2)
			<< "[X: " << x << ", Y: " << y << ", Z: " << z << ", W: " << w << "]";
		std::string string = ss.str();
		return string;
	}

	bool endsWith(const std::string_view str, const std::string_view suffix)
	{
		return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
		return true;
	}

	bool startsWith(const std::string_view str, const std::string_view prefix)
	{
		return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
		return true;
	}


	void asciiToLowerMut(std::string& string)
	{
		for (char& c : string) {
			c = tolower(c);
		}
	}

	std::string asciiToLower(const std::string_view str)
	{
		std::string result = std::string(str);
		std::transform(result.begin(), result.end(), result.begin(), ::tolower);
		return result;
	}

	std::string leftPad(const std::string& string, const uint32_t count, const char paddingChar)
	{
		std::string result = string;
		if (count > result.size()) {
			result.insert(0, count - result.size(), paddingChar);
		}
		return result;
	}

	std::string join(const std::vector<std::string>& strings, const std::string& delimiter)
	{
		return strings.empty() ? "" : std::accumulate(
				++strings.begin(), strings.end(), *strings.begin(),
				[](auto&& a, auto&& b) -> auto& { a += ','; a += b; return a; }
		);
	}

	std::string fromU8(const std::u8string& u8string)
	{
		return std::string(u8string.cbegin(), u8string.cend());
	}

	std::string toString(const std::filesystem::path& path)
	{
		return fromU8(path.u8string());
	}

	bool endsWithEither(std::string_view str, const std::initializer_list<FileExt>& extensions)
	{
		for (const auto& ext : extensions) {
			if (ext.isExtOf(str)) {
				return true;
			}
		}
		return false;
	}

	std::pair<string, string> replaceExtensionAndGetOld(const std::string_view filename, const std::string_view extension)
	{
		size_t lastDot = filename.find_last_of(".");
		if (lastDot == string::npos) {
			return { string(filename) + string(extension), "" };
		}
		return {
			string(filename.substr(0, lastDot)) + string(extension),
			string(filename.substr(lastDot, filename.length() - lastDot))
		};
	}

	std::string replaceExtension(const std::string_view filename, const std::string_view extension) {
		return replaceExtensionAndGetOld(filename, extension).first;
	}

	void throwError(const std::string& message) {
		LOG(FATAL) << message;
	}
}
