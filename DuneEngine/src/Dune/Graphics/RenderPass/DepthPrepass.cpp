#include "pch.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	DepthPrepassData* DepthPrepass::Create(Renderer& renderer)
	{
		Device& device = renderer.GetRenderContext()->GetDevice();
		DepthPrepassData* pData = new DepthPrepassData();

		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };
		const wchar_t* maskedArgs[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug", L"-D", L"ALPHA_MASK" };

		dWString shaderPath = StringUtils::ToWide(FileSystem::ResolvePath("engine://Shaders/DepthOnly.hlsl"));
		ShaderDesc shaderDesc
		{
			.stage = EShaderStage::Vertex,
			.filePath = shaderPath.c_str(),
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		};

		Shader depthVS[2];
		depthVS[0].Initialize(shaderDesc);
		shaderDesc.args = maskedArgs;
		shaderDesc.argsCount = _countof(maskedArgs);
		depthVS[1].Initialize(shaderDesc);

		Shader depthMaskedPS;
		shaderDesc.stage = EShaderStage::Pixel;
		shaderDesc.entryFunc = L"PSMain";
		depthMaskedPS.Initialize(shaderDesc);

		pData->depthRS.Initialize(device,
		{
			.layout =
			{
				{.type = EBindingType::Constant, .byteSize = sizeof(dMatrix4x4), .visibility = EShaderVisibility::All},
				{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
				{.type = EBindingType::Constant, .byteSize = sizeof(MaterialData), .visibility = EShaderVisibility::Pixel},
			},
			.allowInputLayout = true,
			.allowSRVHeapIndexing = true,
		});

		VertexInput maskedVertexInputs[]
		{
			VertexInput{.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
			VertexInput{.pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false },
		};
		
		VertexInput vertexInputs[]
		{
			VertexInput{.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
		};

		for (dU32 variant = 0; variant < Material::kDepthVariantCount; variant++)
		{
			bool isMasked = Material::GetAlphaMode(variant) == EAlphaMode::Mask;
			dSpan<VertexInput> inputLayout = isMasked ? dSpan<VertexInput>(maskedVertexInputs) : dSpan<VertexInput>(vertexInputs);
			pData->depthPSO[variant].Initialize(device,
			{
				.pVertexShader = &depthVS[isMasked ? 1 : 0],
				.pPixelShader = isMasked ? &depthMaskedPS : nullptr,
				.pRootSignature = &pData->depthRS,
				.inputLayout = inputLayout,
				.rasterizerState = {.cullingMode = Material::IsDoubleSided(variant) ? ECullingMode::None : ECullingMode::Back },
				.depthStencilState = {.depthEnabled = true, .depthWrite = true },
				.depthStencilFormat = EFormat::D32_FLOAT,
			});
		}

		depthMaskedPS.Destroy();
		for (Shader& vs : depthVS)
			vs.Destroy();

		return pData;
	}

	void DepthPrepass::Setup(RenderGraphBuilder& builder, RenderPassContext& context, DepthPrepassData* pData)
	{
		builder.Write(context.pRenderer->GetDepthBufferHandle(), EResourceState::DepthStencil);
	}

	void DepthPrepass::Execute(RenderPassContext& context, DepthPrepassData* pData)
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
		Viewport viewport{ 0.0, 0.0, (float)pWindow->GetWidth(), (float)pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, pWindow->GetWidth(), pWindow->GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

		dMatrix4x4 viewProjection;
		ComputeViewProjectionMatrix(*context.pCamera, nullptr, nullptr, &viewProjection);

		commandList.SetGraphicsRootSignature(pData->depthRS);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		FrameData& frameData = *context.pFrameData;

		dU32 currentVariant = dU32(-1);
		for( dU32 drawIdx = 0; drawIdx < (dU32)frameData.drawItems.size() - frameData.blendingMaterialCount; drawIdx++  )
		{
			const DrawItem& drawItem = frameData.drawItems[drawIdx];
			Assert(drawItem.materialVariant < Material::kDepthVariantCount);
			const Material& material = resourceManager.GetMaterial(drawItem.materialIdx);
			Assert(material.alphaMode != EAlphaMode::Blend);
			if (currentVariant != drawItem.materialVariant)
			{
				currentVariant = drawItem.materialVariant;
				commandList.SetPipelineState(pData->depthPSO[currentVariant]);
			}

			if (material.alphaMode == EAlphaMode::Mask)
			{
				MaterialData materialData = material.shaderData;
				if (materialData.albedoIdx != dU32(-1))
				{
					Texture& albedoTexture = resourceManager.GetTexture(materialData.albedoIdx);
					Descriptor albedo = srvHeap.Allocate(1);
					device.CreateSRV(albedo, albedoTexture);
					materialData.albedoIdx = srvHeap.GetIndex(albedo);
				}
				commandList.PushGraphicsConstants(2, &materialData, sizeof(MaterialData));
			}

			InstanceData instanceData;
			instanceData.objectToWorld = drawItem.objectToWorld;
			commandList.PushGraphicsConstants(1, &instanceData, sizeof(InstanceData));
			Mesh& mesh = resourceManager.GetMesh(drawItem.meshIdx);
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
		}
	}

	void DepthPrepass::Destroy(Renderer&, DepthPrepassData* pData)
	{
		for (PipelineState& pso : pData->depthPSO)
			pso.Destroy();
		pData->depthRS.Destroy();
		delete pData;
	}
}
