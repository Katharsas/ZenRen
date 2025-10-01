// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX						// Disable Windows min/max Macros overriding std::min/max

#include "targetver.h"

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
#include <algorithm>

// commonly used libraries
#include <g3log/g3log.hpp>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>