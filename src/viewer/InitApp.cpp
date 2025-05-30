﻿// TODO rename to Main.cpp

#include "stdafx.h"
#include "InitApp.h"

#include "conio.h"
#include "win_resources/resource.h"

#include "Logger.h"
#include "Util.h"
#include "Args.h"
#include "GameLoop.h"
#include "Input.h"

#include "Win.h"
#include <shellapi.h>
#include <imgui.h>


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Default client size
uint16_t windowClientWidth = 1366;
uint16_t windowClientHeight = 768;

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
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	logger::init();

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_G3D11APP, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Initialize window:
	HWND hWnd = InitInstance(hInstance, nCmdShow);
	if (!hWnd) {
		return FALSE;
	}

	// Get argv[0]
	wchar_t arg0wide[MAX_PATH];
	GetModuleFileNameW(hInstance, arg0wide, MAX_PATH);
	std::string exePath = util::wideToUtf8(arg0wide);
	//LOG(INFO) << "Arg 0: " << exePath;

	//GetCommandLineW();
	//LOG(INFO) << "Args: " << util::wideToUtf8(std::wstring(lpCmdLine));

	int argCount;
	LPWSTR* argList = CommandLineToArgvW(lpCmdLine, &argCount);
	std::vector<std::string> args;
	for (int i = 0; i < argCount; ++i) {
		args.push_back(util::wideToUtf8(std::wstring(argList[i])));
	}
	//LOG(INFO) << "Args: " << util::join(args, ",");

	auto optionsToValues = viewer::parseOptions(args, viewer::options);
	bool noLog;
	viewer::getOptionFlag(viewer::ARG_NO_LOG, &noLog, optionsToValues);

	logger::initFileSink(!noLog, "ZenRen.log.txt");

	viewer::Arguments arguments;
	viewer::getOptionFlag(viewer::ARG_RDP_COMPAT, &(arguments.rdpCompatMode), optionsToValues);
	viewer::getOptionString(viewer::ARG_LEVEL, &(arguments.level), optionsToValues);
	viewer::getOptionPath(viewer::ARG_VDF_DIR, &(arguments.vdfFilesRoot), optionsToValues);
	viewer::getOptionPath(viewer::ARG_ASSET_DIR, &(arguments.assetFilesRoot), optionsToValues);

	// Initialize
	viewer::init(hWnd, arguments, windowClientWidth, windowClientHeight);
	
	// Main message loop:
	MSG msg;
	bool quit = false;

	while (!quit) {
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT) {
				quit = true;
				break;
			}
		}
		viewer::execute();
	}

	viewer::cleanup();

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
	//wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.hbrBackground  = CreateSolidBrush(RGB(0, 0, 0));// black window background when loading
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
	RECT viewSize = {0, 0, (int32_t) windowClientWidth, (int32_t) windowClientHeight};
	AdjustWindowRect(&viewSize, WS_OVERLAPPEDWINDOW, FALSE);

	LEVELS level = DEBUG;
	LOG(level) << "Window Size:";
	LOG(level) << "    Width: " + std::to_string(viewSize.right - viewSize.left);
	LOG(level) << "    Height: " + std::to_string(viewSize.bottom - viewSize.top);

	HWND hWnd = CreateWindowExW(0L, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0,
		viewSize.right - viewSize.left,
		viewSize.bottom - viewSize.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) {
		DWORD errorMessageID = ::GetLastError();
		LOG(INFO) << "CreateWindowExW Error-ID: " << errorMessageID;
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
	static bool in_sizemove = false;
	static bool minimized = false;

	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
		return 0;// TODO is this correct check?
	}

	if (viewer::input::onWindowMessage(message, wParam, lParam)) {
		return 0;
	}

	switch (message)
	{
	case WM_SIZE:
		{
			windowClientWidth = LOWORD(lParam);
			windowClientHeight = HIWORD(lParam);

			minimized = wParam == SIZE_MINIMIZED;
			if (!minimized && !in_sizemove) {
				viewer::onWindowResized(windowClientWidth, windowClientHeight);
			}
		}
		break;
	case WM_ENTERSIZEMOVE:
		{
			in_sizemove = true;
		}
		break;
	case WM_EXITSIZEMOVE:
		{
			in_sizemove = false;
			viewer::onWindowResized(windowClientWidth, windowClientHeight);
		}
		break;
	case WM_GETMINMAXINFO:
		{
			// prevent the window from being resized too small
			auto info = reinterpret_cast<MINMAXINFO*>(lParam);
			info->ptMinTrackSize.x = 320;
			info->ptMinTrackSize.y = 200;
		}
		break;
	//case WM_PAINT:
	//	{
			//PAINTSTRUCT ps;
			//HDC hdc = BeginPaint(hWnd, &ps);
			//// TODO: Add any drawing code that uses hdc here...
			//EndPaint(hWnd, &ps);
	//	}
	//	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_DPICHANGED:
		{
			WORD upperWord = (uint16_t)(wParam >> 16);
			WORD lowerWord = (uint16_t)wParam;
			assert(upperWord == lowerWord);
			float dpiScale = upperWord / 96.f;
			viewer::onWindowDpiChanged(dpiScale);
		}
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}
