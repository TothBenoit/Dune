#pragma once

namespace Dune
{
	struct Transform;

	struct Camera
	{
		float fov{ 90.f };
		float near{ 0.1f };
		float far{ 10000.0f };
	};

	void ComputeViewMatrix(const Transform& transform, dMatrix4x4* pView);
	void ComputeProjectionMatrix(const Camera& camera, float aspectRatio, dMatrix4x4* pProjection);
}
