#include "pch.h"
#include "Dune/World/World.h"
#include "Dune/Graphics/GraphicsSystem.h"

namespace Dune
{
	void World::Initialize(Graphics::GraphicsSystem& graphics, const WorldDesc& desc, dU64 frameNumber)
	{
		Assert(!IsInitialized());
		m_pGraphics = &graphics;
		if (desc.render)
			m_renderScene = graphics.CreateRenderScene(frameNumber);
	}

	void World::Destroy()
	{
		Assert(IsInitialized());
		if (m_renderScene.IsValid())
			m_pGraphics->DestroyRenderScene(m_renderScene);
		m_renderScene = {};
		m_renderBridge.Reset();
		m_gameScene.DestroyAllEntities();
		m_gameScene.ClearChanges();
		m_pGraphics = nullptr;
	}

	void World::Update(const FrameContext& frameContext)
	{
		if (m_renderScene.IsValid())
			m_renderBridge.Update(frameContext, m_gameScene, m_pGraphics->GetRenderScene(m_renderScene));
		m_gameScene.ClearChanges();
	}

#ifdef _DEBUG
	void World::ValidateRenderScene(dU64 frameNumber) const
	{
		if (!m_renderScene.IsValid() || m_gameScene.HasChanges())
			return;
		const Graphics::RenderScene& renderScene = m_pGraphics->GetRenderScene(m_renderScene);
		if (renderScene.GetAppliedFrame() == frameNumber)
			m_renderBridge.Validate(m_gameScene, renderScene);
	}
#endif

	Graphics::ViewData World::MakeView(const Camera& camera)
	{
		return RenderBridge::MakeView(camera);
	}
}
