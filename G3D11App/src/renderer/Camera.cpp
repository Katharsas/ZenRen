#include "stdafx.h"
#include "Camera.h"

#include "Settings.h"

namespace renderer::camera {

	struct CameraMatrices
	{
		XMMATRIX view;
		XMMATRIX projection;
	};

	struct CameraLocation
	{
		XMVECTOR position;
		XMVECTOR target;
		XMVECTOR up;
	};

	CameraMatrices matrices;
	CameraLocation location;

	/* Calculates combined World-View-Projection-Matrix for use as constant buffer value
		*/
	XMMATRIX calculateWorldViewProjection(const XMMATRIX & objectsWorldMatrix)
	{
		const XMMATRIX wvp = objectsWorldMatrix * matrices.view * matrices.projection;
		return XMMatrixTranspose(wvp);
	}

	void init(bool reverseZ) {
		location = {
			XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		};

		// Set view & projection matrix
		matrices.view = XMMatrixLookAtLH(location.position, location.target, location.up);

		const float aspectRatio = static_cast<float>(settings::SCREEN_WIDTH) / settings::SCREEN_HEIGHT;
		const float nearZ = 0.1f;
		const float farZ = 1000.0f;
		matrices.projection = XMMatrixPerspectiveFovLH(0.4f * 3.14f, aspectRatio, reverseZ ? farZ : nearZ, reverseZ ? nearZ : farZ);
	}
}