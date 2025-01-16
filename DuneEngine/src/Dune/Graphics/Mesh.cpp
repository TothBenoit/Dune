#include "pch.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/CommandList.h"

namespace Dune::Graphics
{

	Buffer* CreateIndexBuffer(Device* pDevice, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", EBufferUsage::Index, EBufferMemory::GPUStatic, size * byteStride, byteStride };
		Buffer* pIndexBuffer = new Buffer();
		pIndexBuffer->Initialize(pDevice, desc);
		return pIndexBuffer;
	}

	Buffer* CreateVertexBuffer(Device* pDevice, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer",EBufferUsage::Vertex, EBufferMemory::GPUStatic, size * byteStride, byteStride };
		Buffer* pVertexBuffer = new Buffer();
		pVertexBuffer->Initialize(pDevice, desc);
		return pVertexBuffer;
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

		m_pUploadBuffer = new Buffer();
		BufferDesc desc{ L"UploadBuffer",EBufferUsage::Constant, EBufferMemory::CPU, uploadByteSize, uploadByteSize };
		m_pUploadBuffer->Initialize(pDevice, desc);

		m_pIndexBuffer = CreateIndexBuffer(pDevice, indexCount, sizeof(dU16));
		UploadBuffer(pCommandList, *m_pIndexBuffer, *m_pUploadBuffer, pIndices, indexByteSize, 0);
		m_pVertexBuffer = CreateVertexBuffer(pDevice, vertexCount, vertexByteStride);
		UploadBuffer( pCommandList, *m_pVertexBuffer, *m_pUploadBuffer, pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Initialize(Device* pDevice, CommandList* pCommandList, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;

		dU32 vertexByteSize = vertexCount * vertexByteStride;
		dU32 indexByteSize = indexCount * sizeof(dU32);
		dU32 uploadByteSize = vertexByteSize + indexByteSize;

		m_pUploadBuffer = new Buffer();
		BufferDesc desc{ L"UploadBuffer",EBufferUsage::Constant, EBufferMemory::CPU, uploadByteSize, uploadByteSize };
		m_pUploadBuffer->Initialize(pDevice, desc);

		m_pIndexBuffer = CreateIndexBuffer(pDevice, indexCount, sizeof(dU32));
		UploadBuffer(pCommandList, *m_pIndexBuffer, *m_pUploadBuffer, pIndices, indexByteSize, 0 );
		m_pVertexBuffer = CreateVertexBuffer(pDevice, vertexCount, vertexByteStride);
		UploadBuffer(pCommandList, *m_pVertexBuffer, *m_pUploadBuffer, pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Destroy()
	{
		m_pUploadBuffer->Destroy();
		delete m_pUploadBuffer;
		m_pIndexBuffer->Destroy();
		delete m_pIndexBuffer;
		m_pVertexBuffer->Destroy();
		delete m_pVertexBuffer;
	}
}

