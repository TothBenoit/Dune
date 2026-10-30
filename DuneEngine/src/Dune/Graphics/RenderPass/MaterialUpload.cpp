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
		return new MaterialUploadData();
	}

	void MaterialUpload::Setup(RenderPassContext& context, MaterialUploadData* pData)
	{
		const dVector<MaterialData>& materials = context.pFrameData->pResources->materials;
		dU32 materialCount = (dU32)materials.size();
		if (materialCount == 0)
			return;

		const dU32 materialsByteSize = materialCount * (dU32)sizeof(MaterialData);

		RenderGraph& graph = context.GetRenderGraph();
		if (!pData->handle.IsValid())
			pData->handle = graph.CreateBuffer({ .debugName{ L"MaterialBuffer" }, .memory{ RHI::EBufferMemory::GPU }, .byteSize{ materialsByteSize }, .byteStride{ sizeof(MaterialData) } });
		else if (graph.GetBuffer(pData->handle).GetByteSize() < materialsByteSize)
			graph.ResizeBuffer(pData->handle, materialsByteSize);

		const RGBuffer materialBuffer = pData->handle;
		context.blackboard.Add<MaterialOutputs>({ .buffer = materialBuffer });
		RenderGraphBuilder& builder = context.AddPass();
		builder.Write(materialBuffer, RHI::EResourceState::CopyDest);

		builder.OnExecute([materialBuffer](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				const dVector<MaterialData>& materials = context.pFrameData->pResources->materials;
				const dU32 materialsByteSize = (dU32)(materials.size() * sizeof(MaterialData));

				StagingMemory staging = renderer.AllocateStaging(materialsByteSize, 16);
				memcpy(staging.pCpuAddress, materials.data(), materialsByteSize);
				frame.commandList.CopyBufferRegion(context.GetRenderGraph().GetBuffer(materialBuffer), 0, staging.buffer, staging.offset, materialsByteSize);
			});
	}

	void MaterialUpload::Destroy(Renderer&, MaterialUploadData* pData)
	{
		delete pData;
	}
}
