#pragma once

#include "Dune/Graphics/RHI/Buffer.h"

namespace Dune::Graphics
{
	class Buffer;
	class Device;
	class CommandList;

	struct Vertex
	{
		dVec3 position;
		dVec3 normal;
		dVec3 tangent;
		dVec2 uv;
	};

	class Mesh
	{
	public:
		void Initialize(Device* pDevice, CommandList* pCommandList, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		void Initialize(Device* pDevice, CommandList* pCommandList, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		void Destroy();

		[[nodiscard]] Buffer&           GetIndexBuffer() { return m_indexBuffer; }
		[[nodiscard]] Buffer&           GetVertexBuffer() { return m_vertexBuffer; }
		[[nodiscard]] dU32              GetIndexCount() const { return m_indexCount; }
		[[nodiscard]] dU32              GetVertexCount() const { return m_vertexCount; }
		[[nodiscard]] dU32              GetVertexByteStride() const { return m_vertexByteStride; }
		[[nodiscard]] bool              IsIndex32bits() const { return m_isIndex32bits; }

	private:
		dU32 m_indexCount{ 0 };
		dU32 m_vertexCount{ 0 };
		dU32 m_vertexByteStride{ 0 };
		bool m_isIndex32bits{ 0 };
		Buffer m_indexBuffer;
		Buffer m_vertexBuffer;
		Buffer m_uploadBuffer;
	};
}