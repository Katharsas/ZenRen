#include "stdafx.h"
#include "D3D11Renderer.h"

// Directx 11 Renderer according to this tutorial:
// http://www.directxtutorial.com/Lesson.aspx?lessonid=11-4-2

#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>

// include the Direct3D Library file
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")

// global declarations
IDXGISwapChain *swapchain;             // the pointer to the swap chain interface
ID3D11Device *device;                  // the pointer to our Direct3D device interface
ID3D11DeviceContext *context;          // the pointer to our Direct3D device context
ID3D11RenderTargetView *backbuffer;

ID3D11Buffer *pVertexBuffer;
ID3D11VertexShader *pVS;    // the vertex shader
ID3D11PixelShader *pPS;     // the pixel shader

ID3D11InputLayout *pLayout;

struct VERTEX {            // a struct to define a vertex
	FLOAT X, Y, Z;
	D3DXCOLOR Color;
};

void InitPipeline();
void InitGraphics();

void InitD3D(HWND hWnd)
{
	// Direct3D initialization
	// #########################

	// create a struct to hold information about the swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc;

	// clear out the struct for use
	ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

	// fill the swap chain description struct
	swapChainDesc.BufferCount = 1;                                    // one back buffer
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	swapChainDesc.BufferDesc.Width = SCREEN_WIDTH;                    // set the back buffer width
	swapChainDesc.BufferDesc.Height = SCREEN_HEIGHT;                  // set the back buffer height
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	swapChainDesc.OutputWindow = hWnd;                                // the window to be used
	swapChainDesc.SampleDesc.Count = 4;                               // how many multisamples
	swapChainDesc.Windowed = TRUE;                                    // windowed/full-screen mode
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;     // allow full-screen switching

	// create a device, device context and swap chain using the information in the scd struct
	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		0,
		D3D11_CREATE_DEVICE_DEBUG,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&swapchain,
		&device,
		nullptr,
		&context);

	// Set the render target
	// #########################

	// get the address of the back buffer
	ID3D11Texture2D *pBackBuffer;
	swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	// use the back buffer address to create the render target
	device->CreateRenderTargetView(pBackBuffer, nullptr, &backbuffer);
	pBackBuffer->Release();

	// set the render target as the back buffer
	context->OMSetRenderTargets(1, &backbuffer, nullptr);

	// // Set the viewport
	// #########################

	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = SCREEN_WIDTH;
	viewport.Height = SCREEN_HEIGHT;

	context->RSSetViewports(1, &viewport);

	InitPipeline();
	InitGraphics();
}

void CleanD3D()
{
	swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

	// close and release all existing COM objects
	pLayout->Release();
	pVS->Release();
	pPS->Release();
	pVertexBuffer->Release();
	swapchain->Release();
	backbuffer->Release();
	device->Release();
	context->Release();
}

void RenderFrame(void)
{
	// clear the back buffer to a deep blue
	context->ClearRenderTargetView(backbuffer, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

	// do 3D rendering on the back buffer here
	{
		// select which vertex buffer to display
		UINT stride = sizeof(VERTEX);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);

		// select which primtive type we are using
		context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// draw the vertex buffer to the back buffer
		context->Draw(3, 0);
	}

	// switch the back buffer and the front buffer
	swapchain->Present(0, 0);
}

void InitPipeline()
{
	// load and compile the two shaders
	ID3D10Blob *VS, *PS;
	D3DX11CompileFromFileW(L"Shaders/shaders.hlsl", 0, 0, "VShader", "vs_4_0", 0, 0, 0, &VS, 0, 0);
	D3DX11CompileFromFileW(L"Shaders/shaders.hlsl", 0, 0, "PShader", "ps_4_0", 0, 0, 0, &PS, 0, 0);

	// encapsulate both shaders into shader objects
	device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &pVS);// TODO VS is nullptr
	device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &pPS);

	// set the shader objects avtive
	context->VSSetShader(pVS, 0, 0);
	context->PSSetShader(pPS, 0, 0);

	// create the input layout object
	D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	device->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &pLayout);
	context->IASetInputLayout(pLayout);
}

void InitGraphics()
{
	// create a triangle using the VERTEX struct
	VERTEX OurVertices[] = {
		{ 0.0f, 0.5f, 0.0f, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
		{ 0.45f, -0.5, 0.0f, D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
		{ -0.45f, -0.5f, 0.0f, D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f) }
	};

	// create the vertex buffer
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	bd.Usage = D3D11_USAGE_DYNAMIC;                // write access access by CPU and GPU
	bd.ByteWidth = sizeof(VERTEX) * 3;             // size is the VERTEX struct * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // use as a vertex buffer
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // allow CPU to write in buffer

	device->CreateBuffer(&bd, nullptr, &pVertexBuffer); // create the buffer

	// copy the vertices into the buffer
	D3D11_MAPPED_SUBRESOURCE ms;
	context->Map(pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);   // map the buffer
	memcpy(ms.pData, OurVertices, sizeof(OurVertices));                // copy the data
	context->Unmap(pVertexBuffer, 0);                                  // unmap the buffer
}