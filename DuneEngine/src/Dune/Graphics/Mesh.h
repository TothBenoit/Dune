#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
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
		Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);
		~Mesh();

		void UploadBuffers();
		void UploadVertexBuffer();
		void UploadIndexBuffer();

	private:
		template <typename T, typename H> friend class Pool;

		dVector<dU32> m_indices;
		dVector<Vertex> m_vertices;

		Handle<Buffer> m_indexBufferHandle;
		Handle<Buffer> m_vertexBufferHandle;
	};
}