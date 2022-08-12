#pragma once

#pragma warning(push)
#pragma warning(disable : 4838)
#include <xnamath.h>
#pragma warning(pop)

namespace renderer::camera {

	XMMATRIX calculateWorldViewProjection(const XMMATRIX & objectsWorldMatrix);
	void init(bool reverseZ);
}

