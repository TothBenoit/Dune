#pragma once

namespace Dune
{
	struct CameraComponent
	{
		dMatrix viewMatrix;
		float	verticalFieldOfView{ 85.f };
	};
}