#include "stdafx.h"
#include "D3D11Renderer.h"

#include <windowsx.h>
#include <d3d11.h>
//#include <d3dx11.h>
//#include <d3dx10.h>

// include the Direct3D Library file
// TODO configure this stuff so it works
//#pragma comment (lib, "d3d11.lib")
//#pragma comment (lib, "d3dx11.lib")
//#pragma comment (lib, "d3dx10.lib")

// global declarations
IDXGISwapChain *swapchain;             // the pointer to the swap chain interface
ID3D11Device *dev;                     // the pointer to our Direct3D device interface
ID3D11DeviceContext *devcon;           // the pointer to our Direct3D device context

void InitD3D(HWND hWnd)
{
}

void CleanD3D()
{
}
