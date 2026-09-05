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
#include "Dune/Graphics/Material.h"
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

		pData->forwardRS.Initialize(device,
		{
			.layout =
			{
				{.type = EBindingType::Constant, .byteSize = sizeof(ForwardGlobals), .visibility = EShaderVisibility::All},
				{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
				{.type = EBindingType::Constant, .byteSize = sizeof(MaterialData), .visibility = EShaderVisibility::Pixel},
			},
			.allowInputLayout = true,
			.allowSRVHeapIndexing = true,
		});

		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };
		const wchar_t* maskedArgs[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug", L"-D", L"ALPHA_MASK"};

		Shader forwardVS;
		dWString shaderPath = StringUtils::ToWide(FileSystem::ResolvePath("engine://Shaders/Forward.hlsl"));
		ShaderDesc shaderDesc
		{
			.stage = EShaderStage::Vertex,
			.filePath = shaderPath.c_str(),
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		};
		forwardVS.Initialize(shaderDesc);

		Shader forwardPS[2];
		shaderDesc.stage = EShaderStage::Pixel;
		shaderDesc.entryFunc = L"PSMain";
		forwardPS[0].Initialize(shaderDesc);
		shaderDesc.args = maskedArgs;
		shaderDesc.argsCount = _countof(maskedArgs);
		forwardPS[1].Initialize(shaderDesc);

		for (dU32 variant = 0; variant < Material::kVariantCount; variant++)
		{
			EAlphaMode alphaMode = Material::GetAlphaMode(variant);
			dU32 shaderIdx = alphaMode == EAlphaMode::Mask ? 1 : 0;
			pData->forwardPSO[variant].Initialize(device,
				{
					.pVertexShader = &forwardVS,
					.pPixelShader = &forwardPS[shaderIdx],
					.pRootSignature = &pData->forwardRS,
					.inputLayout =
					{
						VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
						VertexInput {.pName = "NORMAL", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .isPerInstance = false },
						VertexInput {.pName = "TANGENT", .index = 0, .format = EFormat::R32G32B32A32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .isPerInstance = false },
						VertexInput {.pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false }
					},
					.rasterizerState = { .cullingMode = Material::IsDoubleSided(variant) ? ECullingMode::None : ECullingMode::Back },
					.depthStencilState = 
					{ 
						.depthFunc = alphaMode == EAlphaMode::Blend ? ECompFunc::LessEqual : ECompFunc::Equal, 
						.depthEnabled = true, 
						.depthWrite = false 
					},
					.renderTargetCount = 1,
					.renderTargetsFormat = { EFormat::R16G16B16A16_FLOAT },
					.renderTargetsBlend = { { .blendEnable = alphaMode == EAlphaMode::Blend } },
					.depthStencilFormat = EFormat::D32_FLOAT,
				}
			);
		}

		forwardVS.Destroy();
		for(Shader& ps : forwardPS)
			ps.Destroy();
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

		const dVector<Material>& materials = renderer.GetRenderContext()->GetResourceManager().GetMaterials();
		dVector<dU32>& materialDescriptorCache = pData->materialDescriptorCache;
		materialDescriptorCache.assign(materials.size(), dU32(-1));
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
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &globals, sizeof(ForwardGlobals));

		dU32 currentVariant = dU32(-1);
		for( const DrawItem& drawItem : context.pFrameData->drawItems )
		{
			dU32 variant = drawItem.materialVariant;
			if (currentVariant != variant)
			{
				commandList.SetPipelineState(pData->forwardPSO[variant]);
				currentVariant = variant;
			}

			const Material& material = resourceManager.GetMaterial(drawItem.materialIdx);
			MaterialData materialData = material.shaderData;

			dVector<dU32>& materialDescriptorCache = pData->materialDescriptorCache;
			dU32 materialSRVIdx = materialDescriptorCache[drawItem.materialIdx];
			if (materialSRVIdx == dU32(-1))
			{
				dU32 srvNeeded = 0;
				if (materialData.albedoIdx != dU32(-1))
					srvNeeded++;
				if (materialData.normalIdx != dU32(-1))
					srvNeeded++;
				if (materialData.roughnessMetalnessIdx != dU32(-1))
					srvNeeded++;
				if (srvNeeded > 0)
				{
					Descriptor materialSRV = srvHeap.Allocate(srvNeeded);
					materialSRVIdx = srvHeap.GetIndex(materialSRV);
					materialDescriptorCache[drawItem.materialIdx] = materialSRVIdx;
					dU32 srvIdx = materialSRVIdx;
					if (materialData.albedoIdx != dU32(-1))
						device.CreateSRV(srvHeap.GetDescriptorAt(srvIdx++), resourceManager.GetTexture(materialData.albedoIdx));
					if (materialData.normalIdx != dU32(-1))
						device.CreateSRV(srvHeap.GetDescriptorAt(srvIdx++), resourceManager.GetTexture(materialData.normalIdx));
					if (materialData.roughnessMetalnessIdx != dU32(-1))
						device.CreateSRV(srvHeap.GetDescriptorAt(srvIdx++), resourceManager.GetTexture(materialData.roughnessMetalnessIdx));
				}
			}

			dU32 srvIdx = materialSRVIdx;
			if (materialData.albedoIdx != dU32(-1))
				materialData.albedoIdx             = srvIdx++;
			if (materialData.normalIdx != dU32(-1))
				materialData.normalIdx             = srvIdx++;
			if (materialData.roughnessMetalnessIdx != dU32(-1))
				materialData.roughnessMetalnessIdx = srvIdx++;
			commandList.PushGraphicsConstants(2, &materialData, sizeof(MaterialData));

			InstanceData instanceData;
			instanceData.objectToWorld = drawItem.objectToWorld;
			commandList.PushGraphicsConstants(1, &instanceData, sizeof(InstanceData));

			Mesh& mesh = resourceManager.GetMesh(drawItem.meshIdx);
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
		}
	}

	void Forward::Destroy(Renderer&, ForwardData* pData)
	{
		for (PipelineState& pso : pData->forwardPSO)
			pso.Destroy();
		pData->forwardRS.Destroy();
		delete pData;
	}
}
