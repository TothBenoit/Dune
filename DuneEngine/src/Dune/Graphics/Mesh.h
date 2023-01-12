#pragma once

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

		const Buffer* const GetIndexBuffer() const { return m_indexBuffer.get(); }
		const Buffer* const GetVertexBuffer() const { return m_vertexBuffer.get(); }

	private:
		void UploadBuffers();
		bool UploadVertexBuffer();
		bool UploadIndexBuffer();

		dVector<dU32> m_indices;
		dVector<Vertex> m_vertices;

		std::unique_ptr<Buffer> m_indexBuffer = nullptr;
		std::unique_ptr<Buffer> m_vertexBuffer = nullptr;
	};
}