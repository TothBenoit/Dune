#include "pch.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/Shaders/ShaderTypes.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Texture.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Scene/Scene.h"

namespace Dune::Graphics
{
	void Forward::Initialize(Device* pDevice)
	{
		m_pDevice = pDevice;
		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };

		Shader forwardVS;
		forwardVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = L"Shaders\\Forward.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		});

		Shader forwardPS;
		forwardPS.Initialize
		({
			.stage = EShaderStage::Pixel,
			.filePath = L"Shaders\\Forward.hlsl",
			.entryFunc = L"PSMain",
			.args = args,
			.argsCount = _countof(args),
		});

		m_rootSignature.Initialize(pDevice,
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

		m_pipeline.Initialize(pDevice,
			{
				.pVertexShader = &forwardVS,
				.pPixelShader = &forwardPS,
				.pRootSignature = &m_rootSignature,
				.inputLayout =
				{
					VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
					VertexInput {.pName = "NORMAL", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .bPerInstance = false },
					VertexInput {.pName = "TANGENT", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .bPerInstance = false },
					VertexInput {.pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 36, .bPerInstance = false }
				},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.renderTargetCount = 1,
				.renderTargetsFormat = { EFormat::R16G16B16A16_FLOAT },
				.depthStencilFormat = EFormat::D32_FLOAT,
			}
		);

		forwardVS.Destroy();
		forwardPS.Destroy();
	}

	void Forward::Destroy()
	{
		m_pipeline.Destroy();
		m_rootSignature.Destroy();
	}

	void Forward::Render(Scene& scene, DescriptorHeap& srvHeap, CommandList& commandList, ForwardGlobals& globals, dQueue<Descriptor>& descriptorsToRelease)
	{
		commandList.SetGraphicsRootSignature(m_rootSignature);
		commandList.SetPipelineState(m_pipeline);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &globals, sizeof(ForwardGlobals));

		const entt::registry& kRegistry = scene.registry;
		kRegistry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				Mesh& mesh = scene.meshes[renderData.meshIdx];
				MaterialData material = scene.materials[renderData.materialIdx];

				InstanceData instance;
				DirectX::XMStoreFloat4x4(&instance.modelMatrix,
					DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
					DirectX::XMMatrixRotationQuaternion(transform.rotation) *
					DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
				);

				Texture& albedoTexture = scene.textures[material.albedoIdx];
				Texture& normalTexture = scene.textures[material.normalIdx];
				Texture& roughnessMetalnessTexture = scene.textures[material.roughnessMetalnessIdx];
				Descriptor albedo = srvHeap.Allocate();
				m_pDevice->CreateSRV(albedo, albedoTexture);
				Descriptor normal = srvHeap.Allocate();
				m_pDevice->CreateSRV(normal, normalTexture);
				Descriptor roughnessMetalness = srvHeap.Allocate();
				m_pDevice->CreateSRV(roughnessMetalness, roughnessMetalnessTexture);
				material.albedoIdx = srvHeap.GetIndex(albedo);
				material.normalIdx = srvHeap.GetIndex(normal);
				material.roughnessMetalnessIdx = srvHeap.GetIndex(roughnessMetalness);

				commandList.PushGraphicsConstants(1, &instance, sizeof(InstanceData));
				commandList.PushGraphicsConstants(2, &material, sizeof(MaterialData));
				commandList.BindIndexBuffer(mesh.GetIndexBuffer());
				commandList.BindVertexBuffer(mesh.GetVertexBuffer());
				commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);

				descriptorsToRelease.push(albedo);
				descriptorsToRelease.push(normal);
				descriptorsToRelease.push(roughnessMetalness);
			});
	}
}