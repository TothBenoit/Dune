#include <Dune.h>
#include <Dune/Engine.h>
#include <Dune/Application.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <Dune/Platform/RHI/ImGuiContext.h>
#include <Dune/Graphics/ForwardPipeline.h>
#include <Dune/Resources/Shaders/ShaderInterop.h>
#include <Dune/Platform/Window.h>
#include "SimpleCameraController.h"
#include <Dune/Game/GameScene.h>
#include <Dune/World/World.h>
#include <Dune/Game/Component/Light.h>
#include <Dune/Game/Component/Transform.h>
#include <Dune/Game/Component/Name.h>
#include <Dune/Game/Component/MeshInstance.h>
#include <Dune/Game/Component/Camera.h>
#include <filesystem>
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>

using namespace Dune;

static const Transform kStaticCameras[3]
{
	{ .position = { -9.0f, 1.5f, 0.0f }, .rotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XM_PIDIV2, 0.0f), .scale = 1.0f },
	{ .position = { 9.0f, 1.5f, 0.0f }, .rotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, -DirectX::XM_PIDIV2, 0.0f), .scale = 1.0f },
	{ .position = { 0.0f, 9.0f, 0.0f }, .rotation = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XM_PIDIV2 - 0.01f, 0.0f, 0.0f), .scale = 1.0f },
};

class App : public Application
{
public:
	void Init(Engine& engine) override
	{
		m_pWindow = engine.CreateWindow({ .title = L"Dune" }, { .enableImGui = true, .pImGuiLayoutPath = "app://imgui.ini" });
		m_pWorld = engine.CreateWorld({});
		m_pScene = &m_pWorld->GetGameScene();
		GameScene& scene = *m_pScene;

		EntityID sun = scene.CreateEntity("Sun");
		Light sunLight{};
		sunLight.color = { 1.0f, 1.0f, 0.984f };
		sunLight.direction = { -15.0f, 0.0f, -85.f };
		sunLight.intensity = 6.f;
		sunLight.type = ELightType::Directional;
		sunLight.castShadow = true;
		scene.Add(sun, sunLight);

		EntityID ambient = scene.CreateEntity("Ambient");
		Light ambientLight{};
		ambientLight.color = { 0.937f, 0.977f, 1.0f };
		ambientLight.position = { 0.0f, 5.2f, 0.0f };
		ambientLight.intensity = 64.f;
		ambientLight.range = 1600.0f;
		ambientLight.type = ELightType::Point;
		ambientLight.castShadow = false;
		scene.Add(ambient, ambientLight);

		EntityID candle = scene.CreateEntity("Candle");
		Light candleLight{};
		candleLight.color = { 1.0f, 0.576f, 0.16f };
		candleLight.position = { -10.28f, 1.36f, 0.28f };
		candleLight.intensity = 7.7f;
		candleLight.range = 4.f;
		candleLight.type = ELightType::Point;
		candleLight.castShadow = true;
		scene.Add(candle, candleLight);

		EntityID spot = scene.CreateEntity("Spot");
		Light spotLight{};
		spotLight.position = { 8.0f, 2.8f, 0.2f };
		spotLight.intensity = 400.0f;
		spotLight.color = { 1.0f, 1.0f, 1.0f };
		spotLight.range = 6.f;
		spotLight.direction = { 0.0f, 0.0f, -45.0f};
		spotLight.halfAngle = DirectX::XMConvertToRadians(45.0f);
		spotLight.penumbra = 0.5f;
		spotLight.type = ELightType::Spot;
		spotLight.castShadow = true;
		scene.Add(spot, spotLight);

		m_cameraEntity = m_pScene->CreateEntity("Camera", m_camera.GetTransform());
		m_pScene->Add(m_cameraEntity, Camera{ .near = 0.1f, .far = 500.0f });
		m_pView = engine.CreateWorldView({ .pWorld = m_pWorld, .camera = m_cameraEntity, .pWindow = m_pWindow });
		m_closeEvent = m_pWindow->RegisterEvent(this, &App::OnClose, EWindowMessageType::Close);
		m_sponza = engine.LoadModel(*m_pWorld, "app://Sponza/Sponza.gltf");
		Graphics::GraphicsSystem& graphics = engine.GetGraphics();
		m_pScene->SpawnMesh("Cube", { .position = { -0.5f, 0.15f, 1.5f }, .rotation = DirectX::XMQuaternionIdentity(), .scale = 0.3f }, graphics.GetPrimitive(EPrimitive::Cube), graphics.CreateMaterialSet({ graphics.CreateMaterial({ .baseColor = { 0.8f, 0.1f, 0.1f } }) }));
		m_pScene->SpawnMesh("Sphere", { .position = { 0.0f, 0.15f, 1.5f }, .rotation = DirectX::XMQuaternionIdentity(), .scale = 0.3f }, graphics.GetPrimitive(EPrimitive::Sphere), graphics.CreateMaterialSet({ graphics.CreateMaterial({ .baseColor = { 0.9f, 0.9f, 0.9f }, .roughness = 0.3f }) }));
		m_pScene->SpawnMesh("Plane", { .position = { 0.5f, 0.15f, 1.5f }, .rotation = DirectX::XMQuaternionRotationAxis({ 1.0f, 0.0f, 0.0f, 0.0f }, -DirectX::XM_PIDIV2), .scale = 0.3f }, graphics.GetPrimitive(EPrimitive::Plane), graphics.CreateMaterialSet({ graphics.CreateMaterial({ .baseColor = { 0.1f, 0.3f, 0.8f }, .faceCulling = RHI::ECullingMode::None }) }));
	}

