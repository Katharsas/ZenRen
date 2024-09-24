#include "stdafx.h"
#include "Camera.h"

#include "Settings.h"
#include "Util.h"

using namespace DirectX;

namespace render::camera {

	struct CameraMatrices
	{
		XMMATRIX view;
		XMMATRIX projection;
		BoundingFrustum frustum;
	};

	struct CameraLocation
	{
		XMVECTOR position;// TODO position should have w = 1 everywhere?
		XMVECTOR target;
		XMVECTOR up;// normalized
	};

	CameraMatrices matrices;
	CameraLocation location;

	CameraLocation defaultLocation = {
		XMVectorSet(0.0f, 3.0f, -5.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVector4Normalize(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
	};
	CameraLocation g2NewWorldCity = {
		XMVectorSet(-25.0f, 10.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 5.0f, 0.0f, 0.0f),
		XMVector4Normalize(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
	};

	ObjectMatrices getWorldViewMatrix(const XMMATRIX & objectsWorldMatrix)
	{
		const XMMATRIX worldView = objectsWorldMatrix * matrices.view;

		// pre-transposed for HLSL usage - since HLSL requires all matrices in tranposed form, we save a double transpose on worldViewNormal
		ObjectMatrices result;
		result.worldView = XMMatrixTranspose(worldView);
		result.worldViewNormal = XMMatrixInverse(nullptr, worldView);
		return result;
	}
	XMMATRIX getProjectionMatrix()
	{
		// pre-transposed for HLSL usage
		return XMMatrixTranspose(matrices.projection);
	}
	BoundingFrustum getFrustum()
	{
		return matrices.frustum;
	}

	void init() {
		location = g2NewWorldCity;
	}

	XMVECTOR getCameraPosition() {
		return location.position;
	}

	void updateCamera(bool reverseZ, BufferSize& viewportSize, float viewDistance) {
		
		//reverseZ = false;
		// Set view & projection matrix
		matrices.view = XMMatrixLookAtLH(location.position, location.target, location.up);

		const float aspectRatio = static_cast<float>(viewportSize.width) / viewportSize.height;
		const float nearZ = 0.1f;
		const float farZ = viewDistance;
		// we use LH (Y-up) instead of RH (Z-up) for now
		matrices.projection = XMMatrixPerspectiveFovLH(0.4f * 3.14f, aspectRatio, reverseZ ? farZ : nearZ, reverseZ ? nearZ : farZ);
		
		// BoundingFrustum does not support z-reversed or RH
		XMMATRIX projectionForFrustum = reverseZ ? XMMatrixPerspectiveFovLH(0.4f * 3.14f, aspectRatio, nearZ, farZ) : matrices.projection;

		// update frustum
		BoundingFrustum::CreateFromMatrix(matrices.frustum, projectionForFrustum);
		XMVECTOR determinant;
		matrices.frustum.Transform(matrices.frustum, XMMatrixInverse(&determinant, matrices.view));
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

	void turnCameraVertical(float amount) {
		auto targetRel = location.target - location.position;
		auto axis = XMVector3Cross(location.up, targetRel);
		auto transf = XMMatrixRotationAxis(axis, amount);
		auto targetRelNew = XMVector4Transform(targetRel, transf);
		location.target = location.position + targetRelNew;
	}
}