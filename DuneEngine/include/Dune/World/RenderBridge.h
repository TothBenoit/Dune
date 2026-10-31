#pragma once

#include "Dune/Game/GameScene.h"
#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct Camera;
	struct FrameContext;

	namespace Graphics
	{
		class RenderScene;
		struct ViewData;
	}

	class RenderBridge
	{
	public:
		void Update(const FrameContext& frameContext, const GameScene& gameScene, Graphics::RenderScene& renderScene);
		void Reset();
#ifdef _DEBUG
		void Validate(const GameScene& gameScene, const Graphics::RenderScene& renderScene) const;
#endif

		[[nodiscard]] static Graphics::ViewData MakeView(const Camera& camera);

	private:
		void SyncObject(const GameScene& gameScene, Graphics::RenderScene& renderScene, EntityID entity);
		void SyncLight(const GameScene& gameScene, Graphics::RenderScene& renderScene, EntityID entity);

	private:
		dHashMap<EntityID, RenderObjectHandle> m_objects;
		dHashMap<EntityID, RenderLightHandle>  m_lights;
	};
}
