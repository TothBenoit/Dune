#pragma once

#ifdef __cplusplus
#define BEGIN_NAMESPACE_SHADER namespace Dune::Graphics {
#define END_NAMESPACE_SHADER }

namespace Dune::Graphics
{
	using uint = dU32;

	using float3x3 = dMatrix3x3;
	using float3x4 = dMatrix3x4;
	using float4x3 = dMatrix4x3;
	using float4x4 = dMatrix4x4;
	using matrix = dMatrix;

	using float2 = dVec2;
	using float3 = dVec3;
	using float4 = dVec4;

	using uint2 = dVec2u;
	using uint3 = dVec3u;
	using uint4 = dVec4u;

	using int2 = dVec2i;
	using int3 = dVec3i;
	using int4 = dVec4i;
}

#else

#define BEGIN_NAMESPACE_SHADER
#define END_NAMESPACE_SHADER

#endif

BEGIN_NAMESPACE_SHADER

#define SHADOW_MAP_RESOLUTION 4096
#define SHADOW_MAP_RESOLUTION_F float(SHADOW_MAP_RESOLUTION)

struct ForwardGlobals
{
	float4x4   viewProjectionMatrix;
	float3     cameraPosition;
	int        directionalLightCount;
	int        pointLightCount;
	int        spotLightCount;
	uint       directionalLightBufferIndex;
	uint       pointLightBufferIndex;
	uint       spotLightBufferIndex;
	uint3      _padding;
};

struct InstanceData
{
	float4x4   modelMatrix;
};

struct MaterialData
{
	float3     baseColor;
	float      metalnessFactor;
	float      roughnessFactor;
	uint       albedoIdx;
	uint       normalIdx;
	uint       roughnessMetalnessIdx;
};

struct PointLight
{
	float3     color;
	float      intensity;
	float3     position;
	float      radius;
};

struct DirectionalLight
{
	float3     color;
	float      intensity;
	float3     direction;
	uint       shadowIndex;
	float4x4   viewProjection;
};

struct SpotLight
{
	float3     color;
	float      intensity;
	float3     position;
	float      range;
	float3     direction;
	float      angle;
	float      penumbra;
	float3     _padding;
};
END_NAMESPACE_SHADER