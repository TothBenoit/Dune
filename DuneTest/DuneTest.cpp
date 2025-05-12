#include <Dune.h>
#include <chrono>
#include <Dune/Core/JobSystem.h>
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/RHI/Device.h>
#include <Dune/Graphics/RHI/ImGuiWrapper.h>
#include <Dune/Graphics/Mesh.h>
#include <Dune/Graphics/Shaders/ShaderTypes.h>
#include <Dune/Graphics/Renderer.h>
#include <Dune/Graphics/Window.h>
#include <Dune/Utilities/SimpleCameraController.h>
#include <Dune/Scene/Scene.h>
#include <Dune/Utilities/SceneLoader.h>
#include <Dune/Utilities/StringUtils.h>
#include <filesystem>
#include <imgui/imgui.h>

using namespace Dune;

class App
{
public:
	App(Graphics::Device* pDevice, Scene* pScene)
		: m_pDevice{pDevice}
		, m_pScene{pScene}
	{}

	void Run()
	{
		m_window.Initialize({});
		m_window.SetOnResizeFunc(&m_renderer, [](void* pData, dU32 width, dU32 height)
			{
				Graphics::Renderer* pRenderer = (Graphics::Renderer*)pData;
				pRenderer->OnResize(width, height);
			}
		);
		m_renderer.Initialize(*m_pDevice, m_window);
		m_imgui.Initialize(m_window, m_renderer);

		auto lastFrameTimer = std::chrono::high_resolution_clock::now();
		while (m_window.Update())
		{
			auto timer = std::chrono::high_resolution_clock::now();
			m_deltaTime = (float)std::chrono::duration<float>(timer - lastFrameTimer).count();
			lastFrameTimer = std::chrono::high_resolution_clock::now();

			DrawGUI();
			m_camera.Update(m_deltaTime, m_window.GetInput());
			m_renderer.Render(*m_pScene, m_camera.GetCamera());
		}

		m_imgui.Destroy();
		m_renderer.Destroy();
		m_window.Destroy();
	}

