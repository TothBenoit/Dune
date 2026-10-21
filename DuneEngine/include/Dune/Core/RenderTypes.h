#pragma once

namespace Dune
{
	enum class EAlphaMode : dU8
	{
		Opaque,
		Mask,
		Blend,
		Count
	};
	static_assert((dU32)EAlphaMode::Blend == (dU32)EAlphaMode::Count - 1, "Blend must sort last");

	struct Vertex
	{
		dVec3 position;
		dVec3 normal;
		dVec4 tangent;
		dVec2 uv;
	};

	struct SubMesh
	{
		dU32 indexOffset;
		dU32 indexCount;
		dU32 vertexOffset;
		dU32 materialSlot;
	};
}
