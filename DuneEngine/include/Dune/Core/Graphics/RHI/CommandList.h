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
	class Texture;
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

	enum class EPrimitiveTopology
	{
		TriangleList = 4,
	};

	struct Viewport
	{
		float topLeftX;
		float topLeftY;
		float width;
		float height;
		float minDepth;
		float maxDepth;
	};

	struct Scissor
	{
		dU32 left;
		dU32 top;
		dU32 right;
		dU32 bottom;
	};

	class CommandList : public Resource
	{
	public:
		void Initialize(Device* pDevice, ECommandType type, CommandAllocator& CommandAllocator);
		void Destroy();

        void Reset(CommandAllocator& commandAllocator);
        void Reset(CommandAllocator& commandAllocator, const GraphicsPipeline& pipeline);
        void Close();

		void SetPrimitiveTopology(EPrimitiveTopology primitiveTopology);
		void SetViewports(dU32 numViewport, Viewport* pViewports);
		void SetScissors(dU32 numScissor, Scissor* pScissors);

		void CopyBufferRegion(Buffer& destBuffer, dU64 dstOffset, Buffer& srcBuffer, dU64 srcOffset, dU64 size);
		void UploadTexture(Texture& destTexture, Buffer& uploadBuffer, dU64 uploadByteOffset, dU32 firstSubresource, dU32 numSubresource, void* pSrcData);

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