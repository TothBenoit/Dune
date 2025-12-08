#include "pch.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"
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

		m_forwardRS.Initialize(pDevice,
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

		m_forwardPSO.Initialize(pDevice,
			{
				.pVertexShader = &forwardVS,
				.pPixelShader = &forwardPS,
				.pRootSignature = &m_forwardRS,
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
		m_forwardPSO.Destroy();
		m_forwardRS.Destroy();
	}

	void Forward::Render(Scene& scene, ScratchDescriptorHeap& srvHeap, CommandList& commandList, ForwardGlobals& globals)
	{
		commandList.SetGraphicsRootSignature(m_forwardRS);
		commandList.SetPipelineState(m_forwardPSO);
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

				if (material.albedoIdx != dU32(-1))
				{
					Texture& albedoTexture = scene.textures[material.albedoIdx];
					Descriptor albedo = srvHeap.Allocate(1);
					m_pDevice->CreateSRV(albedo, albedoTexture);
					material.albedoIdx = srvHeap.GetIndex(albedo);
				}

				if (material.normalIdx != dU32(-1))
				{
					Texture& normalTexture = scene.textures[material.normalIdx];
					Descriptor normal = srvHeap.Allocate(1);
					m_pDevice->CreateSRV(normal, normalTexture);
					material.normalIdx = srvHeap.GetIndex(normal);
				}

				if (material.roughnessMetalnessIdx != dU32(-1))
				{
					Texture& roughnessMetalnessTexture = scene.textures[material.roughnessMetalnessIdx];
					Descriptor roughnessMetalness = srvHeap.Allocate(1);
					m_pDevice->CreateSRV(roughnessMetalness, roughnessMetalnessTexture);
					material.roughnessMetalnessIdx = srvHeap.GetIndex(roughnessMetalness);
				}

				commandList.PushGraphicsConstants(1, &instance, sizeof(InstanceData));
				commandList.PushGraphicsConstants(2, &material, sizeof(MaterialData));
				commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
				commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
				commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
			});
	}
}