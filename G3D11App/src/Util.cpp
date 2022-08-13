#include "stdafx.h"
#include "Util.h"

#include <iomanip>
#include <sstream>
#include <codecvt>

namespace util {
	
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

	const std::wstring utf8ToWide(const std::string& string) {
		return converter.from_bytes(string);
	}

	const std::string wideToUtf8(const std::wstring& string) {
		return converter.to_bytes(string);
	}

	const std::string toString(XMVECTOR vector) {
		auto x = XMVectorGetX(vector);
		auto y = XMVectorGetY(vector);
		auto z = XMVectorGetZ(vector);
		auto w = XMVectorGetW(vector);

		std::stringstream ss;
		ss << std::fixed << std::setprecision(2)
			<< "[X: " << x << ", Y: " << y << ", Z: " << z << ", W: " << w << "]";
		std::string string = ss.str();
		return string;
	}
}
