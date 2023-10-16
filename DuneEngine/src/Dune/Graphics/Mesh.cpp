#include "pch.h"
#include "Mesh.h"
#include "Dune/Core/Logger.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Buffer.h"

namespace Dune
{
	Mesh::Mesh(const dVector<dU16>& indices, const dVector<Vertex>& vertices)
	{
		UploadVertexBuffer(vertices.data(), (dU32)vertices.size(), sizeof(Vertex));
		UploadIndexBuffer(indices.data(), (dU32)indices.size(), sizeof(dU16));
	}
	Mesh::Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices)
	{
		UploadVertexBuffer(vertices.data(), (dU32)vertices.size(), sizeof(Vertex));
		UploadIndexBuffer(indices.data(), (dU32)indices.size(), sizeof(dU32));
	}

	Mesh::~Mesh()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseBuffer(m_vertexBufferHandle);
		renderer.ReleaseBuffer(m_indexBufferHandle);
	}

	void Mesh::UploadVertexBuffer(const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer",  size * byteStride, EBufferUsage::Vertex, EBufferMemory::GPUStatic, pData, byteStride };
		m_vertexBufferHandle = Renderer::GetInstance().CreateBuffer(desc);
		Assert(m_vertexBufferHandle.IsValid());
	}

	void Mesh::UploadIndexBuffer(const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", size * byteStride , EBufferUsage::Index, EBufferMemory::GPUStatic, pData, byteStride};
		m_indexBufferHandle = Renderer::GetInstance().CreateBuffer(desc);
		Assert(m_indexBufferHandle.IsValid());
	}

}

