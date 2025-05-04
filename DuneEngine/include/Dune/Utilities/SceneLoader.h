#pragma once

namespace Dune 
{
	struct Scene;

	namespace Graphics
	{
		class Device;
	}

	namespace SceneLoader
	{
		bool Load(const char* dirPath, const char* fileName, Scene& scene, Graphics::Device& device);
	}
}