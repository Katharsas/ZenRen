#pragma once

#include "Renderer.h"

namespace renderer::camera {

	struct ObjectMatrices {
		XMMATRIX worldView;// for transforming vertices into camera space
		XMMATRIX worldViewNormal;// the inverse + transpose of woldView used for transforming normals into camera space
	};

	ObjectMatrices getWorldViewMatrix(const XMMATRIX& objectsWorldMatrix);
	XMMATRIX getProjectionMatrix();
	void init();
	void updateCamera(bool reverseZ, BufferSize& viewportSize);
	void moveCameraDepth(float amount);
	void moveCameraHorizontal(float amount);
	void moveCameraVertical(float amount);
	void turnCameraHorizontal(float amount);
}