	bool Tick(Engine& engine, float deltaTime) override
	{
		m_deltaTime = deltaTime;
		DrawGUI(engine);
		const Input& input = m_pSplitWindow && m_pSplitWindow->HasFocus() ? m_pSplitWindow->GetInput() : m_pWindow->GetInput();
		m_camera.Update(deltaTime, input);
		m_pScene->Set(m_cameraEntity, m_camera.GetTransform());

		if (input.GetKeyDown(KeyCode::F2) || m_closeSplitWindow)
		{
			if (m_pSplitWindow)
				CloseSplitWindow(engine);
			else
				OpenSplitWindow(engine);
			m_closeSplitWindow = false;
		}
		return m_isRunning;
	}

	void OpenSplitWindow(Engine& engine)
	{
		constexpr dU32 width = 1280;
		constexpr dU32 height = 720;
		constexpr dU32 halfWidth = width / 2;
		constexpr dU32 halfHeight = height / 2;
		m_pSplitWindow = engine.CreateWindow({ .width = width, .height = height, .title = L"Dune - Split view" });
		m_splitCloseEvent = m_pSplitWindow->RegisterEvent(this, &App::OnCloseSplitWindow, EWindowMessageType::Close);

		m_splitViews[0] = engine.CreateWorldView({ .pWorld = m_pWorld, .camera = m_cameraEntity, .pWindow = m_pSplitWindow, .width = halfWidth, .height = halfHeight });
		for (dU32 i = 0; i < 3; i++)
		{
			m_staticCameras[i] = m_pScene->CreateEntity("Static camera", kStaticCameras[i]);
			m_pScene->Add(m_staticCameras[i], Camera{ .near = 0.1f, .far = 500.0f });

			const dU32 quadrant = i + 1;
			m_splitViews[quadrant] = engine.CreateWorldView({ .pWorld = m_pWorld, .camera = m_staticCameras[i], .pWindow = m_pSplitWindow, .x = (quadrant % 2) * halfWidth, .y = (quadrant / 2) * halfHeight, .width = halfWidth, .height = halfHeight });
		}
	}

	void CloseSplitWindow(Engine& engine)
	{
		for (WorldView*& pView : m_splitViews)
		{
			engine.DestroyWorldView(pView);
			pView = nullptr;
		}
		for (EntityID camera : m_staticCameras)
			m_pScene->DestroyEntity(camera);

		m_pSplitWindow->UnregisterEvent(EWindowMessageType::Close, m_splitCloseEvent);
		engine.DestroyWindow(m_pSplitWindow);
		m_pSplitWindow = nullptr;
	}

