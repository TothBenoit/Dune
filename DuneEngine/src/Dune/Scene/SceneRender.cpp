#include "pch.h"
#include "Dune/Scene/SceneRender.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Component/Light.h"
#include "Dune/Component/Camera.h"
#include "Dune/Component/Transform.h"
#include "Dune/Component/MeshInstance.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::SceneRender
{
	using namespace Graphics;

	static LightData ExtractLight(const Light& sceneLight)
	{
		LightData light{};
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
			light.intensity = candelaIntensity;
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
				light.intensity = candelaIntensity;
			}
			light.penumbra = 1.0f / (DirectX::XMScalarCos(sceneLight.angle * (1.0f - sceneLight.penumbra)) - light.angle);
			light.flags |= fIsSpot;
			break;
		}
		if (sceneLight.castShadow)
			light.flags |= fCastShadow;
		return light;
	}

	ViewData ExtractView(const Camera& camera)
	{
		ViewData view;
		ComputeViewProjectionMatrix(camera, &view.view, &view.projection, &view.viewProjection);
		view.position = camera.position;
		return view;
	}

	void Extract(const Scene& scene, const ResourceManager& resources, FrameData& frameData)
	{
		frameData.lights.clear();
		scene.registry.view<const Light>().each([&](const Light& light)
			{
				if (light.intensity > 0.0f)
					frameData.lights.push_back(ExtractLight(light));
			});

		frameData.drawItems.clear();
		scene.registry.view<const Transform, const MeshInstance>().each([&](const Transform& transform, const MeshInstance& meshInstance)
			{
				dMatrix4x4 objectToWorld;
				DirectX::XMStoreFloat4x4(&objectToWorld,
					DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
					DirectX::XMMatrixRotationQuaternion(transform.rotation) *
					DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
				);

				const Mesh& mesh = resources.GetMesh(meshInstance.mesh);
				const MaterialSet& materialSet = resources.GetMaterialSet(meshInstance.materials);
				Assert(materialSet.materials.size() >= mesh.GetMaterialSlotCount());
				for (const SubMesh& subMesh : mesh.GetSubMeshes())
				{
					const MaterialHandle materialHandle = materialSet.materials[subMesh.materialSlot];
					frameData.drawItems.push_back(
						{
							.objectToWorld = objectToWorld,
							.meshIdx = meshInstance.mesh.GetIndex(),
							.materialIdx = materialHandle.GetIndex(),
							.indexOffset = subMesh.indexOffset,
							.indexCount = subMesh.indexCount,
							.vertexOffset = subMesh.vertexOffset,
							.materialKey = resources.GetMaterial(materialHandle).GetKey(),
						});
				}
			});
	}
}
