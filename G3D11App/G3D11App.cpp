// G3D11App.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "G3D11App.h"
#include <windows.h>
#include <locale>
#include <codecvt>
#include <string>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WindowProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.
	// create a "Hello World" message box using MessageBox()
	//MessageBox(NULL,
	//	L"Hello World!",
	//	L"Just another Hello World program!",
	//	MB_ICONEXCLAMATION | MB_OK);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_G3D11APP, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	MSG msg;

	// Main message loop:
	while (TRUE) {
		if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT) {
				break;
			}
		}
		else {
			// shouldnt the game code run outside of else?
		}
	}
	//while (GetMessageW(&msg, nullptr, 0, 0))
	//{
	//	TranslateMessage(&msg);
	//	DispatchMessageW(&msg);
	//}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WindowProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_G3D11APP));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_G3D11APP);
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

const wchar_t* toChar(const std::wstring &string) {
	return string.c_str();
}

std::wstring toWide(const std::string &string) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	std::wstring wide = converter.from_bytes(string);
	return wide;
}

const wchar_t* toWideChar(const std::string &string) {
	return toChar(toWide(string));
}

void println(const std::string &string) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	std::wstring wide = converter.from_bytes(string + "\n");
	OutputDebugStringW(wide.c_str());
}


//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	// set view size and calculate window size with borders/menu
	RECT viewSize = {0, 0, 1000, 800};
	AdjustWindowRect(&viewSize, WS_OVERLAPPEDWINDOW, FALSE);

	println(u8"Window Size:"); println(
		u8"    Width: " + std::to_string(viewSize.right - viewSize.left)
		+ u8", Height: " + std::to_string(viewSize.bottom - viewSize.top)
	);

	HWND hWnd = CreateWindowExW(0L, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0,
		viewSize.right - viewSize.left,
		viewSize.bottom - viewSize.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		{
			//PAINTSTRUCT ps;
			//HDC hdc = BeginPaint(hWnd, &ps);
			//// TODO: Add any drawing code that uses hdc here...
			//EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

