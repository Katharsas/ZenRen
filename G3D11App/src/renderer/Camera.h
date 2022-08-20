#pragma once

namespace renderer::camera {
	XMMATRIX calculateWorldViewProjection(const XMMATRIX & objectsWorldMatrix);
	void init();
	void updateCamera(bool reverseZ);
	void moveCameraDepth(float amount);
	void moveCameraHorizontal(float amount);
	void moveCameraVertical(float amount);
	void turnCameraHorizontal(float amount);
}

