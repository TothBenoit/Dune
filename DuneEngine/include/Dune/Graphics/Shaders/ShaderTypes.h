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
struct ForwardGlobals
{
	float4x4    viewProjectionMatrix;
	float3      cameraPosition;
	int         directionalLightCount;
	float3      ambientColor;
	int         pointLightCount;
};

struct InstanceData
{
	float4x4 modelMatrix;
};

struct PointLight
{
	float3  color;
	float   intensity;
	float3  position;
	float   radius;
};

struct DirectionalLight
{
	float3      color;
	float       intensity;
	float3      direction;
	float       _padding1;
};
END_NAMESPACE_SHADER