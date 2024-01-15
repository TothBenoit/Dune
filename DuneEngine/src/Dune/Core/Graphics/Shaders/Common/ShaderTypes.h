#pragma once

#ifdef __cplusplus
#define BEGIN_NAMESPACE_SHADER(name) namespace name {
#define END_NAMESPACE_SHADER(name) }

namespace Dune
{
	using uint = dU32;

	using float3x3 = dMatrix3x3;
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

#define BEGIN_NAMESPACE_SHADER(name)
#define END_NAMESPACE_SHADER(name)

#endif