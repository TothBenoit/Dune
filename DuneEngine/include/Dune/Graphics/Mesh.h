#pragma once

#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Graphics/MeshData.h"
#include "Dune/Graphics/StagingMemory.h"

namespace Dune::RHI
{
	class Buffer;
	class Device;
	class CommandList;
}

namespace Dune::Graphics
{
	struct MeshDesc
	{
		const void*    pVertices{ nullptr };
		dU32           vertexCount{ 0 };
		dU32           vertexByteStride{ 0 };
		const void*    pIndices{ nullptr };
		dU32           indexCount{ 0 };
		bool           isIndex32bits{ true };
		const SubMesh* pSubMeshes{ nullptr };
		dU32           subMeshCount{ 0 };
		dU32           materialSlotCount{ 0 };
	};

	class Mesh
	{
	public:
		static constexpr dU64 kUploadAlignment{ 16 };

		void Initialize(RHI::Device& device, const MeshDesc& desc);
		void Destroy();

		[[nodiscard]] dU64 GetUploadSize() const;
		void Upload(const MeshDesc& desc, RHI::CommandList& commandList, const StagingMemory& staging);

		[[nodiscard]] const dVector<SubMesh>& GetSubMeshes() const { return m_subMeshes; }
		[[nodiscard]] dU32                    GetMaterialSlotCount() const { return m_materialSlotCount; }
		[[nodiscard]] RHI::Buffer&            GetIndexBuffer() { return m_indexBuffer; }
		[[nodiscard]] const RHI::Buffer&      GetIndexBuffer() const { return m_indexBuffer; }
		[[nodiscard]] RHI::Buffer&            GetVertexBuffer() { return m_vertexBuffer; }
		[[nodiscard]] const RHI::Buffer&      GetVertexBuffer() const { return m_vertexBuffer; }
		[[nodiscard]] dU32                    GetIndexCount() const { return m_indexCount; }
		[[nodiscard]] dU32                    GetVertexCount() const { return m_vertexCount; }
		[[nodiscard]] dU32                    GetVertexByteStride() const { return m_vertexByteStride; }
		[[nodiscard]] bool                    IsIndex32bits() const { return m_isIndex32bits; }

	private:
		dVector<SubMesh> m_subMeshes;
		dU32             m_materialSlotCount{ 0 };
		dU32             m_indexCount{ 0 };
		dU32             m_vertexCount{ 0 };
		dU32             m_vertexByteStride{ 0 };
		bool             m_isIndex32bits{ 0 };
		RHI::Buffer      m_indexBuffer;
		RHI::Buffer      m_vertexBuffer;
	};
}
