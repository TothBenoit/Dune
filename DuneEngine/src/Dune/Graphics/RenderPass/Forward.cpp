#include "pch.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Texture.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	ForwardData* Forward::Create(Renderer& renderer)
	{
		Device& device = renderer.GetRenderContext()->GetDevice();
		ForwardData* pData = new ForwardData();

		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };

		Shader forwardVS;
		dWString shaderPath = StringUtils::ToWide(FileSystem::ResolvePath("engine://Shaders/Forward.hlsl"));
		forwardVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = shaderPath.c_str(),
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		});

		Shader forwardPS;
		forwardPS.Initialize
		({
			.stage = EShaderStage::Pixel,
			.filePath = shaderPath.c_str(),
			.entryFunc = L"PSMain",
			.args = args,
			.argsCount = _countof(args),
		});

		pData->forwardRS.Initialize(device,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(ForwardGlobals), .visibility = EShaderVisibility::All},
					{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Constant, .byteSize = sizeof(MaterialData), .visibility = EShaderVisibility::Pixel},
				},
				.bAllowInputLayout = true,
				.bAllowSRVHeapIndexing = true,
			});

		pData->forwardPSO.Initialize(device,
			{
				.pVertexShader = &forwardVS,
				.pPixelShader = &forwardPS,
				.pRootSignature = &pData->forwardRS,
				.inputLayout =
				{
					VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
					VertexInput {.pName = "NORMAL", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .bPerInstance = false },
					VertexInput {.pName = "TANGENT", .index = 0, .format = EFormat::R32G32B32A32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .bPerInstance = false },
					VertexInput {.pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .bPerInstance = false }
				},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.renderTargetCount = 1,
				.renderTargetsFormat = { EFormat::R16G16B16A16_FLOAT },
				.depthStencilFormat = EFormat::D32_FLOAT,
			}
		);

		forwardVS.Destroy();
		forwardPS.Destroy();

		return pData;
	}

	void Forward::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ForwardData* pData)
	{
		Renderer& renderer = *context.pRenderer;

		builder.Write(renderer.GetHDRTargetHandle(), EResourceState::RenderTarget);
		builder.Write(renderer.GetDepthBufferHandle(), EResourceState::DepthStencil);

		ShadowData* pShadowData = renderer.Get<Shadow>();
		for (ResourceHandle handle : pShadowData->activeHandles)
			builder.Read(handle, EResourceState::ShaderResource);
		if (!pShadowData->activeHandles.empty())
			builder.Read(pShadowData->matricesHandle, EResourceState::ShaderResource);

		LightUploadData* pLightData = renderer.Get<LightUpload>();
		if (pLightData->lightCount > 0)
			builder.Read(pLightData->handle, EResourceState::ShaderResource);
	}

	void Forward::Execute(RenderPassContext& context, ForwardData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		RenderContext* pRenderContext = renderer.GetRenderContext();
		ResourceManager& resourceManager = pRenderContext->GetResourceManager();
		Device& device = pRenderContext->GetDevice();
		Window* pWindow = renderer.GetWindow();

		Descriptor dsv = renderer.GetDepthBufferDSV();
		commandList.ClearRenderTargetView(frame.hdrTargetRTV, frame.hdrTarget.GetClearValue());

		Viewport viewport{ 0.0, 0.0, (float)pWindow->GetWidth(), (float)pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, pWindow->GetWidth(), pWindow->GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetRenderTarget(&frame.hdrTargetRTV.cpuAddress, 1, &dsv.cpuAddress);

		ForwardGlobals globals;
		ComputeViewProjectionMatrix(*context.pCamera, nullptr, nullptr, &globals.viewProjectionMatrix);
		globals.cameraPosition = context.pCamera->position;
		globals.lightCount = renderer.Get<LightUpload>()->lightCount;
		globals.lightBufferIndex = renderer.Get<LightUpload>()->srvIndex;
		globals.lightMatricesIndex = renderer.Get<Shadow>()->matricesSRVIndex;

		commandList.SetGraphicsRootSignature(pData->forwardRS);
		commandList.SetPipelineState(pData->forwardPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &globals, sizeof(ForwardGlobals));

		for( const DrawItem& drawItem : context.pFrameData->drawItems )
		{
			Mesh& mesh = resourceManager.GetMesh(drawItem.meshIdx);
			MaterialData material = resourceManager.GetMaterial(drawItem.materialIdx);

			if (material.albedoIdx != dU32(-1))
			{
				Texture& albedoTexture = resourceManager.GetTexture(material.albedoIdx);
				Descriptor albedo = srvHeap.Allocate(1);
				device.CreateSRV(albedo, albedoTexture);
				material.albedoIdx = srvHeap.GetIndex(albedo);
			}

			if (material.normalIdx != dU32(-1))
			{
				Texture& normalTexture = resourceManager.GetTexture(material.normalIdx);
				Descriptor normal = srvHeap.Allocate(1);
				device.CreateSRV(normal, normalTexture);
				material.normalIdx = srvHeap.GetIndex(normal);
			}

			if (material.roughnessMetalnessIdx != dU32(-1))
			{
				Texture& roughnessMetalnessTexture = resourceManager.GetTexture(material.roughnessMetalnessIdx);
				Descriptor roughnessMetalness = srvHeap.Allocate(1);
				device.CreateSRV(roughnessMetalness, roughnessMetalnessTexture);
				material.roughnessMetalnessIdx = srvHeap.GetIndex(roughnessMetalness);
			}

			InstanceData data;
			data.objectToWorld = drawItem.objectToWorld;
			commandList.PushGraphicsConstants(1, &data, sizeof(InstanceData));
			commandList.PushGraphicsConstants(2, &material, sizeof(MaterialData));
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
		}
	}

	void Forward::Destroy(Renderer&, ForwardData* pData)
	{
		pData->forwardPSO.Destroy();
		pData->forwardRS.Destroy();
		delete pData;
	}
}
