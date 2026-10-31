#pragma once

#include "Dune/Graphics/FrameData.h"
#include "Dune/Core/FrameContext.h"
#include "Dune/Core/HandleTypes.h"
#include "Dune/Core/HandleAllocator.h"
#include "Dune/Core/ResourcePool.h"

namespace Dune::Graphics
{
	class ResourceManager;

	struct RenderObject
	{
		dMatrix4x4        objectToWorld;
		MeshHandle        mesh;
		MaterialSetHandle materials;
	};

	template<typename T, typename Tag>
	struct RenderSceneChanges
	{
		struct Write
		{
			Handle<Tag> handle;
			T           item;
		};

		dVector<Write>       creates;
		dVector<Write>       sets;
		dVector<Handle<Tag>> destroys;
	};

	struct RenderSceneChangelog
	{
		RenderSceneChanges<RenderObject, RenderObject> objects;
		RenderSceneChanges<LightData, RenderLight>     lights;
	};

	class RenderScene
	{
	public:
		void Initialize(dU64 frameNumber);
		void Destroy();

		void BeginChangelog(dU64 frameNumber);
		void ApplyChangelogs(dU64 frameNumber);
		[[nodiscard]] dU64 GetAppliedFrame() const { return m_appliedFrame; }

		[[nodiscard]] RenderObjectHandle CreateObject(const RenderObject& object);
		void SetObject(RenderObjectHandle handle, const RenderObject& object);
		void DestroyObject(RenderObjectHandle handle);

		[[nodiscard]] RenderLightHandle CreateLight(const LightData& light);
		void SetLight(RenderLightHandle handle, const LightData& light);
		void DestroyLight(RenderLightHandle handle);

		[[nodiscard]] const RenderObject& GetObject(RenderObjectHandle handle) const { return m_objects.Get(handle); }
		[[nodiscard]] const LightData& GetLight(RenderLightHandle handle) const { return m_lights.Get(handle); }

		[[nodiscard]] const dVector<DrawItem>& GetDrawItems(const ResourceManager& resources);
		[[nodiscard]] const dVector<LightData>& GetLightsData();

	private:
		HandleAllocator<RenderObject>         m_objectHandles;
		ResourcePool<RenderObject>            m_objects;
		HandleAllocator<RenderLight>          m_lightHandles;
		ResourcePool<LightData, RenderLight>  m_lights;
		RenderSceneChangelog                  m_changelogs[kFrameRingSize];
		dU32                                  m_changelogIndex{ 0 };
		dU64                                  m_appliedFrame{ 0 };
		dVector<DrawItem>                     m_drawItems;
		dVector<LightData>                    m_lightsData;
		dU32                                  m_drawItemsResourcesVersion{ 0 };
		bool                                  m_objectsDirty{ false };
		bool                                  m_lightsDirty{ false };
	};
}