	void DrawGUI()
	{
		m_imgui.Lock();
		m_imgui.NewFrame();

		ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.66f;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
		ImGui::PopStyleVar(3);
		ImGui::DockSpace(ImGui::GetID("Viewport"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode );
		ImGui::End();

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File", true))
			{
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window", true))
			{
				bool selected{ false };
				if (ImGui::Selectable("Scene", &selected))
					m_showScene = true;

				if (ImGui::Selectable("Inspector", &selected))
					m_showInspector = true;

				if (ImGui::Selectable("Demo", &selected))
					m_showDemo = true;

				ImGui::EndMenu();
			}

			float smoothing = 0.95f;
			m_framerate = m_framerate * smoothing + (1.0f / m_deltaTime) * (1.0f - smoothing);
			dString FPSlabel = dStringUtils::printf("FPS : %.0f", m_framerate);
			ImGui::BeginMenu(FPSlabel.c_str(), false);
			ImGui::EndMainMenuBar();
		}

		const entt::registry& kRegistry = m_pScene->registry;
		entt::registry& registry = m_pScene->registry;
		if (m_showScene)
		{
			if (ImGui::Begin("Scene", &m_showScene))
			{
				if (ImGui::Button("Add Entity"))
				{
					ImGui::OpenPopup("##AddEntity");
				}
				
				// This can cause a data race if another window is currently rendering
				// I didn't decide if the user should be able to modify a scene while another thread is rendering so I don't fix it yet
				if (ImGui::BeginPopup("##AddEntity"))
				{
					if (ImGui::Button("Point light"))
					{
						EntityID id = registry.create();
						Name& name = registry.emplace<Name>(id);
						name.name.assign("PointLight");
						Graphics::PointLight& light = registry.emplace<Graphics::PointLight>(id);
						light.color = { 1.0f, 1.0f, 1.0f };
						light.intensity = 1.0f;
						light.radius = 10.f;
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::Button("Directional light"))
					{
						EntityID id = registry.create();
						Name& name = registry.emplace<Name>(id);
						name.name.assign("DirectionalLight");
						Graphics::DirectionalLight& light = registry.emplace<Graphics::DirectionalLight>(id);
						light.color = { 1.0f, 1.0f, 1.0f };
						light.intensity = 1.0f;
						light.direction = { 0.0f, -1.0f, 0.0f };
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
				kRegistry.view<const Name>().each([&](EntityID entity, const Name& name)
					{
						bool isSelected = m_selectedEntity == entity;
						ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ((isSelected) ? ImGuiTreeNodeFlags_Selected : 0);
						dString id = dStringUtils::printf("%d", entity);
						ImGui::TreeNodeEx(id.c_str(), flags, name.name.c_str());
						if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
							m_selectedEntity = entity;
					});
			}
			ImGui::End();
		}

		if (m_showInspector)
		{
			ImGui::Begin("Inspector", &m_showInspector);

			if (const Name* pName = kRegistry.try_get<Name>(m_selectedEntity) )
			{
				ImGui::Text("%s", pName->name.c_str());
				ImGui::Separator();

				if (Transform* pTransform = registry.try_get<Transform>(m_selectedEntity))
				{
					if (ImGui::TreeNodeEx("Transform :", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::DragFloat3("Position", &pTransform->position.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::DragFloat4("Rotation", pTransform->rotation.m128_f32, 0.25f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::DragFloat("Scale", &pTransform->scale, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::TreePop();
					}
				}

				if (Graphics::PointLight* pLight = registry.try_get<Graphics::PointLight>(m_selectedEntity))
				{
					if (ImGui::TreeNodeEx("PointLight :", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::DragFloat3("Position", &pLight->position.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::ColorPicker3("Color", &pLight->color.x);
						ImGui::DragFloat("Intensity", &pLight->intensity, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::DragFloat("Radius", &pLight->radius, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::TreePop();
					}
				}

				if (Graphics::DirectionalLight* pLight = registry.try_get<Graphics::DirectionalLight>(m_selectedEntity))
				{
					if (ImGui::TreeNodeEx("DirectionalLight :", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::ColorPicker3("Color", &pLight->color.x);
						ImGui::DragFloat("Intensity", &pLight->intensity, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::DragFloat3("Direction", &pLight->direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
						if (ImGui::Button("Normalize")) 
							DirectX::XMStoreFloat3(&pLight->direction, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&pLight->direction)));
						ImGui::TreePop();
					}
				}
			}
			ImGui::End();
		}

		if (m_showDemo)
			ImGui::ShowDemoWindow(&m_showDemo);

		m_imgui.Unlock();
	}

private:
	Graphics::Device* m_pDevice;
	Scene* m_pScene;

	Graphics::Window m_window{};
	Graphics::Renderer m_renderer{};
	Graphics::ImGuiWrapper m_imgui{};
	SimpleCameraController m_camera{};

	float m_deltaTime{ 0.0f };
	float m_framerate{ 0.0f };

	bool m_showScene{ true };
	bool m_showInspector{ true };
	bool m_showDemo{ false };

	EntityID m_selectedEntity{ (EntityID)-1 };
};

void Test(Graphics::Device* pDevice, Scene* pScene)
{
	App app{ pDevice, pScene };
	app.Run();
}

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
	dU32 testCount{ 1 };
	Job::Initialize(testCount);
	Graphics::Device device{};
	device.Initialize();

	Scene scene{};
	entt::registry& registry = scene.registry;
	SceneLoader::Load(std::filesystem::current_path().string().append("\\Resources\\Sponza\\").c_str(), "Sponza.gltf", scene, device);
	EntityID sun = registry.create();
	Graphics::DirectionalLight& light = registry.emplace<Graphics::DirectionalLight>(sun);
	light.color = { 1.1f, 0.977f, 0.937f };
	light.direction = { 0.1f, -1.0f, 0.1f };
	light.intensity = 1.0f;
	Name& name = registry.emplace<Name>(sun);
	name.name.assign("Sun");

	Job::JobBuilder jobBuilder{};
	for (dU32 i = 0 ; i < testCount; i++)
		jobBuilder.DispatchJob<Job::Fence::None>([&]() { Test(&device, &scene); });
	Job::Wait();

	for (Graphics::Texture& texture : scene.textures)
		texture.Destroy();
	for (Graphics::Mesh& mesh : scene.meshes)
		mesh.Destroy();

	device.Destroy();
	Job::Shutdown();

	return 0;
}