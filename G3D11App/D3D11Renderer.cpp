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
IDXGISwapChain *swapchain;
ID3D11Device *device;
ID3D11DeviceContext *deviceContext;

ID3D11RenderTargetView *backBuffer;    // 32-bit
ID3D11RenderTargetView *backBufferHDR; // 64-bit
ID3D11ShaderResourceView *backBufferHDRResource;

ID3D11VertexShader *vs_toneMapping;
ID3D11PixelShader *ps_toneMapping;

ID3D11Buffer *vertexBuffer;
ID3D11VertexShader *vs_triangle;    // the vertex shader
ID3D11PixelShader *ps_triangle;     // the pixel shader

ID3D11InputLayout *pLayout;

struct VERTEX {            // a struct to define a vertex
	FLOAT X, Y, Z;
	D3DXCOLOR Color;
};

void initSwapChainAndBackBuffer(HWND hWnd);
void initBackBufferHDR();
void initPipelineTriangle();
void initPipelineToneMapping();
void initGraphics();


void initD3D(HWND hWnd)
{
	initSwapChainAndBackBuffer(hWnd);
	initBackBufferHDR();
	
	// // Set the viewport
	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = SCREEN_WIDTH;
	viewport.Height = SCREEN_HEIGHT;

	deviceContext->RSSetViewports(1, &viewport);

	initPipelineTriangle();
	initPipelineToneMapping();
	initGraphics();
}

void cleanD3D()
{
	swapchain->SetFullscreenState(FALSE, nullptr);    // switch to windowed mode

	// close and release all existing COM objects
	pLayout->Release();
	vs_triangle->Release();
	ps_triangle->Release();
	vertexBuffer->Release();
	swapchain->Release();
	backBuffer->Release();
	backBufferHDR->Release();
	backBufferHDRResource->Release();
	vs_toneMapping->Release();
	ps_toneMapping->Release();
	device->Release();
	deviceContext->Release();
}

void renderFrame(void)
{
	// set the rtv
	deviceContext->OMSetRenderTargets(1, &backBufferHDR, nullptr);

	// clear the back buffer to a deep blue
	deviceContext->ClearRenderTargetView(backBufferHDR, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

	// do 3D rendering here
	{
		// set the shader objects avtive
		deviceContext->VSSetShader(vs_triangle, 0, 0);
		deviceContext->PSSetShader(ps_triangle, 0, 0);

		// select which vertex buffer to display
		UINT stride = sizeof(VERTEX);
		UINT offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

		// select which primtive type we are using
		deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// draw the vertex buffer to the back buffer
		deviceContext->Draw(3, 0);
	}

	// map HDR to back buffer via tone mapping
	// TODO: set toneMappingShaders
	// TODO: create Quad as big as screen
	// TODO: render that quad with backBufferHDRResource as texture to backbuffer

	//deviceContext->VSSetShader(vs_toneMapping, 0, 0);
	//deviceContext->PSSetShader(ps_toneMapping, 0, 0);

	// switch the back buffer and the front buffer
	swapchain->Present(0, 0);
}

void initSwapChainAndBackBuffer(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

	// fill the swap chain description struct
	swapChainDesc.BufferCount = 1;                                    // one back buffer
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	swapChainDesc.BufferDesc.Width = SCREEN_WIDTH;                    // set the back buffer width
	swapChainDesc.BufferDesc.Height = SCREEN_HEIGHT;                  // set the back buffer height
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	swapChainDesc.OutputWindow = hWnd;                                // the window to be used
	swapChainDesc.SampleDesc.Count = 1;                               // how many multisamples
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
		&deviceContext);

	// get the address of the back buffer
	ID3D11Texture2D *texture;
	swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&texture);

	// use the texture address to create the render target
	device->CreateRenderTargetView(texture, nullptr, &backBuffer);
	texture->Release();
}

void initBackBufferHDR()
{
	auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	// Create buffer texture
	D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(format, SCREEN_WIDTH, SCREEN_HEIGHT);
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.MipLevels = 1;

	ID3D11Texture2D *texture;
	device->CreateTexture2D(&desc, nullptr, &texture);

	// Create RTV
	D3D11_RENDER_TARGET_VIEW_DESC descRTV = CD3D11_RENDER_TARGET_VIEW_DESC(
		D3D11_RTV_DIMENSION_TEXTURE2D,
		format
	);
	device->CreateRenderTargetView(texture, &descRTV, &backBufferHDR);

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = CD3D11_SHADER_RESOURCE_VIEW_DESC(
		D3D11_SRV_DIMENSION_TEXTURE2D,
		format
	);
	descSRV.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView((ID3D11Resource *)texture, &descSRV, &backBufferHDRResource);

	// Done
	texture->Release();
}

void initPipelineTriangle()
{
	// load and compile the two shaders
	ID3D10Blob *VS, *PS;
	D3DX11CompileFromFileW(L"Shaders/shaders.hlsl", 0, 0, "VShader", "vs_4_0", 0, 0, 0, &VS, 0, 0);
	D3DX11CompileFromFileW(L"Shaders/shaders.hlsl", 0, 0, "PShader", "ps_4_0", 0, 0, 0, &PS, 0, 0);

	// encapsulate both shaders into shader objects
	device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &vs_triangle);
	device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &ps_triangle);

	// create the input layout object
	D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	device->CreateInputLayout(inputLayoutDesc, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &pLayout);
	deviceContext->IASetInputLayout(pLayout);
}

void initPipelineToneMapping()
{
	ID3D10Blob *VS, *PS;
	D3DX11CompileFromFileW(L"Shaders/toneMapping.hlsl", 0, 0, "VS_Main", "vs_4_0", 0, 0, 0, &VS, 0, 0);
	D3DX11CompileFromFileW(L"Shaders/toneMapping.hlsl", 0, 0, "PS_Main", "ps_4_0", 0, 0, 0, &PS, 0, 0);

	device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &vs_toneMapping);
	device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &ps_toneMapping);

	// TODO layouts
}

void initGraphics()
{
	// create a triangle using the VERTEX struct
	VERTEX ourVertices[] = {
		{ 0.0f, 0.5f, 0.0f, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
		{ 0.45f, -0.5, 0.0f, D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
		{ -0.45f, -0.5f, 0.0f, D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f) }
	};

	// create the vertex buffer
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	bd.Usage = D3D11_USAGE_DYNAMIC;                // write access by CPU and GPU
	bd.ByteWidth = sizeof(VERTEX) * 3;             // size is the VERTEX struct * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // use as a vertex buffer
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // allow CPU to write in buffer

	device->CreateBuffer(&bd, nullptr, &vertexBuffer); // create the buffer

	// copy the vertices into the buffer
	D3D11_MAPPED_SUBRESOURCE ms;
	deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);   // map the buffer
	memcpy(ms.pData, ourVertices, sizeof(ourVertices));                // copy the data
	deviceContext->Unmap(vertexBuffer, 0);                                  // unmap the buffer
}