#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RenderPass/MaterialUpload.h"

namespace Dune::Graphics
{
	MaterialUploadData* MaterialUpload::Create(Renderer& renderer)
	{
		MaterialUploadData* pData = new MaterialUploadData();
		pData->srv = renderer.GetSRVHeap().Allocate();
		pData->srvIndex = renderer.GetSRVHeap().GetIndex(pData->srv);
		return pData;
	}

	void MaterialUpload::Setup(RenderGraphBuilder& builder, RenderPassContext& context, MaterialUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		dVector<MaterialData>& materials = context.pFrameData->materials;

		dU32 materialCount = (dU32)materials.size();
		const dU32 materialsByteSize = materialCount * (dU32)sizeof(MaterialData);

		if (pData->buffer.GetByteSize() < materialsByteSize)
		{
			Frame& frame = renderer.GetCurrentFrame();
			Device& device = renderer.GetRenderContext()->GetDevice();
			if (pData->buffer.Get())
				frame.buffersToRelease.push(pData->buffer);
			pData->buffer.Initialize(device, { .debugName{ L"MaterialBuffer" }, .memory{ EBufferMemory::GPU }, .byteSize{ materialsByteSize } });
			device.CreateSRV(pData->srv, pData->buffer, { .elementCount = materialCount, .byteStride = sizeof(MaterialData) });
			device.CopyDescriptors(1, pData->srv.cpuAddress, frame.srvHeap.GetDescriptorAt(pData->srvIndex + context.pFrameData->reservedSharedSRV).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->handle == kInvalidResourceHandle)
				pData->handle = renderer.RegisterBuffer(&pData->buffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->handle, &pData->buffer, EResourceState::Undefined);
		}

		builder.Write(pData->handle, EResourceState::CopyDest);
	}

	void MaterialUpload::Execute(RenderPassContext& context, MaterialUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Device& device = renderer.GetRenderContext()->GetDevice();
		Frame& frame = renderer.GetCurrentFrame();
		dVector<MaterialData>& materials = context.pFrameData->materials;
		const dU32 materialsByteSize = pData->buffer.GetByteSize();

		Buffer uploadBuffer{};
		uploadBuffer.Initialize(device, { .debugName{ L"MaterialUploadBuffer" }, .byteSize{ materialsByteSize } });

		void* pMappedData{ nullptr };
		uploadBuffer.Map(0, materialsByteSize, &pMappedData);
		memcpy(pMappedData, materials.data(), materialsByteSize);
		uploadBuffer.Unmap(0, materialsByteSize);

		frame.commandList.CopyBufferRegion(pData->buffer, 0, uploadBuffer, 0, materialsByteSize);
		frame.buffersToRelease.push(uploadBuffer);
	}

	void MaterialUpload::Destroy(Renderer& renderer, MaterialUploadData* pData)
	{
		renderer.GetSRVHeap().Free(pData->srv);
		if (pData->buffer.Get())
			pData->buffer.Destroy();
		delete pData;
	}
}
