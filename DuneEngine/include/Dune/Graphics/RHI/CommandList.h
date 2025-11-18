#pragma once

#include "Dune/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
    class DescriptorHeap;
	struct Descriptor;
	class RootSignature;
	class PipelineState;
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
        void Reset(CommandAllocator& commandAllocator, PipelineState& pipeline);
        void Close();

		void SetPrimitiveTopology(EPrimitiveTopology primitiveTopology);
		void SetViewports(dU32 numViewport, Viewport* pViewports);
		void SetScissors(dU32 numScissor, Scissor* pScissors);

		void CopyBufferRegion(Buffer& destBuffer, dU64 dstOffset, Buffer& srcBuffer, dU64 srcOffset, dU64 size);
		void UploadTexture(Texture& destTexture, Buffer& uploadBuffer, dU64 uploadByteOffset, dU32 firstSubresource, dU32 numSubresource, void* pSrcData);

		void Transition(const Barrier& barrier);
        void SetDescriptorHeaps(DescriptorHeap& srvHeap);
        void SetDescriptorHeaps(DescriptorHeap& srvHeap, DescriptorHeap& samplerHeap);
		void SetGraphicsRootSignature(RootSignature& rootSignature);
		void SetComputeRootSignature(RootSignature& rootSignature);
		void SetPipelineState(PipelineState& pipeline);
		void SetRenderTarget(const dU64* rtvs, dU32 rtvCount, const dU64* dsv);
		void ClearRenderTargetView(const Descriptor& rtv, const float clearColor[4]);
		void ClearDepthBuffer(const Descriptor& dsv, float depth, float stencil);
		void ClearUAVUInt(dU64 gpuAddress, dU64 cpuAddress, void* pResource, dU32 value);
		void PushGraphicsConstants(dU32 slot, const void* pData, dU32 byteSize);
		void PushComputeConstants(dU32 slot, const void* pData, dU32 byteSize);
		void PushGraphicsBuffer(dU32 slot, Buffer& buffer);
		void PushComputeBuffer(dU32 slot, Buffer& buffer);
		void PushGraphicsSRV(dU32 slot, Buffer& buffer);
		void PushComputeSRV(dU32 slot, Buffer& buffer);
		void PushGraphicsUAV(dU32 slot, Buffer& buffer);
		void PushComputeUAV(dU32 slot, Buffer& buffer);
		void BindGraphicsGroup(dU32 slot, const Descriptor& srv);
		void BindComputeGroup(dU32 slot, const Descriptor& srv);
		void BindIndexBuffer(Buffer& indexBuffer, bool is32bits);
		void BindVertexBuffer(Buffer& vertexBuffer, dU32 byteStride);
		void DrawInstanced(dU32 vertexCount, dU32 instanceCount, dU32 vertexStart, dU32 instanceStart);
		void DrawIndexedInstanced(dU32 indexCount, dU32 instanceCount, dU32 indexStart, dU32 stride, dU32 instanceStart );
		void Dispatch(dU32 threadGroupCountX, dU32 threadGroupCountY, dU32 threadGroupCountZ);
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