#include "stdafx.h"
#include "Util.h"
#include <codecvt>

namespace util {
	
	void println(const std::string &string) {
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		std::wstring wide = converter.from_bytes(string + "\n");
		OutputDebugStringW(wide.c_str());
	}
}
