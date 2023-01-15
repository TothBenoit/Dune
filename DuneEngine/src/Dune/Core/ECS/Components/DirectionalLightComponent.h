#pragma once

namespace Dune
{
	struct DirectionalLightComponent
	{
		dVec3 color { 1.f,1.f,1.f };
		float intensity{ 0.8f };
	};
}