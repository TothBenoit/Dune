#include "pch.h"
#include "Dune/Graphics/Camera.h"

namespace Dune::Graphics
{
	void ComputeViewProjectionMatrix(const Camera& camera, dMatrix4x4* pView, dMatrix4x4* pProjection, dMatrix4x4* pViewProjection)
	{
		dMatrix view{ DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera.position), DirectX::XMLoadFloat3(&camera.target), DirectX::XMLoadFloat3(&camera.up)) };
		dMatrix projection{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(camera.fov), camera.aspectRatio, camera.near, camera.far) };

		if (pView != nullptr)
			DirectX::XMStoreFloat4x4(pView, view);

		if (pProjection != nullptr)
			DirectX::XMStoreFloat4x4(pProjection, projection);

		if (pViewProjection != nullptr)
			DirectX::XMStoreFloat4x4(pViewProjection, view * projection);
	}

	void ComputeViewProjectionMatrix(const Camera& camera, dMatrix4x4* pViewProjection)
	{
		Assert(pViewProjection);
		dMatrix view{ DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera.position), DirectX::XMLoadFloat3(&camera.target), DirectX::XMLoadFloat3(&camera.up)) };
		dMatrix projection{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(camera.fov), camera.aspectRatio, camera.near, camera.far) };
		DirectX::XMStoreFloat4x4(pViewProjection, view * projection);
	}

	void ComputeViewMatrix(const Camera& camera, dMatrix4x4* pView)
	{
		Assert(pView);
		dMatrix view{ DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera.position), DirectX::XMLoadFloat3(&camera.target), DirectX::XMLoadFloat3(&camera.up)) };
		DirectX::XMStoreFloat4x4(pView, view);		
	}

	void ComputeViewMatrix(const dVec3& position, const dVec3& target, const dVec3& up, dMatrix4x4* pView)
	{
		Assert(pView);
		dMatrix view{ DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&position), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up)) };
		DirectX::XMStoreFloat4x4(pView, view);
	}

	void ComputeProjectionMatrix(const Camera& camera, dMatrix4x4* pProjection)
	{
		Assert(pProjection);
		dMatrix projection{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(camera.fov), camera.aspectRatio, camera.near, camera.far) };
		DirectX::XMStoreFloat4x4(pProjection, projection);
	}

	void ComputeProjectionMatrix(float aspectRatio, float fov, float near, float far, dMatrix4x4* pProjection)
	{
		Assert(pProjection);
		dMatrix projection{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(fov), aspectRatio, near, far) };
		DirectX::XMStoreFloat4x4(pProjection, projection);
	}
}