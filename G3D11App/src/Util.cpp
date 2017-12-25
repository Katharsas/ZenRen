#include "stdafx.h"
#include "Util.h"

#include <codecvt>

namespace util {
	
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

	const std::wstring utf8ToWide(const std::string& string) {
		return converter.from_bytes(string);
	}
}
