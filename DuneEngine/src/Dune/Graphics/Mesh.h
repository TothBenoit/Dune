#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	template <typename T, typename H>
	class Pool;
	class Buffer;

	struct Vertex
	{
		dVec3 position;
		dVec3 normal;
	};

	class Mesh
	{
		DISABLE_COPY_AND_MOVE(Mesh);
	public:

		[[nodiscard]] Handle<Buffer> GetIndexBufferHandle() const { return m_indexBufferHandle; }
		[[nodiscard]] Handle<Buffer> GeVertexBufferHandle() const { return m_vertexBufferHandle; }

	private:
		Mesh(const dVector<dU16>& indices, const dVector<Vertex>& vertices);
		Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);
		~Mesh();

		void UploadVertexBuffer(const void* pData, dU32 size, dU32 byteStride);
		void UploadIndexBuffer(const void* pData, dU32 size, dU32 byteStride);

	private:
		friend Pool<Mesh, Mesh>;

		Handle<Buffer> m_indexBufferHandle;
		Handle<Buffer> m_vertexBufferHandle;
	};
}