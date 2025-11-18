#include "pch.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/Shaders/ShaderTypes.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	void Shadow::Initialize(Device* pDevice)
	{
		m_pDevice = pDevice;
		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };

		Shader shadowVS;
		shadowVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = L"Shaders\\DepthOnly.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
			});

		m_rootSignature.Initialize(pDevice,
			{
				.layout =
				{
					{.type = EBindingType::Uniform, .byteSize = sizeof(dMatrix4x4), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Uniform, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
				},
				.bAllowInputLayout = true,
			});

		m_pipeline.Initialize(pDevice,
			{
				.pVertexShader = &shadowVS,

				.pRootSignature = &m_rootSignature,
				.inputLayout =
				{
					VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
				},
				.rasterizerState = {.depthBias = 10, .slopeScaledDepthBias = 4, .bDepthClipEnable = false},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.depthStencilFormat = EFormat::D32_FLOAT,
			}
			);
		shadowVS.Destroy();
	}

	void Shadow::Destroy()
	{
		m_pipeline.Destroy();
		m_rootSignature.Destroy();
	}

	void Shadow::Render(Scene& scene, CommandList& commandList, const dMatrix4x4& viewProjection)
	{
		commandList.SetGraphicsRootSignature(m_rootSignature);
		commandList.SetPipelineState(m_pipeline);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		const entt::registry& kRegistry = scene.registry;
		kRegistry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				InstanceData instance;
				DirectX::XMStoreFloat4x4(&instance.modelMatrix,
					DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
					DirectX::XMMatrixRotationQuaternion(transform.rotation) *
					DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
				);

				commandList.PushGraphicsConstants(1, &instance.modelMatrix, sizeof(InstanceData));
				Mesh& mesh = scene.meshes[renderData.meshIdx];
				commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
				commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
				commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
			});
	}
}