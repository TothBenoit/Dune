#pragma once

namespace Dune::Graphics
{
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

	struct MeshData
	{
		dVector<Vertex>  vertices;
		dVector<dU32>    indices;
		dVector<SubMesh> subMeshes;
		dU32             materialSlotCount{ 0 };
	};
}
