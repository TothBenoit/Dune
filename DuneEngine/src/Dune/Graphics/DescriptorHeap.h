#pragma once

namespace Dune
{
	class Renderer;

	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuAdress{ 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE gpuAdress{ 0 };

		bool IsValid() { return cpuAdress.ptr != 0; }
		bool IsShaderVisible() { return gpuAdress.ptr != 0; }

#ifdef _DEBUG
		dU32 slot{0};
		ID3D12DescriptorHeap* heap{nullptr};
#endif
	};

	class DescriptorHeap
	{
	public:
		inline dU32 GetCapacity() { return m_capacity; }
		inline dU32 GetSize() { return (dU32)m_freeSlots.size(); }
		DescriptorHandle Allocate();
		void Free(DescriptorHandle handle);

	private:
		DescriptorHeap() = default;
		void Initialize(dU32 capacity, D3D12_DESCRIPTOR_HEAP_TYPE type);
		void Release();
	
	private:
		template<typename T, typename H> friend class Pool;
	
		dU32 m_capacity{ 0 };
		dVector<dU32> m_freeSlots{};
		D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
		D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStartAdress{ 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStartAdress{ 0 };
		dU64 m_descriptorSize{ 0 };
		ID3D12DescriptorHeap* m_pDescriptorHeap{ nullptr };
		bool m_bIsShaderVisible{ false };
	};
}