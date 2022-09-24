#pragma once

namespace Dune
{
	struct CameraComponent
	{
		dMatrix viewMatrix;
		dMatrix projectionMatrix;
		float	verticalFieldOfView = 45.f;
	};
}