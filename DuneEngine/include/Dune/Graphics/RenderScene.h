#pragma once

#include "Dune/Graphics/FrameData.h"
#include "Dune/Core/HandleTypes.h"

namespace Dune::Graphics
{
	class ResourceManager;

	class RenderScene
	{
	public:
		void Reset();
		void AddMesh(const dMatrix4x4& objectToWorld, MeshHandle mesh, MaterialSetHandle materials);
		void AddLight(const LightData& light);
		[[nodiscard]] const FrameData& Build(const ResourceManager& resources, float deltaTime);
		[[nodiscard]] const FrameData& GetFrameData() const { return m_frameData; }

	private:
		struct MeshObject
		{
			dMatrix4x4        objectToWorld;
			MeshHandle        mesh;
			MaterialSetHandle materials;
		};

		dVector<MeshObject> m_meshObjects;
		FrameData           m_frameData;
	};
}
