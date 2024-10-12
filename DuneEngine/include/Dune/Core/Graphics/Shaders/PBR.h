#pragma once

#include "ShaderTypes.h"

BEGIN_NAMESPACE_SHADER
	struct PBRGlobals
	{
		float4x4	viewProjectionMatrix;
	};

	struct PBRInstance
	{
		float4x4 modelMatrix;
	};
END_NAMESPACE_SHADER