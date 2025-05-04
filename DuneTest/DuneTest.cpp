#include <Dune.h>
#include <thread>
#include <chrono>
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/RHI/Device.h>
#include <Dune/Graphics/Mesh.h>
#include <Dune/Graphics/Renderer.h>
#include <Dune/Graphics/Window.h>
#include <Dune/Utilities/SimpleCameraController.h>
#include <Dune/Scene/Scene.h>
#include <Dune/Utilities/SceneLoader.h>
#include <filesystem>

using namespace Dune;

void Test(Graphics::Device* pDevice, Scene* pScene)
{
	Graphics::Window window{};
	window.Initialize({});
	Graphics::Renderer renderer;
	SimpleCameraController camera{};
	window.SetOnResizeFunc(&renderer, [](void* pData, dU32 width, dU32 height)
		{
			Graphics::Renderer* pRenderer = (Graphics::Renderer*)pData;
			pRenderer->OnResize(width, height);
		}
	);
	renderer.Initialize(*pDevice, window);
	float dt = 0.f;
	while (window.Update())
	{	
		camera.Update(dt, window.GetInput());
		auto start = std::chrono::high_resolution_clock::now();
		renderer.Render(*pScene, camera.GetCamera());
		auto end = std::chrono::high_resolution_clock::now();
		dt = (float)std::chrono::duration<float>(end - start).count();;
	}

	renderer.Destroy();
	window.Destroy();
}

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
	Graphics::Device device{};
	device.Initialize();

	Scene scene{};
	SceneLoader::Load(std::filesystem::current_path().string().append("\\Resources\\Sponza\\").c_str(), "Sponza.gltf", scene, device);
	
	dVector<std::thread> tests;
	dU32 windowCount{ 5 };
	tests.reserve(windowCount);
	for (dU32 i{ 0 }; i < windowCount; i++)
	{
		tests.emplace_back(std::thread(&Test, &device, &scene));
	}

	for (dU32 i{ 0 }; i < windowCount; i++)
	{
		tests[i].join();
	}

	for ( Graphics::Texture& texture : scene.textures)
		texture.Destroy();
	for (Graphics::Mesh& mesh : scene.meshes)
		mesh.Destroy();

	device.Destroy();

	return 0;
}