#pragma once

#include "Dune/Common/Types.h"
#include "Dune/Graphics/GraphicsBuffer.h"
#include <vector>

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
		Mesh(const std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices);

		void UploadBuffers();
		bool IsUploaded() const { return m_isUploaded; }

		const GraphicsBuffer* const GetIndexBuffer() const { return m_indexBuffer.get(); }
		const GraphicsBuffer* const GetVertexBuffer() const { return m_vertexBuffer.get(); }

	private:

		bool UploadVertexBuffer();
		bool UploadIndexBuffer();

		std::vector<uint32_t> m_indices;
		std::vector<Vertex> m_vertices;

		bool m_isUploaded = false;

		std::unique_ptr<GraphicsBuffer> m_indexBuffer = nullptr;
		std::unique_ptr<GraphicsBuffer> m_vertexBuffer = nullptr;
	};
}