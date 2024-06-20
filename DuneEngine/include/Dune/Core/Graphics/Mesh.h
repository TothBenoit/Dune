#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	template <typename T, typename H, bool ThreadSafe>
	class Pool;
}

namespace Dune::Graphics
{
	class Buffer;
	struct Device;

	struct Vertex
	{
		dVec3 position;
		dVec3 normal;
	};

	class Mesh
	{
		DISABLE_COPY_AND_MOVE(Mesh);
	public:

		[[nodiscard]] Handle<Buffer>	GetIndexBufferHandle() const { return m_indexBufferHandle; }
		[[nodiscard]] Handle<Buffer>	GetVertexBufferHandle() const { return m_vertexBufferHandle; }
		[[nodiscard]] dU32				GetIndexCount() const { return m_indexCount; }
		[[nodiscard]] dU32				GetVertexCount() const { return m_vertexCount; }

	private:
		Mesh(Device* pDevice, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		Mesh(Device* pDevice, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		~Mesh();

		void UploadVertexBuffer(const void* pData, dU32 size, dU32 byteStride);
		void UploadIndexBuffer(const void* pData, dU32 size, dU32 byteStride);

	private:
		friend Pool<Mesh, Mesh, true>;
		dU32 m_indexCount{ 0 };
		dU32 m_vertexCount{ 0 };
		Handle<Buffer> m_indexBufferHandle;
		Handle<Buffer> m_vertexBufferHandle;
		Device* m_pDevice{ nullptr };
	};
}