#include "pch.h"
#include "Dune/Graphics/RenderScene.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::Graphics
{
	template<typename T, typename Tag>
	static bool ApplyChanges(RenderSceneChanges<T, Tag>& changes, ResourcePool<T, Tag>& pool, HandleAllocator<Tag>& handles)
	{
		const bool hasChanges = !changes.creates.empty() || !changes.sets.empty() || !changes.destroys.empty();
		for (typename RenderSceneChanges<T, Tag>::Write& write : changes.creates)
			pool.Insert(write.handle, std::move(write.item));
		for (const typename RenderSceneChanges<T, Tag>::Write& write : changes.sets)
			pool.Get(write.handle) = write.item;
		for (const Handle<Tag> handle : changes.destroys)
		{
			pool.Remove(handle);
			handles.Free(handle);
		}
		changes.creates.clear();
		changes.sets.clear();
		changes.destroys.clear();
		return hasChanges;
	}

	void RenderScene::Initialize(dU64 frameNumber)
	{
		m_objects.Initialize(m_objectHandles);
		m_lights.Initialize(m_lightHandles);
		m_appliedFrame = frameNumber > 0 ? frameNumber - 1 : 0;
	}

	void RenderScene::Destroy()
	{
		m_objects.Destroy();
		m_objectHandles.Destroy();
		m_lights.Destroy();
		m_lightHandles.Destroy();
		for (RenderSceneChangelog& changelog : m_changelogs)
			changelog = {};
		m_changelogIndex = 0;
		m_appliedFrame = 0;
		m_drawItems.clear();
		m_lightsData.clear();
		m_drawItemsResourcesVersion = 0;
		m_objectsDirty = false;
		m_lightsDirty = false;
	}

	void RenderScene::BeginChangelog(dU64 frameNumber)
	{
		if (frameNumber > kFrameRingSize)
			ApplyChangelogs(frameNumber - kFrameRingSize);
		m_changelogIndex = (dU32)(frameNumber % kFrameRingSize);
	}

	void RenderScene::ApplyChangelogs(dU64 frameNumber)
	{
		if (frameNumber <= m_appliedFrame)
			return;

		const dU64 lastFrame = std::min(frameNumber, m_appliedFrame + kFrameRingSize);
		for (dU64 frame = m_appliedFrame + 1; frame <= lastFrame; frame++)
		{
			RenderSceneChangelog& changelog = m_changelogs[frame % kFrameRingSize];
			m_objectsDirty |= ApplyChanges(changelog.objects, m_objects, m_objectHandles);
			m_lightsDirty |= ApplyChanges(changelog.lights, m_lights, m_lightHandles);
		}
		m_appliedFrame = frameNumber;
	}

	RenderObjectHandle RenderScene::CreateObject(const RenderObject& object)
	{
		const RenderObjectHandle handle = m_objectHandles.Allocate();
		m_changelogs[m_changelogIndex].objects.creates.push_back({ handle, object });
		return handle;
	}

	void RenderScene::SetObject(RenderObjectHandle handle, const RenderObject& object)
	{
		m_changelogs[m_changelogIndex].objects.sets.push_back({ handle, object });
	}

	void RenderScene::DestroyObject(RenderObjectHandle handle)
	{
		m_changelogs[m_changelogIndex].objects.destroys.push_back(handle);
	}

	RenderLightHandle RenderScene::CreateLight(const LightData& light)
	{
		const RenderLightHandle handle = m_lightHandles.Allocate();
		m_changelogs[m_changelogIndex].lights.creates.push_back({ handle, light });
		return handle;
	}

	void RenderScene::SetLight(RenderLightHandle handle, const LightData& light)
	{
		m_changelogs[m_changelogIndex].lights.sets.push_back({ handle, light });
	}

	void RenderScene::DestroyLight(RenderLightHandle handle)
	{
		m_changelogs[m_changelogIndex].lights.destroys.push_back(handle);
	}

	const dVector<DrawItem>& RenderScene::GetDrawItems(const ResourceManager& resources)
	{
		if (m_objectsDirty || m_drawItemsResourcesVersion != resources.GetVersion())
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
			m_drawItemsResourcesVersion = resources.GetVersion();
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
