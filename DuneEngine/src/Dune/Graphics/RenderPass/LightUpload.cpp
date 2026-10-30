#include "pch.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"

namespace Dune::Graphics
{
	LightUploadData* LightUpload::Create(Renderer& renderer)
	{
		return new LightUploadData();
	}

	void LightUpload::Setup(RenderPassContext& context, LightUploadData* pData)
	{
		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pVisibleSet)
			return;

		dU32 lightCount = pVisibleSet->lights.GetSize();
		if (lightCount == 0)
			return;

		RenderGraph& graph = context.GetRenderGraph();
		const dU32 lightByteSize = lightCount * (dU32)sizeof(LightData);

		if (!pData->handle.IsValid())
			pData->handle = graph.CreateBuffer({ .debugName{ L"LightBuffer" }, .memory{ RHI::EBufferMemory::GPU }, .byteSize{ lightByteSize }, .byteStride{ sizeof(LightData) } });
		else if (graph.GetBuffer(pData->handle).GetByteSize() < lightByteSize)
			graph.ResizeBuffer(pData->handle, lightByteSize);

		const RGBuffer lightBuffer = pData->handle;
		RenderGraphBuilder& builder = context.AddPass();
		builder.Write(lightBuffer, RHI::EResourceState::CopyDest);
		context.blackboard.Add<LightOutputs>({ .buffer = lightBuffer, .count = lightCount });

		builder.OnExecute([lightBuffer](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				const dSpan<const LightData> lights = context.blackboard.Get<VisibleSet>().lights;
				const dU32 lightByteSize = lights.GetSize() * (dU32)sizeof(LightData);

				StagingMemory staging = renderer.AllocateStaging(lightByteSize, 16);
				memcpy(staging.pCpuAddress, lights.GetData(), lightByteSize);
				frame.commandList.CopyBufferRegion(context.GetRenderGraph().GetBuffer(lightBuffer), 0, staging.buffer, staging.offset, lightByteSize);
			});
	}

	void LightUpload::Destroy(Renderer&, LightUploadData* pData)
	{
		delete pData;
	}
}
