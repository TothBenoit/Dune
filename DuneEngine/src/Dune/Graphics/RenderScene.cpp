#include "pch.h"
#include "Dune/Graphics/RenderScene.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::Graphics
{
	void RenderScene::Initialize()
	{
		m_objects.Initialize(m_objectHandles);
		m_lights.Initialize(m_lightHandles);
	}

	void RenderScene::Destroy()
	{
		m_objects.Destroy();
		m_objectHandles.Destroy();
		m_lights.Destroy();
		m_lightHandles.Destroy();
		m_drawItems.clear();
		m_lightsData.clear();
		m_drawItemsResourcesVersion = 0;
		m_objectsDirty = false;
		m_lightsDirty = false;
	}

	RenderObjectHandle RenderScene::CreateObject(const RenderObject& object)
	{
		const RenderObjectHandle handle = m_objectHandles.Allocate();
		m_objects.Insert(handle, RenderObject{ object });
		m_objectsDirty = true;
		return handle;
	}

	void RenderScene::SetObject(RenderObjectHandle handle, const RenderObject& object)
	{
		m_objects.Get(handle) = object;
		m_objectsDirty = true;
	}

	void RenderScene::DestroyObject(RenderObjectHandle handle)
	{
		m_objects.Remove(handle);
		m_objectHandles.Free(handle);
		m_objectsDirty = true;
	}

	RenderLightHandle RenderScene::CreateLight(const LightData& light)
	{
		const RenderLightHandle handle = m_lightHandles.Allocate();
		m_lights.Insert(handle, LightData{ light });
		m_lightsDirty = true;
		return handle;
	}

	void RenderScene::SetLight(RenderLightHandle handle, const LightData& light)
	{
		m_lights.Get(handle) = light;
		m_lightsDirty = true;
	}

	void RenderScene::DestroyLight(RenderLightHandle handle)
	{
		m_lights.Remove(handle);
		m_lightHandles.Free(handle);
		m_lightsDirty = true;
	}

	const dVector<DrawItem>& RenderScene::GetDrawItems(const ResourceManager& resources, dU32 resourcesVersion)
	{
		if (m_objectsDirty || m_drawItemsResourcesVersion != resourcesVersion)
		{
			m_drawItems.clear();
			for (dU32 index = 0; index < m_objects.GetCapacity(); index++)
			{
				if (!m_objects.IsAliveAt(index))
					continue;

				const RenderObject& object = m_objects.GetAt(index);
				const Mesh& mesh = resources.GetMesh(object.mesh);
				const MaterialSet& materialSet = resources.GetMaterialSet(object.materials);
				Assert(materialSet.materials.size() >= mesh.GetMaterialSlotCount());
				for (const SubMesh& subMesh : mesh.GetSubMeshes())
				{
					const MaterialHandle materialHandle = materialSet.materials[subMesh.materialSlot];
					m_drawItems.push_back(
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
			m_drawItemsResourcesVersion = resourcesVersion;
			m_objectsDirty = false;
		}
		return m_drawItems;
	}

	const dVector<LightData>& RenderScene::GetLightsData()
	{
		if (m_lightsDirty)
		{
			m_lightsData.clear();
			for (dU32 index = 0; index < m_lights.GetCapacity(); index++)
			{
				if (m_lights.IsAliveAt(index))
					m_lightsData.push_back(m_lights.GetAt(index));
			}
			m_lightsDirty = false;
		}
		return m_lightsData;
	}
}
