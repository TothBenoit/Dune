#include "pch.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	LightUploadData* LightUpload::Create(Renderer& renderer)
	{
		LightUploadData* pData = new LightUploadData();
		pData->srv = renderer.GetSRVHeap().Allocate();
		pData->srvIndex = renderer.GetSRVHeap().GetIndex(pData->srv);
		return pData;
	}

	void LightUpload::Setup(RenderGraphBuilder& builder, RenderPassContext& context, LightUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		dVector<Light>& lights = context.pFrameData->lights.allActive;

		pData->lightCount = (dU32)lights.size();
		if (pData->lightCount == 0)
			return;

		Device& device = renderer.GetRenderContext()->GetDevice();
		Frame& frame = renderer.GetCurrentFrame();
		const dU32 lightByteSize = pData->lightCount * (dU32)sizeof(Light);

		if (pData->buffer.GetByteSize() < lightByteSize)
		{
			if (pData->buffer.Get())
				frame.buffersToRelease.push(pData->buffer);
			pData->buffer.Initialize(device,
				{
					.debugName{ L"LightBuffer" },
					.memory{ EBufferMemory::GPU },
					.byteSize{ lightByteSize },
					.initialState{ EResourceState::Undefined }
				});
			device.CreateSRV(pData->srv, pData->buffer, { .elementCount = pData->lightCount, .byteStride = sizeof(Light) });
			device.CopyDescriptors(1, pData->srv.cpuAddress, frame.srvHeap.GetDescriptorAt(pData->srvIndex).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->handle == kInvalidResourceHandle)
				pData->handle = renderer.RegisterBuffer(&pData->buffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->handle, &pData->buffer, EResourceState::Undefined);
		}

		builder.Write(pData->handle, EResourceState::CopyDest);
	}

	void LightUpload::Execute(RenderPassContext& context, LightUploadData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Device& device = renderer.GetRenderContext()->GetDevice();
		Frame& frame = renderer.GetCurrentFrame();
		dVector<Light>& lights = context.pFrameData->lights.allActive;
		const dU32 lightByteSize = pData->lightCount * (dU32)sizeof(Light);

		Buffer uploadBuffer{};
		uploadBuffer.Initialize(device,
			{
				.debugName{ L"LightUploadBuffer" },
				.memory{ EBufferMemory::CPU },
				.byteSize{ lightByteSize },
				.initialState{ EResourceState::Undefined }
			});

		void* pMappedData{ nullptr };
		uploadBuffer.Map(0, lightByteSize, &pMappedData);
		memcpy(pMappedData, lights.data(), lightByteSize);
		uploadBuffer.Unmap(0, lightByteSize);

		frame.commandList.CopyBufferRegion(pData->buffer, 0, uploadBuffer, 0, lightByteSize);
		frame.buffersToRelease.push(uploadBuffer);
	}

	void LightUpload::Destroy(Renderer& renderer, LightUploadData* pData)
	{
		renderer.GetSRVHeap().Free(pData->srv);
		if (pData->buffer.Get())
			pData->buffer.Destroy();
		delete pData;
	}
}
