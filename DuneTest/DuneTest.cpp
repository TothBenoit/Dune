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
#include <Dune/Scene/Camera.h>
#include <Dune/Utilities/SceneLoader.h>
#include <Dune/Utilities/StringUtils.h>
#include <filesystem>
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>

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
		m_camera.SetNear(1.0f);
		m_camera.SetFar(50000.0f);
		m_window.Initialize({});
		m_window.SetOnResizeFunc(this, [](void* pData, dU32 width, dU32 height)
			{
				App* pApp = (App*)pData;
				pApp->m_renderer.OnResize(width, height);
				pApp->m_camera.SetAspectRatio((float)width / height);
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

	dMatrix4x4 ComputeTransformMatrix(dVec3& position, dQuat& rotation, float scale)
	{
		dMatrix4x4 worldTransform{};
		DirectX::XMStoreFloat4x4(&worldTransform,
			DirectX::XMMatrixScalingFromVector({ scale, scale, scale }) *
			DirectX::XMMatrixRotationQuaternion(rotation) *
			DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&position))
		);
		return worldTransform;
	}

	dVec3 ToEulerAngles(dQuat& rotation)
	{
		dVec4 r;
		DirectX::XMStoreFloat4(&r, rotation);

		const float xx = r.x * r.x;
		const float yy = r.y * r.y;
		const float zz = r.z * r.z;

		const float m31 = 2.f * r.x * r.z + 2.f * r.y * r.w;
		const float m32 = 2.f * r.y * r.z - 2.f * r.x * r.w;
		const float m33 = 1.f - 2.f * xx - 2.f * yy;

		const float cy = sqrtf(m33 * m33 + m31 * m31);
		const float cx = atan2f(-m32, cy);
		if (cy > 16.f * FLT_EPSILON)
		{
			const float m12 = 2.f * r.x * r.y + 2.f * r.z * r.w;
			const float m22 = 1.f - 2.f * xx - 2.f * zz;

			return { cx, atan2f(m31, m33), atan2f(m12, m22) };
		}
		else
		{
			const float m11 = 1.f - 2.f * yy - 2.f * zz;
			const float m21 = 2.f * r.x * r.y - 2.f * r.z * r.w;

			return { cx, 0.f, atan2f(-m21, m11) };
		}
	}

	bool DrawGizmo(dMatrix4x4& worldTransform)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Z))
			m_gizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E))
			m_gizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			m_gizmoOperation = ImGuizmo::SCALEU;

		Camera& camera = m_camera.GetCamera();
		dMatrix4x4 view{};
		ComputeViewMatrix(camera, &view);
		dMatrix4x4 proj{};
		ComputeProjectionMatrix(camera, &proj);
		
		return ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], m_gizmoOperation, ImGuizmo::WORLD, &worldTransform.m[0][0], nullptr, nullptr, nullptr, nullptr);
	}

	void DrawGUI()
	{
		m_imgui.Lock();
		m_imgui.NewFrame();
		ImGuizmo::BeginFrame();

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
		
		ImVec2 viewportOrigin = ImGui::GetItemRectMin();
		ImVec2 viewportExtents = ImGui::GetItemRectSize();
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGui::End();

		ImGuizmo::SetRect(viewportOrigin.x, viewportOrigin.y, viewportExtents.x, viewportExtents.y);

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
						Light& light = registry.emplace<Light>(id);
						light.position = { 0.0f, 0.0f, 0.0f };
						light.color = { 1.0f, 1.0f, 1.0f };
						light.intensity = 12.0f;
						light.range = 100.f;
						light.type = ELightType::Point;
						light.castShadow = true;
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::Button("Directional light"))
					{
						EntityID id = registry.create();
						Name& name = registry.emplace<Name>(id);
						name.name.assign("DirectionalLight");
						Light& light = registry.emplace<Light>(id);
						light.color = { 1.0f, 1.0f, 1.0f };
						light.intensity = 6.0f;
						light.direction = { 0.0f, 0.0f, -90.0f };
						light.type = ELightType::Directional;
						light.castShadow = true;
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::Button("Spot light"))
					{
						EntityID id = registry.create();
						Name& name = registry.emplace<Name>(id);
						name.name.assign("SpotLight");
						Light& light = registry.emplace<Light>(id);
						light.position = { 0.0f, 0.0f, 0.0f };
						light.intensity = 620.0f;
						light.color = { 1.0f, 1.0f, 1.0f };
						light.range = 10.f;
						light.direction = { 0.0f, 0.0f, 75.0f };
						light.angle = -45.0f;
						light.penumbra = 0.0f;
						light.type = ELightType::Spot;
						light.castShadow = true;
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
						ImGui::DragFloat3("Position", &pTransform->position.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f cm");
						ImGui::DragFloat4("Rotation", pTransform->rotation.m128_f32, 0.25f, -FLT_MAX, +FLT_MAX, "%.2f");
						ImGui::DragFloat("Scale", &pTransform->scale, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f");

						dMatrix4x4 worldTransform = ComputeTransformMatrix(pTransform->position, pTransform->rotation, pTransform->scale);
						if (DrawGizmo(worldTransform))
						{
							dVec position, scale;
							DirectX::XMMatrixDecompose(&scale, &pTransform->rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
							DirectX::XMStoreFloat3(&pTransform->position, position);
							dVec3 scale3{};
							DirectX::XMStoreFloat3(&scale3, scale);
							if (pTransform->scale != scale3.x)
								pTransform->scale = scale3.x;
							else if (pTransform->scale != scale3.y)
								pTransform->scale = scale3.y;
							else if (pTransform->scale != scale3.z)
								pTransform->scale = scale3.z;
						}

						ImGui::TreePop();
					}
				}

				if (Light* pLight = registry.try_get<Light>(m_selectedEntity))
				{
					switch (pLight->type)
					{
					case ELightType::Point:
						if (ImGui::TreeNodeEx("PointLight :", ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::DragFloat3("Position", &pLight->position.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f cm");
							ImGui::ColorPicker3("Color", &pLight->color.x);
							ImGui::DragFloat("Intensity", &pLight->intensity, 0.05f, 0.0f, +FLT_MAX, "%.0f lm");
							ImGui::DragFloat("Radius", &pLight->range, 0.5f, 0.0f, +FLT_MAX, "%.2f cm");
							ImGui::Checkbox("Cast shadow", &pLight->castShadow);

							dVec quatIdentity = DirectX::XMQuaternionIdentity();
							dMatrix4x4 worldTransform = ComputeTransformMatrix(pLight->position, quatIdentity, pLight->range);
							if (DrawGizmo(worldTransform))
							{
								dVec position, scale, rotation;
								DirectX::XMMatrixDecompose(&scale, &rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
								DirectX::XMStoreFloat3(&pLight->position, position);
								dVec3 scale3{};
								DirectX::XMStoreFloat3(&scale3, scale);
								if (pLight->range != scale3.x)
									pLight->range = scale3.x;
								else if (pLight->range != scale3.y)
									pLight->range = scale3.y;
								else if (pLight->range != scale3.z)
									pLight->range = scale3.z;
							}

							ImGui::TreePop();
						}
						break;
					case ELightType::Directional:
						if (ImGui::TreeNodeEx("DirectionalLight :", ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::DragFloat3("Rotation", &pLight->direction.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
							ImGui::ColorPicker3("Color", &pLight->color.x);
							ImGui::DragFloat("Intensity", &pLight->intensity, 0.05f, 0.0f, +FLT_MAX, "%.2f lux");
							ImGui::Checkbox("Cast shadow", &pLight->castShadow);

							dVec quat = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(pLight->direction.x), DirectX::XMConvertToRadians(pLight->direction.y), DirectX::XMConvertToRadians(pLight->direction.z));
							dMatrix4x4 worldTransform = ComputeTransformMatrix(pLight->position, quat, 1.0f);
							if (DrawGizmo(worldTransform))
							{
								dVec position, scale, rotation;
								DirectX::XMMatrixDecompose(&scale, &rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
								dVec3 eulerAngles = ToEulerAngles(rotation);
								pLight->direction.x = DirectX::XMConvertToDegrees(eulerAngles.x);
								pLight->direction.y = DirectX::XMConvertToDegrees(eulerAngles.y);
								pLight->direction.z = DirectX::XMConvertToDegrees(eulerAngles.z);
							}

							ImGui::TreePop();
						}
						break;
					case ELightType::Spot:
						if (ImGui::TreeNodeEx("SpotLight :", ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::DragFloat3("Position", &pLight->position.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f cm");
							ImGui::DragFloat3("Rotation", &pLight->direction.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
							ImGui::ColorPicker3("Color", &pLight->color.x);
							ImGui::DragFloat("Intensity", &pLight->intensity, 0.05f, 0.0f, +FLT_MAX, "%.0f lm");
							ImGui::DragFloat("Range", &pLight->range, 0.5f, 0.f, +FLT_MAX, "%.2f cm");
							ImGui::DragFloat("Angle", &pLight->angle, 0.5f, 0, 180.f, "%.2f");
							ImGui::DragFloat("Penumbra", &pLight->penumbra, 0.001f, 0.0000001f, 1.0f, "%.2f");
							ImGui::Checkbox("Cast shadow", &pLight->castShadow);

							dVec quat = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(pLight->direction.x), DirectX::XMConvertToRadians(pLight->direction.y), DirectX::XMConvertToRadians(pLight->direction.z));
							dMatrix4x4 worldTransform = ComputeTransformMatrix(pLight->position, quat, pLight->range);
							if (DrawGizmo(worldTransform))
							{
								dVec position, scale, rotation;
								DirectX::XMMatrixDecompose(&scale, &rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
								DirectX::XMStoreFloat3(&pLight->position, position);
								dVec3 scale3{};
								DirectX::XMStoreFloat3(&scale3, scale);
								if (pLight->range != scale3.x )
									pLight->range = scale3.x;
								else if (pLight->range != scale3.y)
									pLight->range = scale3.y;
								else if (pLight->range != scale3.z)
									pLight->range = scale3.z;
								dVec3 eulerAngles = ToEulerAngles(rotation);
								pLight->direction.x = DirectX::XMConvertToDegrees(eulerAngles.x);
								pLight->direction.y = DirectX::XMConvertToDegrees(eulerAngles.y);
								pLight->direction.z = DirectX::XMConvertToDegrees(eulerAngles.z);
							}

							ImGui::TreePop();
						}
						break;
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

	ImGuizmo::OPERATION m_gizmoOperation{ ImGuizmo::ROTATE };
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
	Light& sunLight = registry.emplace<Light>(sun);
	registry.emplace<Name>(sun, "Sun");
	sunLight.color = { 1.0f, 1.0f, 0.984f };
	sunLight.direction = { -15.0f, 0.0f, -85.f };
	sunLight.intensity = 6.f;
	sunLight.type = ELightType::Directional;
	sunLight.castShadow = true;

	EntityID ambient = registry.create();
	registry.emplace<Name>(ambient, "Ambient");
	Light& ambientLight = registry.emplace<Light>(ambient);
	ambientLight.color = { 0.937f, 0.977f, 1.0f };
	ambientLight.position = { 0.0f, 650.0f, 0.0f };
	ambientLight.intensity = 100.f;
	ambientLight.range = 200000.0f;
	ambientLight.type = ELightType::Point;
	ambientLight.castShadow = false;

	EntityID candle = registry.create();
	registry.emplace<Name>(candle, "Candle");
	Light& candleLight = registry.emplace<Light>(candle);
	candleLight.color = { 1.0f, 0.576f, 0.16f };
	candleLight.position = { -1285.f, 170.f, 35.f };
	candleLight.intensity = 12.f;
	candleLight.range = 500.f;
	candleLight.type = ELightType::Point;
	candleLight.castShadow = true;

	EntityID spot = registry.create();
	registry.emplace<Name>(spot, "Spot");
	Light& spotLight = registry.emplace<Light>(spot);
	spotLight.position = { 1000.0f, 350.0f, 25.0f };
	spotLight.intensity = 620.0f;
	spotLight.color = { 1.0f, 1.0f, 1.0f };
	spotLight.range = 750.f;
	spotLight.direction = { 0.0f, 0.0f, -45.0f};
	spotLight.angle = 45.0f;
	spotLight.penumbra = 0.5f;
	spotLight.type = ELightType::Spot;
	spotLight.castShadow = true;

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