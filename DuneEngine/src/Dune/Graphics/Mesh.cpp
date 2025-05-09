#include "pch.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/CommandList.h"

namespace Dune::Graphics
{

	Buffer CreateIndexBuffer(Device* pDevice, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", EBufferUsage::Index, EBufferMemory::GPU, size * byteStride, byteStride };
		Buffer indexBuffer{};
		indexBuffer.Initialize(pDevice, desc);
		return indexBuffer;
	}

	Buffer CreateVertexBuffer(Device* pDevice, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer",EBufferUsage::Vertex, EBufferMemory::GPU, size * byteStride, byteStride };
		Buffer vertexBuffer{};
		vertexBuffer.Initialize(pDevice, desc);
		return vertexBuffer;
	}

	void UploadBuffer(CommandList* pCommandList, Buffer& buffer, Buffer& uploadBuffer, const void* pData, dU32 byteSize, dU32 byteOffset )
	{
		void* pCpuAddress{ nullptr };
		uploadBuffer.Map(0, 0, &pCpuAddress);
		memcpy((void*)(dU64(pCpuAddress) + byteOffset), pData, byteSize);
		pCommandList->CopyBufferRegion(buffer, 0, uploadBuffer, byteOffset, byteSize);
		uploadBuffer.Unmap(0, 0);
	}

	void Mesh::Initialize(Device* pDevice, CommandList* pCommandList, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;

		dU32 vertexByteSize = vertexCount * vertexByteStride;
		dU32 indexByteSize = indexCount * sizeof(dU16);
		dU32 uploadByteSize = vertexByteSize + indexByteSize;

		BufferDesc desc{ L"UploadBuffer",EBufferUsage::Constant, EBufferMemory::CPU, uploadByteSize, uploadByteSize };
		m_uploadBuffer.Initialize(pDevice, desc);

		m_indexBuffer = CreateIndexBuffer(pDevice, indexCount, sizeof(dU16));
		UploadBuffer(pCommandList, m_indexBuffer, m_uploadBuffer, pIndices, indexByteSize, 0);
		m_vertexBuffer = CreateVertexBuffer(pDevice, vertexCount, vertexByteStride);
		UploadBuffer( pCommandList, m_vertexBuffer, m_uploadBuffer, pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Initialize(Device* pDevice, CommandList* pCommandList, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;

		dU32 vertexByteSize = vertexCount * vertexByteStride;
		dU32 indexByteSize = indexCount * sizeof(dU32);
		dU32 uploadByteSize = vertexByteSize + indexByteSize;

		BufferDesc desc{ L"UploadBuffer",EBufferUsage::Constant, EBufferMemory::CPU, uploadByteSize, uploadByteSize };
		m_uploadBuffer.Initialize(pDevice, desc);

		m_indexBuffer = CreateIndexBuffer(pDevice, indexCount, sizeof(dU32));
		UploadBuffer(pCommandList, m_indexBuffer, m_uploadBuffer, pIndices, indexByteSize, 0 );
		m_vertexBuffer = CreateVertexBuffer(pDevice, vertexCount, vertexByteStride);
		UploadBuffer(pCommandList, m_vertexBuffer, m_uploadBuffer, pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Destroy()
	{
		m_uploadBuffer.Destroy();
		m_indexBuffer.Destroy();
		m_vertexBuffer.Destroy();
	}
}

