// G3D11App.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "G3D11App.h"

#include "conio.h"
#include "resource.h"
#include "Util.h"
#include "D3D11Renderer.h"
#include "g3log/logworker.hpp"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WindowProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	InitConsole();

	// Configure Logger (make sure log dir exists, TODO: create dir if mising)
	// https://github.com/KjellKod/g3sinks/blob/master/snippets/ColorCoutSink.hpp
	auto worker = g3::LogWorker::createLogWorker();
	auto defaultSink = worker->addDefaultLogger(u8"log", u8"../logs/");
	auto consoleSink = worker->addSink(std2::make_unique<ColorCoutSink>(), &ColorCoutSink::ReceiveLogMessage);
	g3::initializeLogging(worker.get());

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_G3D11APP, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Initialize window:
	HWND hWnd = InitInstance(hInstance, nCmdShow);
	if (!hWnd) {
		return FALSE;
	}

	// Initialize Direct3D:
	initD3D(hWnd);
	
	// Main message loop:
	MSG msg;

	while (TRUE) {
		if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT) {
				break;
			}
		}
		
		renderFrame();
	}

	// Clean up DirectX and COM
	cleanD3D();

	CleanConsole();

	return (int) msg.wParam;
}

void InitConsole() {
	// Code from: http://stackoverflow.com/a/25927081/3552897

	// Create console an, set title, disable closing it (closes with main window on ClearConsole() call)
	AllocConsole();
	SetConsoleTitleW(L"Log");
	HWND hwnd = GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(hwnd, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);

	// redirect all stdio things to this console, so that logger can use cout as sink
	FILE *conin, *conout;
	freopen_s(&conin, "conin$", "r", stdin);
	freopen_s(&conout, "conout$", "w", stdout);
	freopen_s(&conout, "conout$", "w", stderr);
	std::wcout.clear();
	std::cout.clear();
	std::wcerr.clear();
	std::cerr.clear();
	std::wcin.clear();
	std::cin.clear();
}

void CleanConsole() {
	FreeConsole();
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
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	// set view size and calculate window size with borders/menu
	RECT viewSize = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
	AdjustWindowRect(&viewSize, WS_OVERLAPPEDWINDOW, FALSE);

	util::println(u8"Window Size:"); util::println(
		u8"    Width: " + std::to_string(viewSize.right - viewSize.left)
		+ u8", Height: " + std::to_string(viewSize.bottom - viewSize.top)
	);

	HWND hWnd = CreateWindowExW(0L, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0,
		viewSize.right - viewSize.left,
		viewSize.bottom - viewSize.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) {
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
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
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}