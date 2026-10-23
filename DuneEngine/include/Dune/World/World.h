#pragma once

#include "Dune/Game/GameScene.h"
#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct Camera;

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
		void Initialize(Graphics::GraphicsSystem& graphics, const WorldDesc& desc);
		void Destroy();
		void Update();

		[[nodiscard]] GameScene& GetGameScene() { return m_gameScene; }
		[[nodiscard]] const GameScene& GetGameScene() const { return m_gameScene; }
		[[nodiscard]] RenderSceneHandle GetRenderScene() const { return m_renderScene; }
		[[nodiscard]] bool IsInitialized() const { return m_pGraphics != nullptr; }

		[[nodiscard]] static Graphics::ViewData MakeView(const Camera& camera);

	private:
		Graphics::GraphicsSystem* m_pGraphics{ nullptr };
		GameScene                 m_gameScene;
		RenderSceneHandle         m_renderScene;
	};

	using WorldHandle = Handle<World>;
}
