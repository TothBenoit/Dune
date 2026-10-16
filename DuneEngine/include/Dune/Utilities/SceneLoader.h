#pragma once

namespace Dune
{
	class Scene;
	struct Model;

	namespace SceneLoader
	{
		void Spawn(Scene& scene, const Model& model);
	}
}
