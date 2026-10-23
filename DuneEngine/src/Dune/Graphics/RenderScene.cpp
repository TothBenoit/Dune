#include "pch.h"
#include "Dune/Graphics/RenderScene.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::Graphics
{
	void RenderScene::Reset()
	{
		m_meshObjects.clear();
		m_frameData.lights.clear();
	}

	void RenderScene::AddMesh(const dMatrix4x4& objectToWorld, MeshHandle mesh, MaterialSetHandle materials)
	{
		m_meshObjects.push_back({ .objectToWorld = objectToWorld, .mesh = mesh, .materials = materials });
	}

	void RenderScene::AddLight(const LightData& light)
	{
		m_frameData.lights.push_back(light);
	}

	const FrameData& RenderScene::Build(const ResourceManager& resources, float deltaTime)
	{
		m_frameData.drawItems.clear();
		for (const MeshObject& object : m_meshObjects)
		{
			const Mesh& mesh = resources.GetMesh(object.mesh);
			const MaterialSet& materialSet = resources.GetMaterialSet(object.materials);
			Assert(materialSet.materials.size() >= mesh.GetMaterialSlotCount());
			for (const SubMesh& subMesh : mesh.GetSubMeshes())
			{
				const MaterialHandle materialHandle = materialSet.materials[subMesh.materialSlot];
				m_frameData.drawItems.push_back(
					{
						.objectToWorld = object.objectToWorld,
						.meshIdx = object.mesh.GetIndex(),
						.materialIdx = materialHandle.GetIndex(),
						.indexOffset = subMesh.indexOffset,
						.indexCount = subMesh.indexCount,
						.vertexOffset = subMesh.vertexOffset,
						.materialKey = resources.GetMaterial(materialHandle).GetKey(),
					});
			}
		}

		resources.FillFrameData(m_frameData);
		m_frameData.deltaTime = deltaTime;
		return m_frameData;
	}
}
