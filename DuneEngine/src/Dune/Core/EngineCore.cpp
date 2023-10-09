#include "pch.h"
#include "EngineCore.h"
#include "Dune/Core/Input.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"
#include "Dune/Core/ECS/Components/CameraComponent.h"
#include "Dune/Core/ECS/Components/PointLightComponent.h"
#include "Dune/Core/ECS/Components/DirectionalLightComponent.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Material.h"
#include "Dune/Utilities/MeshLoader.h"
#include "Dune/Core/JobSystem.h"

namespace Dune
{
	void EngineCore::Init(const Window* pWindow)
	{
		if (m_isInitialized)
		{
			LOG_CRITICAL("Tried to initialize EngineCore which was already initialized");
			return;
		}

		Job::Initialize();

		ComponentManager<TransformComponent>::Init();
		ComponentManager<BindingComponent>::Init();
		ComponentManager<GraphicsComponent>::Init();
		ComponentManager<CameraComponent>::Init();
		ComponentManager<PointLightComponent>::Init();
		ComponentManager<DirectionalLightComponent>::Init();

		Renderer::GetInstance().Initialize(pWindow);

		m_isInitialized = true;

		m_cameraID = CreateEntity("Camera");
		AddComponent<CameraComponent>(m_cameraID);
	}

	void EngineCore::Shutdown()
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to shutdown EngineCore which was not initialized");
			return;
		}
#endif // _DEBUG

		Job::Wait();
		Job::Shutdown();

		ComponentManager<TransformComponent>::Shutdown();
		ComponentManager<BindingComponent>::Shutdown();
		ComponentManager<GraphicsComponent>::Shutdown();
		ComponentManager<CameraComponent>::Shutdown();
		ComponentManager<PointLightComponent>::Shutdown();
		ComponentManager<DirectionalLightComponent>::Shutdown();

		Renderer::GetInstance().Shutdown();
	}

	void EngineCore::Update(float dt)
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to update EngineCore which was not initialized");
			return;
		}
#endif // _DEBUG
		Profile("EngineCoreUpdate");

		m_deltaTime = dt;
		DrawInterface();
		UpdateCamera();
		UpdateTransforms();
		UpdateGraphicsData();
		ClearModifiedEntities();

		Renderer::GetInstance().Render();
	}

	EntityID EngineCore::CreateEntity(const dString& name)
	{
		ProfileFunc();
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to create an entity while EngineCore was not initialized");
			return ID::invalidID;
		}
#endif // _DEBUG

		EntityID id = m_entityManager.CreateEntity();

		//Add mandatory components
		AddComponent<TransformComponent>(id);
		AddComponent<BindingComponent>(id);

		m_modifiedEntities.insert(id);

		m_sceneGraph.AddNode(id, name);
		return id;
	}

	void EngineCore::RemoveEntity(EntityID id)
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to remove an entity while EngineCore was not initialized");
			return;
		}
