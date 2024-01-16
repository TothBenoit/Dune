#pragma once

#include "ShaderTypes.h"

BEGIN_NAMESPACE_SHADER(Dune)
	struct DirectionalLight
	{
		float3		m_color;
		float		m_intensity;
		float3		m_dir;
		float		m_padding1;
		float4x4	m_viewProj;
	};
END_NAMESPACE_SHADER(Dune)