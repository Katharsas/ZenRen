#pragma once

#pragma warning(push)
#pragma warning(disable : 4838)
#include <xnamath.h>
#pragma warning(pop)

namespace renderer::camera {
	XMMATRIX calculateWorldViewProjection(const XMMATRIX & objectsWorldMatrix);
	void init();
	void updateCamera(bool reverseZ);
	void moveCameraDepth(float amount);
	void moveCameraHorizontal(float amount);
	void moveCameraVertical(float amount);
	void turnCameraHorizontal(float amount);
}