#endif // _DEBUG
		Renderer& renderer{ Renderer::GetInstance() };

		if (const GraphicsComponent * pGraphicsComponent{ GetComponent<GraphicsComponent>(id) })
			renderer.RemoveGraphicsElement(id, pGraphicsComponent->mesh);

		if (const PointLightComponent* pPointComponent{ GetComponent<PointLightComponent>(id) })
			renderer.RemovePointLight(id);

		if (const DirectionalLightComponent * pDirectionalLight{ GetComponent<DirectionalLightComponent>(id) })
			renderer.RemoveDirectionalLight(id);

		m_entityManager.RemoveEntity(id);
		m_modifiedEntities.erase(id);
		m_sceneGraph.DeleteNode(id);
	}

	const CameraComponent* EngineCore::GetCamera()
	{
		return GetComponent<CameraComponent>(m_cameraID);
	}

	CameraComponent* EngineCore::ModifyCamera()
	{
		return ModifyComponent<CameraComponent>(m_cameraID);
	}

	Handle<Mesh> EngineCore::GetDefaultMesh()
	{
		return Renderer::GetInstance().GetDefaultMesh();
	}

	void EngineCore::UpdateCamera()
	{
		ProfileFunc();
		//TODO: Input and transformation should be decoupled so we can compute new transformation only when it changed
		if (!m_entityManager.IsValid(m_cameraID))
			return;

		dVec3 translate{ 0.f,0.f,0.f };
		dVec3 rotation{ 0.f,0.f,0.f };
		bool cameraHasMoved = false;

		//Get input
		if (Input::GetKey(KeyCode::Q))
		{
			translate.x = -1.f;
			cameraHasMoved = true;
		}
		if (Input::GetKey(KeyCode::D))
		{
			translate.x = 1.f;
			cameraHasMoved = true;
		}

		if (Input::GetKey(KeyCode::A))
		{
			translate.y = -1.f;
			cameraHasMoved = true;
		}
		if (Input::GetKey(KeyCode::E))
		{
			translate.y = 1.f;
			cameraHasMoved = true;
		}

		if (Input::GetKey(KeyCode::Z))
		{
			translate.z = 1.f;
			cameraHasMoved = true;
		}
		if (Input::GetKey(KeyCode::S))
		{
			translate.z = -1.f;
			cameraHasMoved = true;
		}
		if (Input::GetMouseButton(2))
		{
			rotation.x = Input::GetMouseDeltaY();
			rotation.y = Input::GetMouseDeltaX();
			cameraHasMoved = true;
		}


		if ((m_modifiedEntities.find(m_cameraID) == m_modifiedEntities.end()) && !cameraHasMoved)
		{
			return;
		}

		TransformComponent* cameraTransform = ModifyComponent<TransformComponent>(m_cameraID);
		Assert(cameraTransform);
		CameraComponent* camera = ModifyComponent<CameraComponent>(m_cameraID);
		Assert(camera);

		const float clampedDeltaTime = DirectX::XMMin(m_deltaTime, 0.033f);

		//Add rotation
		constexpr float turnSpeed = DirectX::XMConvertToRadians(45.f);
		constexpr float xRotationClampValues[] { DirectX::XMConvertToRadians(-89.99f), DirectX::XMConvertToRadians(89.99f) };
		cameraTransform->rotation.x = std::clamp(std::fmodf(cameraTransform->rotation.x + rotation.x * turnSpeed * clampedDeltaTime, DirectX::XM_2PI), xRotationClampValues[0], xRotationClampValues[1]);
		cameraTransform->rotation.y = std::fmodf(cameraTransform->rotation.y + rotation.y * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
		cameraTransform->rotation.z = std::fmodf(cameraTransform->rotation.z + rotation.z * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);

		//Compute quaternion from camera rotation
		DirectX::XMVECTOR quat{ DirectX::XMQuaternionRotationRollPitchYaw(cameraTransform->rotation.x, cameraTransform->rotation.y, cameraTransform->rotation.z) };
		quat = DirectX::XMQuaternionNormalize(quat);

		//Apply camera rotation to translation
		DirectX::XMStoreFloat3(&translate, 
			DirectX::XMVector3Rotate(
				DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&translate)),
				quat
			)
		);

		//Apply translation
		const float speed = (Input::GetKey(KeyCode::ShiftKey))? 25.f:5.f;
		cameraTransform->position.x += translate.x * speed * clampedDeltaTime;
		cameraTransform->position.y += translate.y * speed * clampedDeltaTime;
		cameraTransform->position.z += translate.z * speed * clampedDeltaTime;

		//Compute camera view matrix
		DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&cameraTransform->position);
		DirectX::XMVECTOR at{ 0, 0, 1, 0 };
		DirectX::XMVECTOR up{ 0, 1, 0, 0 };
		at = DirectX::XMVector3Rotate(at, quat);
		camera->viewMatrix = DirectX::XMMatrixLookToLH(eye, at, up);
	}

	void EngineCore::UpdateTransforms()
	{
		ProfileFunc();
		for (const EntityID entity: m_modifiedEntities)
		{
			if (TransformComponent* transform = ModifyComponent<TransformComponent>(entity))
			{
				dMatrix modelMatrix = DirectX::XMMatrixIdentity();
				modelMatrix = DirectX::XMMatrixMultiply(modelMatrix, DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&transform->scale)));
				modelMatrix = DirectX::XMMatrixMultiply(modelMatrix, DirectX::XMMatrixRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&transform->rotation)));
				modelMatrix = DirectX::XMMatrixMultiply(modelMatrix, DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform->position)));

				transform->matrix = modelMatrix;
			}
		}
	}

	void EngineCore::DrawMainMenuBar()
	{
		ProfileFunc();
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Main"))
			{
				if (ImGui::MenuItem("Show ImGui demo"))
				{
					m_showImGuiDemo = true;
				}
				if (ImGui::MenuItem("Show scene"))
				{
					m_showScene = true;
				}
				if (ImGui::MenuItem("Show inspector"))
				{
					m_showInspector = true;
				}
				ImGui::EndMenu();
			}
			
			{
				static float framerate = 0.0f;
				framerate += (1 / m_deltaTime);
				framerate /= 2;
				dString FPSlabel = dStringUtils::printf("FPS : %.0f", framerate);
				ImGui::BeginMenu(FPSlabel.c_str(), false);
			}
			
			ImGui::EndMainMenuBar();
		}
	}

	void EngineCore::DrawInterface()
	{
		ProfileFunc();
		if (m_showImGuiDemo)
			ImGui::ShowDemoWindow(&m_showImGuiDemo);

		DrawMainMenuBar();
		if(m_showScene)
			DrawScene();
		if (m_showInspector)
			DrawInspector();
	}

	void EngineCore::DrawScene()
	{
		ProfileFunc();
		static float spawnRadius = 500.f;
		ImGui::Begin("Scene", &m_showScene);
		ImGui::Text("Scene graph");
		ImGui::SameLine();
		if (ImGui::Button("Add Entity"))
		{
			ImGui::OpenPopup("##AddEntity");
		}

		if (ImGui::BeginPopup("##AddEntity"))
		{
			if (ImGui::Button("Empty"))
			{
				EntityID id = CreateEntity("New entity");
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Point light"))
			{
				EntityID id = CreateEntity("Point light");
				AddComponent<PointLightComponent>(id);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Directional light"))
			{
				EntityID id = CreateEntity("Directional light");
				AddComponent<DirectionalLightComponent>(id);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Cube"))
			{
				EntityID id = CreateEntity("Cube");
				AddComponent<GraphicsComponent>(id).mesh = Renderer::GetInstance().GetDefaultMesh();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Add 5000 GraphicsEntities"))
		{
			Profile("Add 5000 GraphicsEntities")

			for (int i = 0; i < 5000; i++)
			{
				EntityID id = CreateEntity("New entity");
				AddComponent<GraphicsComponent>(id).mesh = Renderer::GetInstance().GetDefaultMesh();

				TransformComponent* transform = ModifyComponent<TransformComponent>(id);
				float LO = -spawnRadius;
				float HI = spawnRadius;

				float u = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
				float v = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
				float theta = u * 2.0f * DirectX::XM_PI;
				float phi = acosf(2.0f * v - 1.0f);
				float r = cbrtf(static_cast <float> (rand()) / static_cast <float> (RAND_MAX));
				float sinTheta = sinf(theta);
				float cosTheta = cosf(theta);
				float sinPhi = sinf(phi);
				float cosPhi = cosf(phi);
				transform->position.x = spawnRadius * r * sinPhi * cosTheta;
				transform->position.y = spawnRadius * r * sinPhi * sinTheta;
				transform->position.z = spawnRadius * r * cosPhi;

				constexpr float maxRotation = 180;
				constexpr float minRotation = -180;
				constexpr float rotationRatio{ RAND_MAX / (maxRotation - minRotation) };
				
				transform->rotation.x = minRotation + static_cast <float> (rand()) / rotationRatio;
				transform->rotation.y = minRotation + static_cast <float> (rand()) / rotationRatio;
				transform->rotation.z = minRotation + static_cast <float> (rand()) / rotationRatio;

				constexpr float maxScale = 1.5;
				constexpr float minScale = 0.5;
				constexpr float scaleRatio{ RAND_MAX / (maxScale - minScale)};

				transform->scale.x = minScale + static_cast <float> (rand()) / scaleRatio;
				transform->scale.y = minScale + static_cast <float> (rand()) / scaleRatio;
				transform->scale.z = minScale + static_cast <float> (rand()) / scaleRatio;
			}
		}

		ImGui::SameLine();
		const bool wantToDeleteEntity = ImGui::IsWindowFocused() && Input::GetKey(KeyCode::Delete);
		if ((ImGui::Button("Remove Entity") || wantToDeleteEntity) && (ID::IsValid(m_selectedEntity)))
		{
			//Check if m_selectedEntity was deleted but not set to invalidID
			Assert(m_entityManager.IsValid(m_selectedEntity));

			RemoveEntity(m_selectedEntity);
			m_selectedEntity = ID::invalidID;
		}

		ImGui::DragFloat("Spawn Radius", &spawnRadius, 1.f, 0, FLT_MAX, "%.0f", 1);
		
		static char path[512];
		ImGui::InputText("Mesh path", path, 512);
		if (ImGui::Button("Load and spawn mesh"))
		{
			dVector<Handle<Mesh>> sponzeMeshes { MeshLoader::Load(path)};
			for (auto meshHandle : sponzeMeshes)
			{
				EntityID id = CreateEntity("Mesh");
				AddComponent<GraphicsComponent>(id).mesh = meshHandle;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Reload shaders"))
		{
			Renderer::GetInstance().ReloadShaders();
		}

		ImGui::Separator();
		DrawGraph();
		ImGui::End();
	}

	void EngineCore::DrawGraph()
	{
		ImGui::BeginChild("##Graph");
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

		if (ImGui::TreeNodeEx((void*)(uintptr_t)ID::invalidID, flags, "Root"))
		{
			ImGuiListClipper clipper;
			const dList<SceneGraph::Node*>& nodes = m_sceneGraph.GetRoot()->GetChildren();
			clipper.Begin((int)nodes.size());
			while (clipper.Step())
			{
				dList<SceneGraph::Node*>::const_iterator it = m_sceneGraph.GetRoot()->GetChildren().begin();
				for (int i = 0; i < clipper.DisplayStart; i++)
				{
					it++;
				}

				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				{
					const SceneGraph::Node* child = *it;
					DrawNode(child);
					it++;
				}
			}


			ImGui::TreePop();
		}
		ImGui::EndChild();
	}

	void EngineCore::DrawNode(const SceneGraph::Node* node)
	{
		bool isSelected{ node->GetSelf() == m_selectedEntity };
		bool isOpen { false };

		//Create tree node
		if (!node->GetChildren().empty())
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ((isSelected) ? ImGuiTreeNodeFlags_Selected : 0);
			isOpen = ImGui::TreeNodeEx((void*)(uintptr_t)node->GetSelf(), flags, node->GetName().c_str());
		}
		else
		{
			ImGui::Selectable(node->GetName().c_str(), isSelected);
		}

		//Select on click
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			m_selectedEntity = node->GetSelf();
		}

		//Draw children if opened
		if (isOpen)
		{
			for (const SceneGraph::Node* child : node->GetChildren())
			{
				DrawNode(child);
			}

			ImGui::TreePop();
		}
	}
	void EngineCore::DrawInspector()
	{
		ProfileFunc();
		ImGui::Begin("Inspector", &m_showInspector);

		if (ID::IsValid(m_selectedEntity))
		{
			Assert(m_entityManager.IsValid(m_selectedEntity));

			const SceneGraph::Node* node = m_sceneGraph.GetNode(m_selectedEntity);
			Assert(node);

			ImGui::Text("%s", node->GetName().c_str());
			ImGui::Separator();

			if (TransformComponent* transform = ModifyComponent<TransformComponent>(m_selectedEntity))
			{
				if (ImGui::TreeNodeEx("Transform :"))
				{
					dVec3& pos = transform->position;
					float imGuiPos[3] = { pos.x, pos.y, pos.z };
					if (ImGui::DragFloat3("Position", imGuiPos, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f"))
					{
						pos.x = imGuiPos[0];
						pos.y = imGuiPos[1];
						pos.z = imGuiPos[2];
					}

					dVec3& rot = transform->rotation;
					float imGuiRot[3] = { DirectX::XMConvertToDegrees(rot.x),DirectX::XMConvertToDegrees(rot.y), DirectX::XMConvertToDegrees(rot.z) };
					if (ImGui::DragFloat3("Rotation", imGuiRot, 0.25f, -FLT_MAX, +FLT_MAX, "%.2f"))
					{
						rot.x = std::fmodf(DirectX::XMConvertToRadians(imGuiRot[0]), DirectX::XM_2PI);
						rot.y = std::fmodf(DirectX::XMConvertToRadians(imGuiRot[1]), DirectX::XM_2PI);
						rot.z = std::fmodf(DirectX::XMConvertToRadians(imGuiRot[2]), DirectX::XM_2PI);
					}

					dVec3& scale = transform->scale;
					float imGuiScale[3] = { scale.x, scale.y, scale.z };
					if (ImGui::DragFloat3("Scale", imGuiScale, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f"))
					{
						scale.x = imGuiScale[0];
						scale.y = imGuiScale[1];
						scale.z = imGuiScale[2];
					}
					ImGui::TreePop();
				}
			}


			if (CameraComponent* camera = ModifyComponent<CameraComponent>(m_selectedEntity))
			{
				if (ImGui::TreeNodeEx("Camera :"))
				{
					float verticalFieldOfView = camera->verticalFieldOfView;
					if (ImGui::DragFloat("Vertical field of view", &verticalFieldOfView, 0.05f, 5.f, 179.999f, "%f", ImGuiSliderFlags_AlwaysClamp))
					{
						camera->verticalFieldOfView = verticalFieldOfView;
					}
					ImGui::TreePop();
				}
			}

			if (GraphicsComponent* graphicsComponent = ModifyComponent<GraphicsComponent>(m_selectedEntity))
			{
				if (ImGui::TreeNodeEx("Material :"))
				{
					float imGuiBaseColor[3] = { graphicsComponent->material->m_baseColor.x, graphicsComponent->material->m_baseColor.y, graphicsComponent->material->m_baseColor.z };
					ImGui::ColorPicker3("BaseColor", imGuiBaseColor);
					graphicsComponent->material->m_baseColor.x = imGuiBaseColor[0];
					graphicsComponent->material->m_baseColor.y = imGuiBaseColor[1];
					graphicsComponent->material->m_baseColor.z = imGuiBaseColor[2];

					ImGui::TreePop();
				}
			}
			else
			{
				if (ImGui::Button("Add GraphicsComponent"))
				{
					AddComponent<GraphicsComponent>(m_selectedEntity).mesh = Renderer::GetInstance().GetDefaultMesh();
				}
			}

			if (PointLightComponent* pointLightComponent = ModifyComponent<PointLightComponent>(m_selectedEntity))
			{
				if (ImGui::TreeNodeEx("Point light attributes :"))
				{
					float imGuiIntensity = pointLightComponent->intensity;
					ImGui::DragFloat("Intensity", &imGuiIntensity, 0.01f, -FLT_MAX, +FLT_MAX, "%.2f");
					pointLightComponent->intensity = imGuiIntensity;

					float imGuiRadius = pointLightComponent->radius;
					ImGui::DragFloat("Radius", &imGuiRadius, 0.01f, -FLT_MAX, +FLT_MAX, "%.2f");
					pointLightComponent->radius = imGuiRadius;

					float imGuiBaseColor[3] = { pointLightComponent->color.x, pointLightComponent->color.y, pointLightComponent->color.z };
					ImGui::ColorPicker3("Color", imGuiBaseColor);
					pointLightComponent->color.x = imGuiBaseColor[0];
					pointLightComponent->color.y = imGuiBaseColor[1];
					pointLightComponent->color.z = imGuiBaseColor[2];

					ImGui::TreePop();
				}

			}
			else
			{
				if (ImGui::Button("Add PointLightComponent"))
				{
					AddComponent<PointLightComponent>(m_selectedEntity);
				}
			}

			if (DirectionalLightComponent* pointLightComponent = ModifyComponent<DirectionalLightComponent>(m_selectedEntity))
			{
				if (ImGui::TreeNodeEx("Directional light attributes :"))
				{
					float imGuiIntensity = pointLightComponent->intensity;
					ImGui::DragFloat("Intensity", &imGuiIntensity, 0.01f, -FLT_MAX, +FLT_MAX, "%.2f");
					pointLightComponent->intensity = imGuiIntensity;

					float imGuiBaseColor[3] = { pointLightComponent->color.x, pointLightComponent->color.y, pointLightComponent->color.z };
					ImGui::ColorPicker3("Color", imGuiBaseColor);
					pointLightComponent->color.x = imGuiBaseColor[0];
					pointLightComponent->color.y = imGuiBaseColor[1];
					pointLightComponent->color.z = imGuiBaseColor[2];

					ImGui::TreePop();
				}
			}
			else
			{
				if (ImGui::Button("Add DirectionalLightComponent"))
				{
					AddComponent<DirectionalLightComponent>(m_selectedEntity);
				}
			}
		}
		
		ImGui::End();
	}

	void EngineCore::UpdateGraphicsData()
	{
		ProfileFunc();

		Renderer& renderer{ Renderer::GetInstance() };

		if (m_modifiedEntities.find(m_cameraID) != m_modifiedEntities.end())
		{
			renderer.UpdateCamera(GetComponent<CameraComponent>(m_cameraID), GetComponent<TransformComponent>(m_cameraID)->position);
		}

		// should we track modified component instead of modified entities ?
		for (const EntityID entity : m_modifiedEntities)
		{
			if (const GraphicsComponent* pGraphicsComponent = GetComponent<GraphicsComponent>(entity))
			{
				if (pGraphicsComponent->mesh.IsValid())
				{
					const TransformComponent* pTransformComponent{ GetComponent<TransformComponent>(entity) };
					Assert(pTransformComponent);

					const dMatrix& transformMatrix{ pTransformComponent->matrix };

					InstanceData instanceData;
					DirectX::XMStoreFloat4x4(&instanceData.modelMatrix, transformMatrix);
					DirectX::XMStoreFloat4x4(&instanceData.normalMatrix, XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, transformMatrix)));
					instanceData.baseColor = pGraphicsComponent->material->m_baseColor;

					renderer.SubmitGraphicsElement(entity, pGraphicsComponent->mesh, instanceData);
				}
			}

			if (const PointLightComponent* pPointLightComponent = GetComponent<PointLightComponent>(entity))
			{
				const TransformComponent* pTransformComponent = GetComponent<TransformComponent>(entity);
				
				renderer.SubmitPointLight(entity, pPointLightComponent->color, pPointLightComponent->intensity, pTransformComponent->position, pPointLightComponent->radius);
			}

			if (const DirectionalLightComponent* pDirectionalLightComponent = GetComponent<DirectionalLightComponent>(entity))
			{
				const TransformComponent* pTransformComponent = GetComponent<TransformComponent>(entity);
				
				DirectX::XMVECTOR quat{ DirectX::XMQuaternionRotationRollPitchYaw(pTransformComponent->rotation.x, pTransformComponent->rotation.y, pTransformComponent->rotation.z) };
				quat = DirectX::XMQuaternionNormalize(quat);

				// Compute camera view matrix
				// TODO : The directional light should moved with the camera to center the shadow map on the camera position
				DirectX::XMVECTOR at{ DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 0.f, 0.f, 1.f, 0.f }, quat)) };
				DirectX::XMVECTOR up{ 0.f, 1.f, 0.f, 0.f};
				DirectX::XMVECTOR eye {DirectX::XMVectorScale(at, -500.f)};
				dMatrix viewMatrix{ DirectX::XMMatrixLookAtLH(eye, at, up) };

				dMatrix projectionMatrix{ DirectX::XMMatrixOrthographicLH(500.f, 500.f, 1.f, 1000.0f) };

				dMatrix4x4 viewProj; 
				DirectX::XMStoreFloat4x4(&viewProj, viewMatrix * projectionMatrix);

				dVec3 dir{};
				DirectX::XMStoreFloat3(&dir, at);

				renderer.SubmitDirectionalLight(entity, pDirectionalLightComponent->color, dir, pDirectionalLightComponent->intensity, viewProj);
			}
		}
	}

	void EngineCore::ClearModifiedEntities()
	{
		ProfileFunc();
		m_modifiedEntities.clear();
	}

}