#pragma once

namespace Dune
{
	enum class ELightType : dU8
	{
		Directional,
		Point,
		Spot
	};

	struct Light
	{
		dVec3       color;
		float       intensity;
		dVec3       position;
		float       range;
		dVec3       direction;
		float       angle;
		float       penumbra;
		ELightType  type;
		bool        castShadow;
	};
}
