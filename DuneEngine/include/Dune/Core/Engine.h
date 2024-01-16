#pragma once

namespace Dune
{
	struct Camera
	{
		dVec3 position{ 0.f, 0.f, 0.f };
		dVec3 rotation{ 0.f, 0.f, 0.f };

		dMatrix matrix;

		dMatrix viewMatrix;
		float	verticalFieldOfView{ 85.f };
	};
}