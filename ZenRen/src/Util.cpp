#include "stdafx.h"
#include "Util.h"

#include <iomanip>
#include <sstream>
#include <codecvt>
#include <locale>
#include <numeric>
#include <exception>

#include <shlobj_core.h>
#include <comdef.h>

namespace util {
	
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

	const std::wstring utf8ToWide(const std::string& string)
	{
		return converter.from_bytes(string);
	}

	const std::string wideToUtf8(const std::wstring& string)
	{
		return converter.to_bytes(string);
	}

	const std::string toString(DirectX::XMVECTOR vector)
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

	bool endsWith(std::string_view str, std::string_view suffix)
	{
		return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
		return true;
	}

	bool startsWith(std::string_view str, std::string_view prefix)
	{
		return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
		return true;
	}


	void asciiToLower(std::string& string) {
		for (char& c : string) {
			c = tolower(c);
		}
	}

	std::string asciiToLower(const std::string& string) {
		std::string result = string;
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

	std::string join(const std::vector<std::string> strings, const std::string& delimiter) {
		return strings.empty() ? "" : std::accumulate(
				++strings.begin(), strings.end(), *strings.begin(),
				[](auto&& a, auto&& b) -> auto& { a += ','; a += b; return a; }
		);
	}

	std::string fromU8(const std::u8string& u8string) {
		return std::string(u8string.cbegin(), u8string.cend());
	}

	std::string toString(const std::filesystem::path& path) {
		return fromU8(path.u8string());
	}

	std::string getUserFolderPath()
	{
		wchar_t* out;
		SHGetKnownFolderPath(FOLDERID_Profile, 0, 0, &out);
		std::wstring wide = out;
		CoTaskMemFree(out);
		return wideToUtf8(wide);
	}

	std::string replaceExtension(const std::string& filename, const std::string& extension) {
		size_t lastdot = filename.find_last_of(".");
		if (lastdot == std::string::npos) {
			return filename + extension;
		}
		return filename.substr(0, lastdot) + extension;
	}

	bool handleHr(const HRESULT& hr, const std::string& message, bool throwOnError) {
		bool failed = FAILED(hr);
		if (failed) {
			_com_error err(hr);
			std::wstring errorMessageW = std::wstring(err.ErrorMessage());
			std::string errorMessage = util::wideToUtf8(errorMessageW);

			LOG(WARNING) << message;
			LOG(WARNING) << errorMessage;

			if (throwOnError) {
				throw std::exception((message + "\n" + errorMessage).c_str());
			}
		}
		return !failed;
	}

	bool warnOnError(const HRESULT& hr, const std::string& message) {
		return handleHr(hr, message, false);
	}

	bool throwOnError(const HRESULT& hr, const std::string& message) {
		return handleHr(hr, message, true);
	}

	void throwError(const std::string& message) {
		LOG(WARNING) << message;
		throw std::exception((message).c_str());
	}
}
