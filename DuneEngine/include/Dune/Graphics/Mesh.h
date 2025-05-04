#pragma once

namespace Dune::Graphics
{
	class Buffer;
	class Device;
	class CommandList;

	struct Vertex
	{
		dVec3 position;
		dVec3 normal;
		dVec3 tangent;
		dVec2 uv;
	};

	class Mesh
	{
	public:
		void Initialize(Device* pDevice, CommandList* pCommandList, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		void Initialize(Device* pDevice, CommandList* pCommandList, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		void Destroy();

		[[nodiscard]] Buffer&           GetIndexBuffer() const { return *m_pIndexBuffer; }
		[[nodiscard]] Buffer&           GetVertexBuffer() const { return *m_pVertexBuffer; }
		[[nodiscard]] dU32              GetIndexCount() const { return m_indexCount; }
		[[nodiscard]] dU32              GetVertexCount() const { return m_vertexCount; }

	private:
		dU32 m_indexCount{ 0 };
		dU32 m_vertexCount{ 0 };
		Buffer* m_pIndexBuffer;
		Buffer* m_pVertexBuffer;
		Buffer* m_pUploadBuffer;
	};
}