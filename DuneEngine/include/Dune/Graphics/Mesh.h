#pragma once

#include "Dune/Graphics/RHI/Buffer.h"

namespace Dune::Graphics
{
	class Buffer;
	class Device;
	class CommandList;

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

	struct MeshDesc
	{
		const void*    pVertices{ nullptr };
		dU32           vertexCount{ 0 };
		dU32           vertexByteStride{ 0 };
		const void*    pIndices{ nullptr };
		dU32           indexCount{ 0 };
		bool           isIndex32bits{ true };
		const SubMesh* pSubMeshes{ nullptr };
		dU32           subMeshCount{ 0 };
		dU32           materialSlotCount{ 0 };
	};

	class Mesh
	{
	public:
		void Initialize(Device& device, CommandList& commandList, Buffer& uploadBuffer, const MeshDesc& desc);
		void Destroy();


		[[nodiscard]] const dVector<SubMesh>& GetSubMeshes() const { return m_subMeshes; }
		[[nodiscard]] dU32                    GetMaterialSlotCount() const { return m_materialSlotCount; }
		[[nodiscard]] Buffer&                 GetIndexBuffer() { return m_indexBuffer; }
		[[nodiscard]] Buffer&                 GetVertexBuffer() { return m_vertexBuffer; }
		[[nodiscard]] dU32                    GetIndexCount() const { return m_indexCount; }
		[[nodiscard]] dU32                    GetVertexCount() const { return m_vertexCount; }
		[[nodiscard]] dU32                    GetVertexByteStride() const { return m_vertexByteStride; }
		[[nodiscard]] bool                    IsIndex32bits() const { return m_isIndex32bits; }

	private:
		dVector<SubMesh> m_subMeshes;
		dU32             m_materialSlotCount{ 0 };
		dU32             m_indexCount{ 0 };
		dU32             m_vertexCount{ 0 };
		dU32             m_vertexByteStride{ 0 };
		bool             m_isIndex32bits{ 0 };
		Buffer           m_indexBuffer;
		Buffer           m_vertexBuffer;
	};
}
