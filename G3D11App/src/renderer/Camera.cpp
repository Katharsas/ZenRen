#include "stdafx.h"
#include "Camera.h"

#include "Settings.h"
#include "Util.h"

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
		XMVECTOR up;// normalized
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

	void init() {
		location = {
			XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
			XMVector4Normalize(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
		};
	}

	void updateCamera(bool reverseZ, BufferSize& viewportSize) {
		// Set view & projection matrix
		matrices.view = XMMatrixLookAtLH(location.position, location.target, location.up);

		const float aspectRatio = static_cast<float>(viewportSize.width) / viewportSize.height;
		const float nearZ = 0.1f;
		const float farZ = 1000.0f;
		matrices.projection = XMMatrixPerspectiveFovLH(0.4f * 3.14f, aspectRatio, reverseZ ? farZ : nearZ, reverseZ ? nearZ : farZ);
	}

	void moveCameraDepth(float amount) {
		auto targetRelNorm = XMVector4Normalize(location.target - location.position);
		auto transl = targetRelNorm * amount;
		location.position = location.position + transl;
		location.target = location.target + transl;
	}

	void moveCameraHorizontal(float amount) {
		auto targetRel = location.target - location.position;
		auto cross = XMVector4Cross(targetRel, location.up, XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
		auto norm = XMVector4Normalize(cross);
		auto transl = norm * amount * -1;
		location.position = location.position + transl;
		location.target = location.target + transl;
	}

	void moveCameraVertical(float amount) {
		auto transl = location.up * amount;
		location.position = location.position + transl;
		location.target = location.target + transl;
	}

	void turnCameraHorizontal(float amount) {
		auto transf = XMMatrixRotationAxis(location.up, amount);
		auto targetRel = location.target - location.position;
		auto targetRelNew = XMVector4Transform(targetRel, transf);
		location.target = location.position + targetRelNew;
	}
}