#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
    class DescriptorHeap;
	struct Descriptor;
	class GraphicsPipeline;
	class CommandAllocator;
	class Buffer;
	class Device;
	class Fence;
	class Barrier;

	enum class ECommandType
	{
		Direct,
		Compute,
		Copy
	};

	class CommandAllocator : public Resource
	{
	public:
		void Initialize(Device* pDevice, ECommandType type);
		void Destroy();

		void Reset();
	};

	class CommandList : public Resource
	{
	public:
		void Initialize(Device* pDevice, ECommandType type, CommandAllocator& CommandAllocator);
		void Destroy();

        void Reset(CommandAllocator& commandAllocator);
        void Reset(CommandAllocator& commandAllocator, const GraphicsPipeline& pipeline);
        void Close();

		void Transition(const Barrier& barrier);
        void SetDescriptorHeaps(DescriptorHeap& srvHeap);
        void SetDescriptorHeaps(DescriptorHeap& srvHeap, DescriptorHeap& samplerHeap);
		void SetGraphicsPipeline(const GraphicsPipeline& pipeline);
		void SetRenderTarget(const dU64* rtvs, dU32 rtvCount, const dU64* dsv);
		void ClearRenderTargetView(const Descriptor& rtv, const float clearColor[4]);
		void ClearDepthBuffer(const Descriptor& dsv, float depth, float stencil);
		void PushGraphicsConstants(dU32 slot, void* pData, dU32 byteSize);
		void PushGraphicsBuffer(dU32 slot, const Descriptor& cbv);
		void PushGraphicsResource(dU32 slot, const Descriptor& srv);
		void BindGraphicsResource(dU32 slot, const Descriptor& srv);
		void BindIndexBuffer(Buffer& indexBuffer);
		void BindVertexBuffer(Buffer& vertexBuffer);
		void DrawIndexedInstanced(dU32 indexCount, dU32 instanceCount, dU32 indexStart, dU32 stride, dU32 instanceStart );
	};

	class CommandQueue : public Resource
	{
	public:
		void Initialize(Device* pDevice, ECommandType type);
		void Destroy();

		void ExecuteCommandLists(CommandList* commandLists, dU32 commandListCount);
		void Signal(Fence& fence, dU64 value);
		void Wait(Fence& fence, dU64 value);
	};
}