#pragma once

#include "Dune/Graphics/FrameData.h"
#include "Dune/Core/HandleTypes.h"

namespace Dune::Graphics
{
	struct RenderObject
	{
		dMatrix4x4        objectToWorld;
		MeshHandle        mesh;
		MaterialSetHandle materials;
	};

	class RenderScene
	{
	public:
		void Reset();
		void AddMesh(const dMatrix4x4& objectToWorld, MeshHandle mesh, MaterialSetHandle materials);
		void AddLight(const LightData& light);

		[[nodiscard]] const dVector<RenderObject>& GetObjects() const { return m_objects; }
		[[nodiscard]] const dVector<LightData>& GetLights() const { return m_lights; }

	private:
		dVector<RenderObject> m_objects;
		dVector<LightData>    m_lights;
	};
}
