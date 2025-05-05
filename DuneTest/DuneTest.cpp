#include <Dune.h>
#include <thread>
#include <chrono>
#include <Dune/Core/JobSystem.h>
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/RHI/Device.h>
#include <Dune/Graphics/RHI/ImGuiWrapper.h>
#include <Dune/Graphics/Mesh.h>
#include <Dune/Graphics/Renderer.h>
#include <Dune/Graphics/Window.h>
#include <Dune/Utilities/SimpleCameraController.h>
#include <Dune/Scene/Scene.h>
#include <Dune/Utilities/SceneLoader.h>
#include <filesystem>
#include <imgui/imgui.h>

using namespace Dune;

void DrawGUI(Graphics::ImGuiWrapper imgui, Scene& scene)
{
	imgui.Lock();
	imgui.NewFrame();
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File", true))
		{
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	imgui.Unlock();
}

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
	Graphics::ImGuiWrapper imgui{};
	imgui.Initialize(window, renderer);
	while (window.Update())
	{	
		DrawGUI(imgui, *pScene);
		camera.Update(dt, window.GetInput());
		auto start = std::chrono::high_resolution_clock::now();
		renderer.Render(*pScene, camera.GetCamera());
		auto end = std::chrono::high_resolution_clock::now();
		dt = (float)std::chrono::duration<float>(end - start).count();;
	}
	imgui.Destroy();
	renderer.Destroy();
	window.Destroy();
}

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
	dU32 windowCount{ 5 };
	Job::Initialize(windowCount);
	Graphics::Device device{};
	device.Initialize();

	Scene scene{};
	SceneLoader::Load(std::filesystem::current_path().string().append("\\Resources\\Sponza\\").c_str(), "Sponza.gltf", scene, device);

	Job::JobBuilder jobBuilder{};
	for (dU32 i = 0 ; i < windowCount; i++)
		jobBuilder.DispatchJob<Job::Fence::None>([&]() { Test(&device, &scene); });
	Job::Wait();

	for ( Graphics::Texture& texture : scene.textures)
		texture.Destroy();
	for (Graphics::Mesh& mesh : scene.meshes)
		mesh.Destroy();

	device.Destroy();
	Job::Shutdown();

	return 0;
}