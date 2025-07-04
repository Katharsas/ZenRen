#pragma once

#include "Primitives.h"

namespace render::camera
{
	struct ObjectMatrices {
		DirectX::XMMATRIX worldView;// for transforming vertices into camera space
		DirectX::XMMATRIX worldViewNormal;// the inverse + transpose of woldView used for transforming normals into camera space
	};

	/* pre-transposed for HLSL usage */
	ObjectMatrices getWorldViewMatrix(const DirectX::XMMATRIX& objectsWorldMatrix = DirectX::XMMatrixIdentity());
	/* pre-transposed for HLSL usage */
	DirectX::XMMATRIX getProjectionMatrix();

	DirectX::BoundingFrustum getFrustum();

	void init();
	DirectX::XMVECTOR getCameraPosition();
	bool updateCamera(bool reverseZ, BufferSize& viewportSize, float viewDistance);
	void moveCameraDepth(float amount);
	void moveCameraHorizontal(float amount);
	void moveCameraVertical(float amount);
	void turnCameraHorizontal(float amount);
	void turnCameraVertical(float amount);
}

