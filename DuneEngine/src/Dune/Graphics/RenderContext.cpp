#include "pch.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Scene/Scene.h"

namespace Dune::Graphics
{
	void RenderContext::Initialize()
	{
		m_device.Initialize();
		m_resourceManager.Initialize(m_device);
	}

	void RenderContext::Destroy()
	{
		m_resourceManager.Destroy();
		m_device.Destroy();
	}

	dMatrix4x4 ComputeShadowMatrix(Light& light)
	{
		dMatrix4x4 lightMatrix;
		if (light.IsPoint())
			DirectX::XMStoreFloat4x4(&lightMatrix, DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.f), 1.0f, 0.1f, light.range));
		else
		{
			dVec up{ 0.f, 1.f, 0.f, 0.f };
			dVec to{ DirectX::XMLoadFloat3(&light.direction) };
			dVec axis = DirectX::XMVector3Cross(up, to);
			if (DirectX::XMVector3Equal(axis, { 0.0f, 0.0f, 0.0f }))
				up = { 0.f, 0.f, 1.f, 0.f };

			if (light.IsSpot())
			{
				dVec eye{ light.position.x, light.position.y, light.position.z };
				dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
				dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(light.angle * 2.0f, 1.0f, 0.1f, light.range) };
				DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
			}
			else
			{
				float shadowWidth{ 4500.f }; // Hardcoded for sponza
				dVec eye{ 0.f, 0.f, 0.f };
				dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
				dMatrix projectionMatrix{ DirectX::XMMatrixOrthographicLH(shadowWidth, shadowWidth, -shadowWidth, shadowWidth) };
				DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
			}
		}
		return lightMatrix;
	}

	void FillLight(const Dune::Light& sceneLight, Light& light)
	{
		light.color = sceneLight.color;
		switch (sceneLight.type)
		{
		case ELightType::Directional:
			light.intensity = sceneLight.intensity;
			DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
			break;
		case ELightType::Point:
		{
			float lightSolidAngle = 4.0f * DirectX::XM_PI;
			float candelaIntensity = sceneLight.intensity / lightSolidAngle;
			light.intensity = candelaIntensity / (0.01f * 0.01f);
		}
		light.range = sceneLight.range;
		light.position = sceneLight.position;
		light.flags |= fIsPoint;
		break;
		case ELightType::Spot:
			light.range = sceneLight.range;
			light.position = sceneLight.position;
			DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
			light.angle = DirectX::XMScalarCos(sceneLight.angle);
			{
				float lightSolidAngle = 2.0f * DirectX::XM_PI * (1.0f - light.angle);
				float candelaIntensity = sceneLight.intensity / lightSolidAngle;
				light.intensity = candelaIntensity / (0.01f * 0.01f);
			}
			light.penumbra = 1.0f / (DirectX::XMScalarCos(sceneLight.angle * (1.0f - sceneLight.penumbra)) - light.angle);
			light.flags |= fIsSpot;
			break;
		}
		if (sceneLight.castShadow)
			light.flags |= fCastShadow;
	}

	void RenderContext::GatherFrameData(const Scene& scene, FrameData& frameData)
	{
		frameData.lights.allActive.clear();
		frameData.lights.shadowCasters.clear();
		frameData.drawItems.clear();

		ResourceManager& resourceManager = GetResourceManager();
		const BlockDescriptorHeap& sharedHeap = resourceManager.GetSRVHeap();
		frameData.sharedSRVHeapCPUAddress = sharedHeap.GetCPUAddress();
		frameData.sharedSRVHeapCapacity = sharedHeap.GetCapacity();

		scene.registry.view<const Dune::Light>().each([&](const Dune::Light& sceneLight)
			{
				if (sceneLight.intensity <= 0.0f)
					return;
				Light light{};
				FillLight(sceneLight, light);
				if (sceneLight.castShadow)
				{
					dU32 casterIndex = (dU32)frameData.lights.shadowCasters.size();
					light.shadowIndex = (dU32)frameData.lights.shadowCasters.size();
					frameData.lights.shadowCasters.push_back((dU32)frameData.lights.allActive.size());
					frameData.lights.shadowMatrices.push_back(ComputeShadowMatrix(light));
				}
				frameData.lights.allActive.push_back(light);
			});

		frameData.blendDrawCount = 0;
		scene.registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				dMatrix4x4 objectToWorld;
				DirectX::XMStoreFloat4x4(&objectToWorld,
					DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
					DirectX::XMMatrixRotationQuaternion(transform.rotation) *
					DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
				);

				Mesh& mesh = resourceManager.GetMesh(renderData.meshIdx);
				Assert(renderData.materialSlotCount == mesh.GetMaterialSlotCount());
				for (const SubMesh& subMesh : mesh.GetSubMeshes())
				{
					DrawItem& drawItem = frameData.drawItems.emplace_back();
					drawItem.objectToWorld = objectToWorld;
					drawItem.meshIdx = renderData.meshIdx;
					drawItem.materialIdx = resourceManager.GetMaterialID(renderData.materialSlotStart + subMesh.materialSlot);
					drawItem.indexOffset = subMesh.indexOffset;
					drawItem.indexCount = subMesh.indexCount;
					drawItem.vertexOffset = subMesh.vertexOffset;
					const Material& material = resourceManager.GetMaterial(drawItem.materialIdx);
					drawItem.materialVariant = material.GetVariant();
					frameData.blendDrawCount += material.alphaMode == EAlphaMode::Blend ? 1 : 0;
				}
			});

		const dVector<Material>& materials = resourceManager.GetMaterials();
		frameData.materials.clear();
		frameData.materials.reserve(materials.size());
		for (const Material& material : materials)
			frameData.materials.push_back(material.shaderData);

		dVector<Mesh>& meshes = resourceManager.GetMeshes();
		frameData.meshes.clear();
		frameData.meshes.reserve(meshes.size());
		for (Mesh& mesh : meshes)
		{
			Buffer& indexBuffer = mesh.GetIndexBuffer();
			Buffer& vertexBuffer = mesh.GetVertexBuffer();
			GPUMeshView& meshView = frameData.meshes.emplace_back();
			meshView.indicesGPUAddress = indexBuffer.GetGPUAddress();
			meshView.indicesByteSize = indexBuffer.GetByteSize();
			meshView.indicesAre32Bit = mesh.IsIndex32bits();
			meshView.verticesGPUAddress = vertexBuffer.GetGPUAddress();
			meshView.verticesByteSize = vertexBuffer.GetByteSize();
			meshView.verticesByteStride = mesh.GetVertexByteStride();
		}

		std::sort(frameData.drawItems.begin(), frameData.drawItems.end(),
			[](const DrawItem& a, const DrawItem& b)
			{
				return a.materialVariant < b.materialVariant;
			}
		);
	}

}
