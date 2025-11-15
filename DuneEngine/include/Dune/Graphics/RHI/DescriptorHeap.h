#pragma once

#include "Dune/Graphics/RHI/Resource.h"
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

	enum class EDescriptorHeapType : dU32
	{
		SRV_CBV_UAV,
		Sampler,
		RTV,
		DSV
	};

	struct DescriptorHeapDesc
	{
		EDescriptorHeapType type;
		dU32 capacity;
		bool isShaderVisible;
	};

	class DescriptorHeap : public Resource
	{
	public:
		[[nodiscard]] inline dU32 GetCapacity() const { return m_capacity; }
		[[nodiscard]] inline dU32 GetIndex(Descriptor descriptor) const { return dU32(descriptor.cpuAddress - m_cpuAddress) / m_descriptorSize; }
		[[nodiscard]] inline dU64 GetCPUAddress() const { return m_cpuAddress; }

		[[nodiscard]] Descriptor GetDescriptorAt(dU32 index) const;

	protected:
		dU64 m_cpuAddress{ 0 };
		dU64 m_gpuAddress{ 0 };
		dU32 m_descriptorSize{ 0 };
		dU32 m_capacity{ 0 };
	};

	class TransientDescriptorHeap : public DescriptorHeap
	{
	public:
		void Initialize(Device* pDevice, const DescriptorHeapDesc& desc);
		void Destroy();

		Descriptor Allocate(dU32 count);
		inline void Reset() { m_count = 0; }

	private:
		dU32 m_count;
	};

	class PersistentDescriptorHeap : public DescriptorHeap
	{
	public:
		void Initialize(Device* pDevice, const DescriptorHeapDesc& desc);
		void Destroy();

		[[nodiscard]] Descriptor Allocate();
		void Free(Descriptor handle);

	private:
		dVector<dU32> m_freeSlots{};
	};
}