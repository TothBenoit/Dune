#pragma once

#include "ShaderTypes.h"

BEGIN_NAMESPACE_SHADER
	struct PBRGlobals
	{
		float4x4    viewProjectionMatrix;
		float3      sunDirection;
		float       _padding1;
		float3      cameraPosition;
		float       _padding2;
	};

	struct PBRInstance
	{
		float4x4 modelMatrix;
	};
END_NAMESPACE_SHADER