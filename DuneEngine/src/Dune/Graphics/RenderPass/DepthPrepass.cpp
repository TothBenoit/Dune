#include "pch.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"
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

		Shader depthPrepassVS;
		depthPrepassVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = L"Shaders\\DepthOnly.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		});

		pData->depthRS.Initialize(device,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(dMatrix4x4), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
				},
				.bAllowInputLayout = true,
			});

		pData->depthPSO.Initialize(device,
			{
				.pVertexShader = &depthPrepassVS,
				.pRootSignature = &pData->depthRS,
				.inputLayout =
				{
					VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
				},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.depthStencilFormat = EFormat::D32_FLOAT,
			}
		);

		depthPrepassVS.Destroy();

		return pData;
	}

	void DepthPrepass::Setup(RenderGraphBuilder& builder, RenderPassContext& context, DepthPrepassData* pData)
	{
		builder.Write(context.pRenderer->GetDepthBufferHandle(), EResourceState::DepthStencil);
	}

	void DepthPrepass::Execute(RenderPassContext& context, DepthPrepassData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		CommandList& commandList = renderer.GetCurrentFrame().commandList;
		ResourceManager& resourceManager = renderer.GetRenderContext()->GetResourceManager();
		Window* pWindow = renderer.GetWindow();

		Descriptor dsv = renderer.GetDepthBufferDSV();
		commandList.ClearDepthBuffer(dsv, renderer.GetDepthBuffer().GetClearValue()[0], 0);

		Viewport viewport{ 0.0, 0.0, (float)pWindow->GetWidth(), (float)pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, pWindow->GetWidth(), pWindow->GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

		dMatrix4x4 viewProjection;
		ComputeViewProjectionMatrix(*context.pCamera, nullptr, nullptr, &viewProjection);

		commandList.SetGraphicsRootSignature(pData->depthRS);
		commandList.SetPipelineState(pData->depthPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		for( const DrawItem& drawItem : context.pFrameData->drawItems )
		{
			InstanceData data;
			data.objectToWorld = drawItem.objectToWorld;

			commandList.PushGraphicsConstants(1, &data.objectToWorld, sizeof(InstanceData));
			Mesh& mesh = resourceManager.GetMesh(drawItem.data.meshIdx);
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
		}
	}

	void DepthPrepass::Destroy(Renderer&, DepthPrepassData* pData)
	{
		pData->depthPSO.Destroy();
		pData->depthRS.Destroy();
		delete pData;
	}
}
