// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: reference additional headers your program requires here
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <array>
#include <optional>

// TODO disable SSE, measure performance so we know if this is even worth it compared to normal optimization
//#define _XM_NO_INTRINSICS_ 
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "g3log/g3log.hpp"