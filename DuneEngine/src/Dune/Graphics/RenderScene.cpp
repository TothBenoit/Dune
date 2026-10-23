#include "pch.h"
#include "Dune/Graphics/RenderScene.h"

namespace Dune::Graphics
{
	void RenderScene::Reset()
	{
		m_objects.clear();
		m_lights.clear();
	}

	void RenderScene::AddMesh(const dMatrix4x4& objectToWorld, MeshHandle mesh, MaterialSetHandle materials)
	{
		m_objects.push_back({ .objectToWorld = objectToWorld, .mesh = mesh, .materials = materials });
	}

	void RenderScene::AddLight(const LightData& light)
	{
		m_lights.push_back(light);
	}
}
