#pragma once

namespace Dune
{
	struct Camera
	{
		dVec3 position{ 0.0f, 0.0f, 0.0f };
		dVec3 target{ 0.0f, 0.0f, 1.0f };
		dVec3 up{ 0.0f, 1.0f, 0.0f };
		float aspectRatio{ 16.f / 9.f };
		float fov{ 90.f };
		float near{ 0.1f };
		float far{ 10000.0f };
	};

	void ComputeViewProjectionMatrix(const Camera& camera, dMatrix4x4* pView, dMatrix4x4* pProjection, dMatrix4x4* pViewProjection);
	void ComputeViewProjectionMatrix(const Camera& camera, dMatrix4x4* pViewProjection);
	void ComputeViewMatrix(const Camera& camera, dMatrix4x4* pView);
	void ComputeViewMatrix(const dVec3& position, const dVec3& target, const dVec3& up, dMatrix4x4* pView);
	void ComputeProjectionMatrix(const Camera& camera, dMatrix4x4* pProjection);
	void ComputeProjectionMatrix(float aspectRatio, float fov, float near, float far, dMatrix4x4* pProjection);
}