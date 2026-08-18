#pragma once

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		class ResourceManager;
	}

	namespace SceneLoader
	{
		bool Load(const char* path, Scene& scene, Graphics::ResourceManager& resourceManager);
	}
}
