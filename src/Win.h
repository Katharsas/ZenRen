#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX						// Disable Windows min/max Macros overriding std::min/max

#include <windows.h>

namespace util
{
	bool warnOnError(const HRESULT& hr, const std::string& message);
	bool warnOnError(const HRESULT& hr);
	bool throwOnError(const HRESULT& hr, const std::string& message);
	bool throwOnError(const HRESULT& hr);

	std::string getUserFolderPath();
}