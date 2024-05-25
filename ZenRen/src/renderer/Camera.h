#pragma once

#include "Renderer.h"

namespace renderer::camera {

	struct ObjectMatrices {
		DirectX::XMMATRIX worldView;// for transforming vertices into camera space
		DirectX::XMMATRIX worldViewNormal;// the inverse + transpose of woldView used for transforming normals into camera space
	};

	/* pre-transposed for HLSL usage */
	ObjectMatrices getWorldViewMatrix(const DirectX::XMMATRIX& objectsWorldMatrix = DirectX::XMMatrixIdentity());
	/* pre-transposed for HLSL usage */
	DirectX::XMMATRIX getProjectionMatrix();

	void init();
	DirectX::XMVECTOR getCameraPosition();
	void updateCamera(bool reverseZ, BufferSize& viewportSize);
	void moveCameraDepth(float amount);
	void moveCameraHorizontal(float amount);
	void moveCameraVertical(float amount);
	void turnCameraHorizontal(float amount);
	void turnCameraVertical(float amount);
}

