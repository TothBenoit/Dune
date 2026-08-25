#include "pch.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	void DepthPrepass::Initialize(Device& device)
	{
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

		m_depthRS.Initialize(device,
			{ 
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(dMatrix4x4), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
				},
				.bAllowInputLayout = true,
			});

		m_depthPSO.Initialize(device,
			{
				.pVertexShader = &depthPrepassVS,
				.pRootSignature = &m_depthRS,
				.inputLayout =
				{
					VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
				},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.depthStencilFormat = EFormat::D32_FLOAT,
			}
		);

		depthPrepassVS.Destroy();
	}

	void DepthPrepass::Destroy()
	{
		m_depthPSO.Destroy();
		m_depthRS.Destroy();
	}

	void DepthPrepass::Render(FrameData& frameData, ResourceManager& resourceManager, CommandList& commandList, const dMatrix4x4& viewProjection)
	{
		commandList.SetGraphicsRootSignature(m_depthRS);
		commandList.SetPipelineState(m_depthPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		for( const RenderInstance& instance : frameData.instances )
		{
			InstanceData data;
			data.objectToWorld = instance.objectToWorld;

			commandList.PushGraphicsConstants(1, &data.objectToWorld, sizeof(InstanceData));
			Mesh& mesh = resourceManager.GetMesh(instance.data.meshIdx);
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
		}
	}
}
