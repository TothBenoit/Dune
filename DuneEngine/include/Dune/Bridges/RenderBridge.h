#pragma once

namespace Dune
{
	class GameScene;
	struct Camera;

	namespace Graphics
	{
		class RenderScene;
		struct ViewData;
	}

	namespace RenderBridge
	{
		void Update(const GameScene& gameScene, Graphics::RenderScene& renderScene);
		[[nodiscard]] Graphics::ViewData MakeView(const Camera& camera);
	}
}
