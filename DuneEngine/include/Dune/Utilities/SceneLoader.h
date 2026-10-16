#pragma once

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		struct ModelData;
		struct ModelResources;
	}

	namespace SceneLoader
	{
		void Spawn(Scene& scene, const Graphics::ModelData& model, const Graphics::ModelResources& resources);
	}
}
