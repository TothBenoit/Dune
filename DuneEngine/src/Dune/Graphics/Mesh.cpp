#include "pch.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/CommandList.h"

namespace Dune::Graphics
{
	Buffer CreateIndexBuffer(Device& device, dU32 byteSize)
	{
		Assert(byteSize != 0);
		BufferDesc desc{ L"IndexBuffer", EBufferUsage::Default, EBufferMemory::GPU, byteSize };
		Buffer indexBuffer{};
		indexBuffer.Initialize(device, desc);
		return indexBuffer;
	}

	Buffer CreateVertexBuffer(Device& device, dU32 byteSize)
	{
		Assert(byteSize != 0);
		BufferDesc desc{ L"VertexBuffer", EBufferUsage::Default, EBufferMemory::GPU, byteSize };
		Buffer vertexBuffer{};
		vertexBuffer.Initialize(device, desc);
		return vertexBuffer;
	}

	void Mesh::Initialize(Device& device, const MeshDesc& desc)
	{
		m_subMeshes.assign(desc.pSubMeshes, desc.pSubMeshes + desc.subMeshCount);
		m_materialSlotCount = desc.materialSlotCount;
		m_isIndex32bits = desc.isIndex32bits;
		m_vertexByteStride = desc.vertexByteStride;
		m_vertexCount = desc.vertexCount;
		m_indexCount = desc.indexCount;

		m_indexBuffer = CreateIndexBuffer(device, m_indexCount * (m_isIndex32bits ? sizeof(dU32) : sizeof(dU16)));
		m_vertexBuffer = CreateVertexBuffer(device, m_vertexCount * m_vertexByteStride);
	}

	dU64 Mesh::GetUploadSize() const
	{
		return (dU64)m_indexBuffer.GetByteSize() + m_vertexBuffer.GetByteSize();
	}

	void Mesh::Upload(const MeshDesc& desc, CommandList& commandList, const StagingMemory& staging)
	{
		Assert(staging.IsValid() && desc.indexCount == m_indexCount && desc.vertexCount == m_vertexCount);
		const dU32 indexByteSize = m_indexBuffer.GetByteSize();
		const dU32 vertexByteSize = m_vertexBuffer.GetByteSize();

		memcpy(staging.pCpuAddress, desc.pIndices, indexByteSize);
		memcpy((dU8*)staging.pCpuAddress + indexByteSize, desc.pVertices, vertexByteSize);

		Buffer stagingBuffer{ staging.buffer };
		commandList.CopyBufferRegion(m_indexBuffer, 0, stagingBuffer, staging.offset, indexByteSize);
		commandList.CopyBufferRegion(m_vertexBuffer, 0, stagingBuffer, staging.offset + indexByteSize, vertexByteSize);
	}

	void Mesh::Destroy()
	{
		m_indexBuffer.Destroy();
		m_vertexBuffer.Destroy();
	}
}
