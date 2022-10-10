#pragma once

namespace Dune
{
	struct CameraComponent
	{
		dMatrix viewMatrix;
		float	verticalFieldOfView = 45.f;
	};
}