	void Shutdown(Engine& engine) override
	{
		if (m_pSplitWindow)
			CloseSplitWindow(engine);

		m_pWindow->UnregisterEvent(EWindowMessageType::Close, m_closeEvent);
		engine.DestroyWorldView(m_pView);

		m_pScene->DestroyAllEntities();
		engine.UnloadModel(m_sponza);
		engine.DestroyWorld(m_pWorld);
		engine.DestroyWindow(m_pWindow);
	}

	static void OnClose(void* pData, const WindowMessage&)
	{
		((App*)pData)->m_isRunning = false;
	}

	static void OnCloseSplitWindow(void* pData, const WindowMessage&)
	{
		((App*)pData)->m_closeSplitWindow = true;
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

		const Window& window = *m_pWindow;
		dMatrix4x4 view{};
		ComputeViewMatrix(m_pScene->Get<Transform>(m_cameraEntity), &view);
		dMatrix4x4 proj{};
		ComputeProjectionMatrix(m_pScene->Get<Camera>(m_cameraEntity), window.GetHeight() > 0 ? (float)window.GetWidth() / window.GetHeight() : 1.0f, &proj);
		
		return ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], m_gizmoOperation, ImGuizmo::WORLD, &worldTransform.m[0][0], nullptr, nullptr, nullptr, nullptr);
	}

	void DrawGUI(Engine& engine)
	{
		RHI::ImGuiContext& imgui = engine.GetImGui(*m_pWindow);
		RHI::ImGuiContext::LockGuard guard = imgui.AcquireContext();
		imgui.NewFrame(guard);
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
			dString FPSlabel = StringUtils::printf("FPS : %.0f", m_framerate);
			ImGui::BeginMenu(FPSlabel.c_str(), false);
			ImGui::EndMainMenuBar();
		}

		GameScene& scene = *m_pScene;
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
						EntityID id = scene.CreateEntity("PointLight");
						Light light{};
						light.position = { 0.0f, 0.0f, 0.0f };
						light.color = { 1.0f, 1.0f, 1.0f };
						light.intensity = 7.7f;
						light.range = 1.f;
						light.type = ELightType::Point;
						light.castShadow = true;
						scene.Add(id, light);
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::Button("Directional light"))
					{
						EntityID id = scene.CreateEntity("DirectionalLight");
						Light light{};
						light.color = { 1.0f, 1.0f, 1.0f };
						light.intensity = 6.0f;
						light.direction = { 0.0f, 0.0f, -90.0f };
						light.type = ELightType::Directional;
						light.castShadow = true;
						scene.Add(id, light);
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::Button("Spot light"))
					{
						EntityID id = scene.CreateEntity("SpotLight");
						Light light{};
						light.position = { 0.0f, 0.0f, 0.0f };
						light.intensity = 400.0f;
						light.color = { 1.0f, 1.0f, 1.0f };
						light.range = 5.f;
						light.direction = { 0.0f, 0.0f, 75.0f };
						light.halfAngle = DirectX::XMConvertToRadians(45.0f);
						light.penumbra = 1.e-7f;
						light.type = ELightType::Spot;
						light.castShadow = true;
						scene.Add(id, light);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
				scene.View<Name>().each([&](EntityID entity, const Name& name)
					{
						bool isSelected = m_selectedEntity == entity;
						ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ((isSelected) ? ImGuiTreeNodeFlags_Selected : 0);
						dString id = StringUtils::printf("%d", entity);
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

			if (const Name* pName = scene.TryGet<Name>(m_selectedEntity) )
			{
				ImGui::Text("%s", pName->name.c_str());
				ImGui::Separator();

				scene.TryModify<Transform>(m_selectedEntity, [&](Transform& transform)
					{
						bool changed = false;
						if (ImGui::TreeNodeEx("Transform :", ImGuiTreeNodeFlags_DefaultOpen))
						{
							changed |= ImGui::DragFloat3("Position", &transform.position.x, 0.01f, -FLT_MAX, +FLT_MAX, "%.2f m");
							changed |= ImGui::DragFloat4("Rotation", transform.rotation.m128_f32, 0.25f, -FLT_MAX, +FLT_MAX, "%.2f");
							changed |= ImGui::DragFloat("Scale", &transform.scale, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f");

							dMatrix4x4 worldTransform = ComputeTransformMatrix(transform.position, transform.rotation, transform.scale);
							if (DrawGizmo(worldTransform))
							{
								changed = true;
								dVec position, scale;
								DirectX::XMMatrixDecompose(&scale, &transform.rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
								DirectX::XMStoreFloat3(&transform.position, position);
								dVec3 scale3{};
								DirectX::XMStoreFloat3(&scale3, scale);
								if (transform.scale != scale3.x)
									transform.scale = scale3.x;
								else if (transform.scale != scale3.y)
									transform.scale = scale3.y;
								else if (transform.scale != scale3.z)
									transform.scale = scale3.z;
							}

							ImGui::TreePop();
						}
						return changed;
					});

				scene.TryModify<Light>(m_selectedEntity, [&](Light& light)
					{
						bool changed = false;
						switch (light.type)
						{
						case ELightType::Point:
							if (ImGui::TreeNodeEx("PointLight :", ImGuiTreeNodeFlags_DefaultOpen))
							{
								changed |= ImGui::DragFloat3("Position", &light.position.x, 0.01f, -FLT_MAX, +FLT_MAX, "%.2f m");
								changed |= ImGui::ColorPicker3("Color", &light.color.x);
								changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, +FLT_MAX, "%.0f lm");
								changed |= ImGui::DragFloat("Radius", &light.range, 0.01f, 1e-7f, +FLT_MAX, "%.2f m");
								changed |= ImGui::Checkbox("Cast shadow", &light.castShadow);

								dVec quatIdentity = DirectX::XMQuaternionIdentity();
								dMatrix4x4 worldTransform = ComputeTransformMatrix(light.position, quatIdentity, light.range);
								if (DrawGizmo(worldTransform))
								{
									changed = true;
									dVec position, scale, rotation;
									DirectX::XMMatrixDecompose(&scale, &rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
									DirectX::XMStoreFloat3(&light.position, position);
									dVec3 scale3{};
									DirectX::XMStoreFloat3(&scale3, scale);
									if (light.range != scale3.x)
										light.range = scale3.x;
									else if (light.range != scale3.y)
										light.range = scale3.y;
									else if (light.range != scale3.z)
										light.range = scale3.z;
								}

								ImGui::TreePop();
							}
							break;
						case ELightType::Directional:
							if (ImGui::TreeNodeEx("DirectionalLight :", ImGuiTreeNodeFlags_DefaultOpen))
							{
								changed |= ImGui::DragFloat3("Rotation", &light.direction.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
								changed |= ImGui::ColorPicker3("Color", &light.color.x);
								changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, +FLT_MAX, "%.2f lux");
								changed |= ImGui::Checkbox("Cast shadow", &light.castShadow);

								dVec quat = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(light.direction.x), DirectX::XMConvertToRadians(light.direction.y), DirectX::XMConvertToRadians(light.direction.z));
								dMatrix4x4 worldTransform = ComputeTransformMatrix(light.position, quat, 1.0f);
								if (DrawGizmo(worldTransform))
								{
									changed = true;
									dVec position, scale, rotation;
									DirectX::XMMatrixDecompose(&scale, &rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
									dVec3 eulerAngles = ToEulerAngles(rotation);
									light.direction.x = DirectX::XMConvertToDegrees(eulerAngles.x);
									light.direction.y = DirectX::XMConvertToDegrees(eulerAngles.y);
									light.direction.z = DirectX::XMConvertToDegrees(eulerAngles.z);
								}

								ImGui::TreePop();
							}
							break;
						case ELightType::Spot:
							if (ImGui::TreeNodeEx("SpotLight :", ImGuiTreeNodeFlags_DefaultOpen))
							{
								changed |= ImGui::DragFloat3("Position", &light.position.x, 0.01f, -FLT_MAX, +FLT_MAX, "%.2f m");
								changed |= ImGui::DragFloat3("Rotation", &light.direction.x, 0.5f, -FLT_MAX, +FLT_MAX, "%.2f");
								changed |= ImGui::ColorPicker3("Color", &light.color.x);
								changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, +FLT_MAX, "%.0f lm");
								changed |= ImGui::DragFloat("Range", &light.range, 0.01f, 1e-7f, +FLT_MAX, "%.2f m");
								float angle = DirectX::XMConvertToDegrees(light.halfAngle * 2.0f);
								if (ImGui::DragFloat("Angle", &angle, 0.5f, 0, 180.f, "%.2f"))
								{
									light.halfAngle = DirectX::XMConvertToRadians(angle) * 0.5f;
									changed = true;
								}
								changed |= ImGui::DragFloat("Penumbra", &light.penumbra, 0.001f, 1e-7f, 1.0f, "%.2f");
								changed |= ImGui::Checkbox("Cast shadow", &light.castShadow);

								dVec quat = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(light.direction.x), DirectX::XMConvertToRadians(light.direction.y), DirectX::XMConvertToRadians(light.direction.z));
								dMatrix4x4 worldTransform = ComputeTransformMatrix(light.position, quat, light.range);
								if (DrawGizmo(worldTransform))
								{
									changed = true;
									dVec position, scale, rotation;
									DirectX::XMMatrixDecompose(&scale, &rotation, &position, DirectX::XMLoadFloat4x4(&worldTransform));
									DirectX::XMStoreFloat3(&light.position, position);
									dVec3 scale3{};
									DirectX::XMStoreFloat3(&scale3, scale);
									if (light.range != scale3.x )
										light.range = scale3.x;
									else if (light.range != scale3.y)
										light.range = scale3.y;
									else if (light.range != scale3.z)
										light.range = scale3.z;
									dVec3 eulerAngles = ToEulerAngles(rotation);
									light.direction.x = DirectX::XMConvertToDegrees(eulerAngles.x);
									light.direction.y = DirectX::XMConvertToDegrees(eulerAngles.y);
									light.direction.z = DirectX::XMConvertToDegrees(eulerAngles.z);
								}

								ImGui::TreePop();
							}
							break;
						}
						return changed;
					});
			}
			ImGui::End();
		}

		if (m_showDemo)
			ImGui::ShowDemoWindow(&m_showDemo);
	}

private:
	Window* m_pWindow{ nullptr };
	World* m_pWorld{ nullptr };
	GameScene* m_pScene{ nullptr };
	WorldView* m_pView{ nullptr };
	ModelHandle m_sponza{};
	dU32 m_closeEvent{ 0 };

	SimpleCameraController m_camera{};
	EntityID m_cameraEntity{};
	bool m_isRunning{ true };

	Window* m_pSplitWindow{ nullptr };
	WorldView* m_splitViews[4]{};
	EntityID m_staticCameras[3]{};
	dU32 m_splitCloseEvent{ 0 };
	bool m_closeSplitWindow{ false };

	float m_deltaTime{ 0.0f };
	float m_framerate{ 0.0f };

	bool m_showScene{ true };
	bool m_showInspector{ true };
	bool m_showDemo{ false };

	EntityID m_selectedEntity{ (EntityID)-1 };

	ImGuizmo::OPERATION m_gizmoOperation{ ImGuizmo::ROTATE };
};

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	const dString solutionDirectory(SOLUTION_DIR);
	App app{};
	Run(app,
		{
			.workerCount = std::min(4u, std::thread::hardware_concurrency()) - 1,
			.engineResourcePath = solutionDirectory + "/DuneEngine/include/Dune/Resources/",
			.roots = { { .name = "app", .path = solutionDirectory + "/Resources/" } },
		});

	return 0;
}
