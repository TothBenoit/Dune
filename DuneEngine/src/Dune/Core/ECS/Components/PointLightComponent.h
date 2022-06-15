#pragma once

namespace Dune
{
	struct PointLightComponent
	{
		float intensity = 1.f;
		float radius = 1.f;
		dVec3 color = { 1.f,1.f,1.f };
	};
}