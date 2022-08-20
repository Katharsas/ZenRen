#pragma once

namespace util {

	const std::wstring utf8ToWide(const std::string& string);
	const std::string wideToUtf8(const std::wstring& string);
	const std::string toString(XMVECTOR vector);
}


