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

#include <dxgi1_4.h>// This is only needed to test for hardware/software capability, otherwise it is not used
#include <dxgi1_2.h>

#include <windowsx.h>

#include <d3d11_1.h>
#include <d3d11.h>
#include <d3dx11.h>

//#include <d3d10_1.h>
#include <d3dx10.h>

// include the Direct3D Library file
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")


struct D3d {
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
};