#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RenderPass/MaterialUpload.h"
#include "Dune/Graphics/RenderPass/Outputs.h"

namespace Dune::Graphics
{
	MaterialUploadData* MaterialUpload::Create(Renderer& renderer)
	{
		MaterialUploadData* pData = new MaterialUploadData();
		pData->srv = renderer.GetSRVHeap().Allocate();
		return pData;
	}

	void MaterialUpload::Setup(RenderGraphBuilder& builder, RenderPassContext& context, MaterialUploadData* pData)
	{
		const dVector<MaterialData>& materials = context.pFrameData->materials;
		dU32 materialCount = (dU32)materials.size();
		if (materialCount == 0)
			return;

		const dU32 materialsByteSize = materialCount * (dU32)sizeof(MaterialData);

		if (pData->buffer.GetByteSize() < materialsByteSize)
		{
			Renderer& renderer = *context.pRenderer;
			Frame& frame = renderer.GetCurrentFrame();
			Device& device = *renderer.GetDevice();
			if (pData->buffer.Get())
				frame.buffersToRelease.push_back(pData->buffer);
			pData->buffer.Initialize(device, { .debugName{ L"MaterialBuffer" }, .memory{ EBufferMemory::GPU }, .byteSize{ materialsByteSize } });
			device.CreateSRV(pData->srv, pData->buffer, { .elementCount = materialCount, .byteStride = sizeof(MaterialData) });
			device.CopyDescriptors(1, pData->srv.cpuAddress, context.GetGPUDescriptor(frame, pData->srv).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->handle == kInvalidResourceHandle)
				pData->handle = renderer.RegisterBuffer(&pData->buffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->handle, &pData->buffer, EResourceState::Undefined);
		}

		context.blackboard.Add<MaterialOutputs>({ .buffer = pData->handle, .bufferIndex = context.GetBindlessIndex(pData->srv) });
		builder.Write(pData->handle, EResourceState::CopyDest);
	}

	void MaterialUpload::Execute(RenderPassContext& context, MaterialUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		const dVector<MaterialData>& materials = context.pFrameData->materials;
		const dU32 materialsByteSize = (dU32)(materials.size() * sizeof(MaterialData));

		StagingMemory staging = renderer.AllocateStaging(materialsByteSize, 16);
		memcpy(staging.pCpuAddress, materials.data(), materialsByteSize);
		frame.commandList.CopyBufferRegion(pData->buffer, 0, staging.buffer, staging.offset, materialsByteSize);
	}

	void MaterialUpload::Destroy(Renderer& renderer, MaterialUploadData* pData)
	{
		renderer.GetSRVHeap().Free(pData->srv);
		if (pData->buffer.Get())
			pData->buffer.Destroy();
		delete pData;
	}
}
