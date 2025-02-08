#pragma once

// Visual Studio setup according to:
// https://stackoverflow.com/questions/46755321/vs-2017-wont-find-directx-include-files
// 
// "To use the legacy DirectX SDK with VS 2012 or later, you need use the reverse the path order--list the DirectX SDK folders
// after the standard ones--because Windows 8.x / Windows 10 SDK contains newer headers than the legacy DirectX SDK."
//
// VC++ Directories (x86):
// $(ExecutablePath);$(DXSDK_DIR)Utilities\bin\x86
// $(IncludePath); $(DXSDK_DIR)Include
// $(LibraryPath); $(DXSDK_DIR)Lib\x86
// 
// VC++ Directories (x64):
// $(ExecutablePath);$(DXSDK_DIR)Utilities\bin\x64;$(DXSDK_DIR)Utilities\bin\x86
// $(IncludePath); $(DXSDK_DIR)Include
// $(LibraryPath); $(DXSDK_DIR)Lib\x64

#include "Win.h"
#include "Dx.h"

#include <dxgi1_4.h>// This is only needed to test for hardware/software capability, otherwise it is not used
#include <dxgi1_2.h>

//#include <windowsx.h>//TODO why was this here, and why in the middle, and why the "x" variant?

#include <d3d11_1.h>
#include <d3d11.h>
//#include <d3dx11.h>

namespace render {

	void release(IUnknown *const dx11object);
	void release(const std::vector<IUnknown*>& dx11objects);

	template<typename T>
	void release(T* const dx11object)
	{
		// we could also do "template<typename T> requires std::is_base_of<IUnknown, T>::value",
		// but this should give us better errors since the Dx.h declaration accepts objects of any type.
		static_assert(std::is_base_of<IUnknown, T>::value);
		release((IUnknown* const)dx11object);
	}
}
