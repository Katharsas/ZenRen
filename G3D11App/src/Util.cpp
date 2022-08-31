#include "stdafx.h"
#include "Util.h"

#include <iomanip>
#include <sstream>
#include <codecvt>
#include <locale>

#include <shlobj_core.h>

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


	void asciiToLowercase(std::string& string) {
		for (char& c : string) {
			c = tolower(c);
		}
	}

	std::string getUserFolderPath()
	{
		wchar_t* out;
		SHGetKnownFolderPath(FOLDERID_Profile, 0, 0, &out);
		std::wstring wide = out;
		CoTaskMemFree(out);
		return wideToUtf8(wide);
	}
}
