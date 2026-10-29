#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Graphics/RenderPass/MaterialUpload.h"
#include "Dune/Graphics/RenderPass/Outputs.h"

namespace Dune::Graphics
{
	MaterialUploadData* MaterialUpload::Create(Renderer& renderer)
	{
		MaterialUploadData* pData = new MaterialUploadData();
		pData->srv = renderer.GetSRVAllocator().Allocate();
		return pData;
	}

	void MaterialUpload::Setup(RenderGraphBuilder& builder, RenderPassContext& context, MaterialUploadData* pData)
	{
		const dVector<MaterialData>& materials = context.pFrameData->pResources->materials;
		dU32 materialCount = (dU32)materials.size();
		if (materialCount == 0)
			return;

		const dU32 materialsByteSize = materialCount * (dU32)sizeof(MaterialData);

		if (pData->buffer.GetByteSize() < materialsByteSize)
		{
			Renderer& renderer = *context.pRenderer;
			Frame& frame = renderer.GetCurrentFrame();
			RHI::Device& device = *renderer.GetDevice();
			if (pData->buffer.Get())
			{
				frame.buffersToRelease.push_back(pData->buffer);
				renderer.ReleaseSRV(pData->srv);
				pData->srv = renderer.GetSRVAllocator().Allocate();
			}
			pData->buffer.Initialize(device, { .debugName{ L"MaterialBuffer" }, .memory{ RHI::EBufferMemory::GPU }, .byteSize{ materialsByteSize } });
			device.CreateSRV(pData->srv, pData->buffer, { .elementCount = materialCount, .byteStride = sizeof(MaterialData) });

			if (!pData->handle.IsValid())
				pData->handle = renderer.GetRenderGraph().RegisterBuffer(&pData->buffer, RHI::EResourceState::Undefined);
			else
				renderer.GetRenderGraph().SetPhysicalResource(pData->handle, &pData->buffer, RHI::EResourceState::Undefined);
		}

		context.blackboard.Add<MaterialOutputs>({ .buffer = pData->handle, .bufferIndex = context.GetBindlessIndex(pData->srv) });
		builder.Write(pData->handle, RHI::EResourceState::CopyDest);

		builder.OnExecute([pData](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				const dVector<MaterialData>& materials = context.pFrameData->pResources->materials;
				const dU32 materialsByteSize = (dU32)(materials.size() * sizeof(MaterialData));

				StagingMemory staging = renderer.AllocateStaging(materialsByteSize, 16);
				memcpy(staging.pCpuAddress, materials.data(), materialsByteSize);
				frame.commandList.CopyBufferRegion(pData->buffer, 0, staging.buffer, staging.offset, materialsByteSize);
			});
	}

	void MaterialUpload::Destroy(Renderer& renderer, MaterialUploadData* pData)
	{
		renderer.GetSRVAllocator().Free(pData->srv);
		if (pData->buffer.Get())
			pData->buffer.Destroy();
		delete pData;
	}
}
