#include "stdafx.h"
#include "Win.h"

#include <comdef.h>
#include <shlobj_core.h>

#include "Util.h"

namespace util
{
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

	std::string getUserFolderPath()
	{
		wchar_t* out;
		SHGetKnownFolderPath(FOLDERID_Profile, 0, 0, &out);
		std::wstring wide = out;
		CoTaskMemFree(out);
		return wideToUtf8(wide);
	}
}