#pragma once

namespace Dune
{
	class Scene;
	struct Camera;

	namespace Graphics
	{
		class ResourceManager;
		struct FrameData;
		struct ViewData;
	}

	namespace SceneRender
	{
		void Extract(const Scene& scene, const Graphics::ResourceManager& resources, Graphics::FrameData& frameData);
		[[nodiscard]] Graphics::ViewData ExtractView(const Camera& camera);
	}
}
