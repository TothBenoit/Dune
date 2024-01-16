#pragma once

#include "ShaderTypes.h"

BEGIN_NAMESPACE_SHADER(Dune)
	struct PointLight
	{
		float3	m_color;
		float	m_intensity;
		float	m_radius;
		float3	m_pos;
	};
END_NAMESPACE_SHADER(Dune)