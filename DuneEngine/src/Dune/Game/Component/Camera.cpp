#include "pch.h"
#include "Dune/Game/Component/Camera.h"
#include "Dune/Game/Component/Transform.h"

namespace Dune
{
	void ComputeViewMatrix(const Transform& transform, dMatrix4x4* pView)
	{
		Assert(pView);
		const dMatrix view{ DirectX::XMMatrixLookToLH(DirectX::XMLoadFloat3(&transform.position), DirectX::XMVector3Rotate({ 0.0f, 0.0f, 1.0f }, transform.rotation), DirectX::XMVector3Rotate({ 0.0f, 1.0f, 0.0f }, transform.rotation)) };
		DirectX::XMStoreFloat4x4(pView, view);
	}

	void ComputeProjectionMatrix(const Camera& camera, float aspectRatio, dMatrix4x4* pProjection)
	{
		Assert(pProjection);
		const dMatrix projection{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(camera.fov), aspectRatio, camera.near, camera.far) };
		DirectX::XMStoreFloat4x4(pProjection, projection);
	}
}
