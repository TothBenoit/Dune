#pragma once

#include "Dune/Graphics/FrameData.h"
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

	class RenderScene
	{
	public:
		void Initialize();
		void Destroy();

		[[nodiscard]] RenderObjectHandle CreateObject(const RenderObject& object);
		void SetObject(RenderObjectHandle handle, const RenderObject& object);
		void DestroyObject(RenderObjectHandle handle);

		[[nodiscard]] RenderLightHandle CreateLight(const LightData& light);
		void SetLight(RenderLightHandle handle, const LightData& light);
		void DestroyLight(RenderLightHandle handle);

		[[nodiscard]] const RenderObject& GetObject(RenderObjectHandle handle) const { return m_objects.Get(handle); }
		[[nodiscard]] const LightData& GetLight(RenderLightHandle handle) const { return m_lights.Get(handle); }

		[[nodiscard]] const dVector<DrawItem>& GetDrawItems(const ResourceManager& resources, dU32 resourcesVersion);
		[[nodiscard]] const dVector<LightData>& GetLightsData();

	private:
		HandleAllocator<RenderObject>         m_objectHandles;
		ResourcePool<RenderObject>            m_objects;
		HandleAllocator<RenderLight>          m_lightHandles;
		ResourcePool<LightData, RenderLight>  m_lights;
		dVector<DrawItem>                     m_drawItems;
		dVector<LightData>                    m_lightsData;
		dU32                                  m_drawItemsResourcesVersion{ 0 };
		bool                                  m_objectsDirty{ false };
		bool                                  m_lightsDirty{ false };
	};
}
