#pragma once

#include "ShaderTypes.h"

BEGIN_NAMESPACE_SHADER
	struct PBRGlobals
	{
		float4x4	viewProjectionMatrix;
		float3		sunDirection;
		float		_padding;
	};

	struct PBRMaterial
	{
		float3	albedo;
		float	roughness;
	};

	struct PBRInstance
	{
		float4x4 modelMatrix;
	};
END_NAMESPACE_SHADER