#pragma once

#include "Common/ShaderTypes.h"

BEGIN_NAMESPACE_SHADER(Dune)
	struct PostProcessGlobals
	{
		matrix		m_invProj;
		matrix		m_invViewProj;
		float4		m_cameraPosition;
		float3		m_cameraDir;
		float		m_padding1;
		float2		m_screenResolution;
		float2		m_padding2;
	};
END_NAMESPACE_SHADER(Dune)