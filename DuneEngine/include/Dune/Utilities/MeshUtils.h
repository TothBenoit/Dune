#pragma once

#include "Dune/Graphics/MeshData.h"

namespace Dune::MeshUtils
{
	[[nodiscard]] Graphics::MeshData CreateCube(const dVec3& size = { 1.0f, 1.0f, 1.0f });
	[[nodiscard]] Graphics::MeshData CreatePlane(const dVec2& size = { 1.0f, 1.0f });
	[[nodiscard]] Graphics::MeshData CreateSphere(float radius = 0.5f, dU32 segmentCount = 32);

	void Transform(Graphics::MeshData& meshData, const dMatrix& matrix);
	void Append(Graphics::MeshData& destination, const Graphics::MeshData& source);
}
