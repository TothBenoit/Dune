#include "pch.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics.h"

namespace Dune::Graphics
{
	Mesh::Mesh(View* pView, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
		: m_indexCount{ indexCount }
		, m_vertexCount{ vertexCount }
	{
		UploadIndexBuffer(pView, pIndices, indexCount, sizeof(dU16));
		UploadVertexBuffer(pView, pVertices, vertexCount, vertexByteStride );
	}

	Mesh::Mesh(View* pView, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
		: m_indexCount{ indexCount }
		, m_vertexCount{ vertexCount }
	{
		UploadIndexBuffer(pView, pIndices, indexCount, sizeof(dU32));
		UploadVertexBuffer(pView, pVertices, vertexCount, vertexByteStride);
	}

	Mesh::~Mesh()
	{
		Graphics::ReleaseBuffer(m_vertexBufferHandle);
		Graphics::ReleaseBuffer(m_indexBufferHandle);
	}

	void Mesh::UploadIndexBuffer(View* pView, const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", size * byteStride , EBufferUsage::Index, EBufferMemory::GPUStatic, pData, byteStride, pView };
		m_indexBufferHandle = Graphics::CreateBuffer(desc);
		Assert(m_indexBufferHandle.IsValid());
	}

	void Mesh::UploadVertexBuffer(View* pView, const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer",  size * byteStride, EBufferUsage::Vertex, EBufferMemory::GPUStatic, pData, byteStride, pView };
		m_vertexBufferHandle = Graphics::CreateBuffer(desc);
		Assert(m_vertexBufferHandle.IsValid());
	}
}

