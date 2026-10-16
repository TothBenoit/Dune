#include "pch.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"

namespace Dune::Graphics
{
	LightUploadData* LightUpload::Create(Renderer& renderer)
	{
		LightUploadData* pData = new LightUploadData();
		pData->srv = renderer.GetSRVHeap().Allocate();
		return pData;
	}

	void LightUpload::Setup(RenderGraphBuilder& builder, RenderPassContext& context, LightUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pVisibleSet)
			return;

		dU32 lightCount = pVisibleSet->lights.GetSize();
		if (lightCount == 0)
			return;

		Device& device = *renderer.GetDevice();
		Frame& frame = renderer.GetCurrentFrame();
		const dU32 lightByteSize = lightCount * (dU32)sizeof(LightData);

		if (pData->buffer.GetByteSize() < lightByteSize)
		{
			if (pData->buffer.Get())
				frame.buffersToRelease.push_back(pData->buffer);
			pData->buffer.Initialize(device, { .debugName{ L"LightBuffer" }, .memory{ EBufferMemory::GPU }, .byteSize{ lightByteSize } });
			device.CreateSRV(pData->srv, pData->buffer, { .elementCount = lightCount, .byteStride = sizeof(LightData) });
			device.CopyDescriptors(1, pData->srv.cpuAddress, context.GetGPUDescriptor(frame, pData->srv).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->handle == kInvalidResourceHandle)
				pData->handle = renderer.RegisterBuffer(&pData->buffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->handle, &pData->buffer, EResourceState::Undefined);
		}

		builder.Write(pData->handle, EResourceState::CopyDest);
		context.blackboard.Add<LightOutputs>({ .buffer = pData->handle, .bufferIndex = context.GetBindlessIndex(pData->srv), .count = lightCount });
	}

	void LightUpload::Execute(RenderPassContext& context, LightUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		const dSpan<const LightData> lights = context.blackboard.Get<VisibleSet>().lights;
		const dU32 lightByteSize = lights.GetSize() * (dU32)sizeof(LightData);

		StagingMemory staging = renderer.AllocateStaging(lightByteSize, 16);
		memcpy(staging.pCpuAddress, lights.GetData(), lightByteSize);
		frame.commandList.CopyBufferRegion(pData->buffer, 0, staging.buffer, staging.offset, lightByteSize);
	}

	void LightUpload::Destroy(Renderer& renderer, LightUploadData* pData)
	{
		renderer.GetSRVHeap().Free(pData->srv);
		if (pData->buffer.Get())
			pData->buffer.Destroy();
		delete pData;
	}
}
