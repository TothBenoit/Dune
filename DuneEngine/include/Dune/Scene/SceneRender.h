#pragma once

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		class ResourceManager;
		struct FrameData;
	}

	namespace SceneRender
	{
		void Extract(const Scene& scene, const Graphics::ResourceManager& resources, Graphics::FrameData& frameData);
	}
}
