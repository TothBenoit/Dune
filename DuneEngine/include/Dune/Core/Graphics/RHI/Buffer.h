#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"
#include "Dune/Core/Graphics/RHI/Barrier.h"

namespace Dune::Graphics
{
	class Device;

	enum class EBufferUsage
	{
		Index,
		Vertex,
		Constant,
		Structured
	};

	enum class EBufferMemory
	{
		CPU,
		GPU,
		GPUStatic
	};

	struct BufferDesc
	{
		const wchar_t* debugName{ L"Buffer" };

		EBufferUsage	usage{ EBufferUsage::Constant };
		EBufferMemory	memory{ EBufferMemory::CPU };
		dU32			byteSize{ 0 };
		dU32			byteStride{ 0 }; // for structured, vertex and index buffer
		EResourceState	initialState{ EResourceState::Undefined };
	};

	class Buffer : public Resource
	{
	public:
		void Initialize(Device* pDeviceInterface, const BufferDesc& desc);
		void Destroy();

		[[nodiscard]] inline dU32 GetByteSize() const { return m_byteSize; }
		[[nodiscard]] inline dU32 GetByteStride() const { return m_byteStride; }
		[[nodiscard]] inline EBufferUsage GetUsage() const { return m_usage; }

		void Map(dU32 byteOffset, dU32 byteSize, void** pCpuAdress);
		void Unmap(dU32 byteOffset, dU32 byteSize);

	private:
		EBufferUsage	m_usage;
		EBufferMemory	m_memory;
		dU32			m_byteSize;
		dU32			m_byteStride;
		dU32			m_state;
	};
}