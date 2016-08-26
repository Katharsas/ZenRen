#pragma once

#include "stdafx.h"

// define the screen resolution
#define SCREEN_WIDTH  1366
#define SCREEN_HEIGHT 768

void InitD3D(HWND hWnd);
void CleanD3D();
void RenderFrame(void);
