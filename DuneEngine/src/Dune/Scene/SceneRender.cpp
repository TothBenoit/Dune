#include "pch.h"
#include "Dune/Scene/SceneRender.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Component/Light.h"
#include "Dune/Component/Transform.h"
#include "Dune/Component/MeshInstance.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::SceneRender
{
	using namespace Graphics;

	void Extract(const Scene& scene, const ResourceManager& resources, FrameData& frameData)
	{
		frameData.lights.clear();
		scene.registry.view<const Light>().each([&](const Light& light) { frameData.lights.push_back(light); });

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
