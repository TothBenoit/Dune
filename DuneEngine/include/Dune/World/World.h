#pragma once

#include "Dune/Game/GameScene.h"
#include "Dune/World/RenderBridge.h"
#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct Camera;
	struct FrameContext;

	namespace Graphics
	{
		class GraphicsSystem;
		struct ViewData;
	}

	struct WorldDesc
	{
		bool render{ true };
	};

	class World
	{
	public:
		void Initialize(Graphics::GraphicsSystem& graphics, const WorldDesc& desc, dU64 frameNumber);
		void Destroy();
		void Update(const FrameContext& frameContext);
#ifdef _DEBUG
		void ValidateRenderScene(dU64 frameNumber) const;
#endif

		[[nodiscard]] GameScene& GetGameScene() { return m_gameScene; }
		[[nodiscard]] const GameScene& GetGameScene() const { return m_gameScene; }
		[[nodiscard]] RenderSceneHandle GetRenderScene() const { return m_renderScene; }
		[[nodiscard]] bool IsInitialized() const { return m_pGraphics != nullptr; }

		[[nodiscard]] static Graphics::ViewData MakeView(const Camera& camera);

	private:
		Graphics::GraphicsSystem* m_pGraphics{ nullptr };
		GameScene                 m_gameScene;
		RenderSceneHandle         m_renderScene;
		RenderBridge              m_renderBridge;
	};

	using WorldHandle = Handle<World>;
}
