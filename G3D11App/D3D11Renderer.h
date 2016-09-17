#pragma once

#include "stdafx.h"

// define the screen resolution
#define SCREEN_WIDTH  1366
#define SCREEN_HEIGHT 768

void initD3D(HWND hWnd);
void cleanD3D();
void renderFrame(void);
