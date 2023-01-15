#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	class Buffer;

	struct Vertex
	{
		dVec3 position;
		dVec4 color;
		dVec3 normal;
	};

	class Mesh
	{
	public:
		Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);
		~Mesh();
		DISABLE_COPY_AND_MOVE(Mesh);

		Handle<Buffer> GetIndexBufferHandle() const { return m_indexBufferHandle; }
		Handle<Buffer> GeVertexBufferHandle() const { return m_vertexBufferHandle; }

	private:
		void UploadBuffers();
		void UploadVertexBuffer();
		void UploadIndexBuffer();

		dVector<dU32> m_indices;
		dVector<Vertex> m_vertices;

		Handle<Buffer> m_indexBufferHandle;
		Handle<Buffer> m_vertexBufferHandle;
	};
}