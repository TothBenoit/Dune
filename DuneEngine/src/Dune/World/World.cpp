#include "pch.h"
#include "Dune/World/World.h"
#include "Dune/Graphics/GraphicsSystem.h"
#include "RenderBridge.h"

namespace Dune
{
	void World::Initialize(Graphics::GraphicsSystem& graphics, const WorldDesc& desc)
	{
		Assert(!IsInitialized());
		m_pGraphics = &graphics;
		if (desc.render)
			m_renderScene = graphics.CreateRenderScene();
	}

	void World::Destroy()
	{
		Assert(IsInitialized());
		if (m_renderScene.IsValid())
			m_pGraphics->DestroyRenderScene(m_renderScene);
		m_renderScene = {};
		m_gameScene.DestroyAllEntities();
		m_gameScene.ClearChanges();
		m_pGraphics = nullptr;
	}

	void World::Update()
	{
		if (m_renderScene.IsValid())
			RenderBridge::Update(m_gameScene, m_pGraphics->GetRenderScene(m_renderScene));
		m_gameScene.ClearChanges();
	}

	Graphics::ViewData World::MakeView(const Camera& camera)
	{
		return RenderBridge::MakeView(camera);
	}
}
