#pragma once

#include "Common/ShaderTypes.h"

BEGIN_NAMESPACE_SHADER(Dune)
	struct PostProcessGlobals
	{
		matrix		m_invProj;
		float2		m_screenResolution;
		float2		m_padding;
	};
END_NAMESPACE_SHADER(Dune)