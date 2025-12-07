#pragma once

#include "Dune/Graphics/RHI/Resource.h"
#include "Dune/Graphics/RHI/Barrier.h"

namespace Dune::Graphics
{
	class Device;

	enum class EBufferUsage
	{
		Default  = 0,
		Uniform  = 1 << 0,
		UAV      = 1 << 1,
	};

	enum class EBufferMemory
	{
		CPU,
		GPU
	};

	struct BufferDesc
	{
		const wchar_t* debugName{ L"Buffer" };

		EBufferUsage    usage{ EBufferUsage::Default };
		EBufferMemory   memory{ EBufferMemory::CPU };
		dU32            byteSize{ 0 };
		EResourceState  initialState{ EResourceState::Undefined };
	};

	class Buffer : public Resource
	{
	public:
		void Initialize(Device* pDeviceInterface, const BufferDesc& desc);
		void Destroy();

		[[nodiscard]] inline dU32 GetByteSize() const { return m_byteSize; }

		void Map(dU32 byteOffset, dU32 byteSize, void** pCpuAdress);
		void Unmap(dU32 byteOffset, dU32 byteSize);

	private:
		dU32            m_byteSize;
		dU32            m_state;
	};
}