#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
    class DescriptorHeap;
	struct Descriptor;
	class Pipeline;
	class Texture;
	class CommandAllocator;
	class Buffer;
	struct Device;
	class Fence;

	enum class CommandType
	{
		Direct,
		Compute,
		Copy
	};

	class CommandAllocator : public Resource
	{
	public:
		CommandAllocator(Device* pDevice, CommandType type);
		~CommandAllocator();
		DISABLE_COPY_AND_MOVE(CommandAllocator);

		void Reset();
	};

	class CommandList : public Resource
	{
	public:
		CommandList(Device* pDevice, CommandType type, CommandAllocator& CommandAllocator);
		~CommandList();
		DISABLE_COPY_AND_MOVE(CommandList);

        void Reset(CommandAllocator& commandAllocator/*, Pipeline& pipeline*/);
        void Close();

        void SetDescriptorHeaps(DescriptorHeap& pSrvHeap);
        void SetDescriptorHeaps(DescriptorHeap& pSrvHeap, DescriptorHeap& pSamplerHeap);
		// void SetPipeline(const Pipeline& pipeline);
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
		CommandQueue(Device* pDevice, CommandType type);
		~CommandQueue();
		DISABLE_COPY_AND_MOVE(CommandQueue);

		void ExecuteCommandLists(CommandList* commandLists, dU32 commandListCount);
		void Signal(Fence& fence, dU64 value);
		void Wait(Fence& fence, dU64 value);
	};
}