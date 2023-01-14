#include "pch.h"
#include "DescriptorHeap.h"
#include "Renderer.h"

namespace Dune
{
    DescriptorHandle DescriptorHeap::Allocate()
    {
        Assert(!m_freeSlots.empty());
        DescriptorHandle handle{};
        dU32 slot{ m_freeSlots.back() };
        m_freeSlots.pop_back();
        handle.cpuAdress = D3D12_CPU_DESCRIPTOR_HANDLE{ m_cpuStartAdress.ptr + m_descriptorSize * (dU64)slot };
        handle.gpuAdress = (m_bIsShaderVisible) ? D3D12_GPU_DESCRIPTOR_HANDLE{ m_gpuStartAdress.ptr + m_descriptorSize * (dU64)slot } : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
#ifdef _DEBUG
        handle.heap = m_pDescriptorHeap;
        handle.slot = slot;
#endif
        return handle;
    }

    void DescriptorHeap::Free(DescriptorHandle handle)
    {
        Assert(handle.IsValid());
        Assert(handle.IsShaderVisible() == m_bIsShaderVisible);
        Assert(handle.heap == m_pDescriptorHeap);
        
        dU32 slot{ (dU32)((handle.cpuAdress.ptr - m_cpuStartAdress.ptr) / m_descriptorSize) };
        Assert(handle.slot == slot);

        m_freeSlots.push_back(slot);
    }

    void DescriptorHeap::Initialize(dU32 capacity, D3D12_DESCRIPTOR_HEAP_TYPE type)
    {
        Assert(!m_pDescriptorHeap);
        Assert(capacity != 0);

        m_capacity = capacity;
        m_type = type;

        D3D12_DESCRIPTOR_HEAP_FLAGS flags{};
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        {
            m_bIsShaderVisible = false;
            flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        }
        else
        {
            m_bIsShaderVisible = true;
            flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }

        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
        descriptorHeapDesc.NumDescriptors = m_capacity;
        descriptorHeapDesc.Type = m_type;
        descriptorHeapDesc.Flags = flags;
        descriptorHeapDesc.NodeMask = 0;
        ID3D12Device* pDevice{ Renderer::GetInstance().GetDevice() };
        ThrowIfFailed(pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pDescriptorHeap)));
        NameDXObject(m_pDescriptorHeap, L"DescriptorHeap");
        
        m_cpuStartAdress = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_gpuStartAdress = (m_bIsShaderVisible) ? m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{0};
        m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(m_type);

        m_freeSlots.reserve(m_capacity);
        for (dU32 i{ 0 }; i < m_capacity; i++)
        {
            m_freeSlots.push_back(i);
        }
    }

    void DescriptorHeap::Release()
    {
        Assert(m_freeSlots.size() == m_capacity);
        m_pDescriptorHeap->Release();
        
        m_capacity = 0;
        m_freeSlots.clear();
    }
}
