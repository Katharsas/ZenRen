// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX						// Disable Windows min/max Macros overriding std::min/max
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// commonly used std headers
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <optional>

// commonly used libraries
#include <g3log/g3log.hpp>

// TODO disable SSE, measure performance so we know if this is even worth it compared to normal optimization
//#define _XM_NO_INTRINSICS_ 
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>