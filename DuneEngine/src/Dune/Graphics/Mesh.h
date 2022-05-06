#pragma once

#include "Dune/Graphics/GraphicsBuffer.h"

namespace Dune
{
	struct Vertex
	{
		dVector3 position;
		dVector4 color;
	};

	class Mesh
	{
	public:
		Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);

		void UploadBuffers();
		bool IsUploaded() const { return m_isUploaded; }

		const GraphicsBuffer* const GetIndexBuffer() const { return m_indexBuffer.get(); }
		const GraphicsBuffer* const GetVertexBuffer() const { return m_vertexBuffer.get(); }

	private:

		bool UploadVertexBuffer();
		bool UploadIndexBuffer();

		dVector<dU32> m_indices;
		dVector<Vertex> m_vertices;

		bool m_isUploaded = false;

		std::unique_ptr<GraphicsBuffer> m_indexBuffer = nullptr;
		std::unique_ptr<GraphicsBuffer> m_vertexBuffer = nullptr;
	};
}