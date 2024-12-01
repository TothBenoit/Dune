#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"
#include <mutex>

namespace Dune::Graphics
{
	class Device;

	struct Descriptor
	{
		dU64 cpuAddress{ 0 };
		dU64 gpuAddress{ 0 };

		[[nodiscard]] bool IsValid() { return cpuAddress != 0; }
		[[nodiscard]] bool IsShaderVisible() { return gpuAddress != 0; }
	};

	enum class DescriptorHeapType : dU32
	{
		SRV_CBV_UAV,
		Sampler,
		RTV,
		DSV
	};

	struct DescriptorHeapDesc
	{
		DescriptorHeapType type;
		dU32 capacity;
	};

	class DescriptorHeap : public Resource
	{
	public:
		void Initialize(Device* pDevice, const DescriptorHeapDesc& desc);
		void Destroy();

		[[nodiscard]] inline dU32 GetCapacity() { return m_capacity; }
		[[nodiscard]] inline dU32 GetSize() { return (dU32)m_freeSlots.size(); }
		[[nodiscard]] Descriptor Allocate();
		void Free(Descriptor handle);

	private:
		dU32 m_capacity{ 0 };
		dVector<dU32> m_freeSlots{};
		dU64 m_cpuAddress{ 0 };
		dU64 m_gpuAddress{ 0 };
		dU64 m_descriptorSize{ 0 };
		std::mutex m_lock;
	};
}