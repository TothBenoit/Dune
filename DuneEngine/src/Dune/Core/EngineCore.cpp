#include "pch.h"
#include "EngineCore.h"
#include "Dune/Core/Input.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"
#include "Dune/Core/ECS/Components/PointLightComponent.h"
#include "Dune/Core/ECS/Components/DirectionalLightComponent.h"

#include <DirectXMath.h>

namespace Dune
{

	void EngineCore::Init(const Window* pWindow)
	{
		if (m_isInitialized)
		{
			LOG_CRITICAL("Tried to initialize EngineCore which was already initialized");
			return;
		}

		m_entityManager = std::make_unique<EntityManager>();

		ComponentManager<TransformComponent>::Init();
		ComponentManager<BindingComponent>::Init();
		ComponentManager<GraphicsComponent>::Init();
		ComponentManager<CameraComponent>::Init();
		ComponentManager<PointLightComponent>::Init();
		ComponentManager<DirectionalLightComponent>::Init();

		m_graphicsRenderer = GraphicsRenderer::Create(pWindow);

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
		m_graphicsRenderer->OnShutdown();
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
		rmt_ScopedCPUSample(EngineCoreUpdate, 0);

		m_deltaTime = dt;
		DrawInterface();
		UpdateCamera();
		UpdateTransforms();
		UpdateGraphicsData();
		ClearModifiedEntities();

		m_graphicsRenderer->Render();
	}

	EntityID EngineCore::CreateEntity(const dString& name)
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to create an entity while EngineCore was not initialized");
			return ID::invalidID;
		}
#endif // _DEBUG

		EntityID id = m_entityManager->CreateEntity();

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
		m_entityManager->RemoveEntity(id);
		m_modifiedEntities.erase(id);
		m_sceneGraph.DeleteNode(id);
		m_graphicsRenderer->RemoveGraphicsElement(id);
		m_graphicsRenderer->RemovePointLight(id);
		m_graphicsRenderer->RemoveDirectionalLight(id);
	}

	void EngineCore::UpdateCamera()
	{
		rmt_ScopedCPUSample(UpdateCamera, 0);
		//TODO: Input and transformation should be decoupled so we can compute new transformation only when it changed
		if (!m_entityManager->IsValid(m_cameraID))
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
			rotation.x = (float)Input::GetMouseDeltaY();
			rotation.y = (float)Input::GetMouseDeltaX();
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
		cameraTransform->rotation.x = std::fmodf(cameraTransform->rotation.x + rotation.x * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
		cameraTransform->rotation.y = std::fmodf(cameraTransform->rotation.y + rotation.y * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
		cameraTransform->rotation.z = std::fmodf(cameraTransform->rotation.z + rotation.z * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
		
		//Compute quaternion from camera rotation
		DirectX::XMVECTOR quat = DirectX::XMQuaternionIdentity();
		DirectX::XMVECTOR x = DirectX::XMQuaternionRotationRollPitchYaw(cameraTransform->rotation.x, 0, 0);
		DirectX::XMVECTOR y = DirectX::XMQuaternionRotationRollPitchYaw(0, cameraTransform->rotation.y, 0);
		DirectX::XMVECTOR z = DirectX::XMQuaternionRotationRollPitchYaw(0, 0, cameraTransform->rotation.z);
		quat = DirectX::XMQuaternionMultiply(y, quat);
		quat = DirectX::XMQuaternionMultiply(x, quat);
		quat = DirectX::XMQuaternionMultiply(z, quat);
		quat = DirectX::XMQuaternionNormalize(quat);

		//Apply camera rotation to translation
		DirectX::XMStoreFloat3(&translate, 
			DirectX::XMVector3TransformNormal(
				DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&translate)),
				DirectX::XMMatrixRotationQuaternion(quat)
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
		at = DirectX::XMVector3TransformNormal(at, DirectX::XMMatrixRotationQuaternion(quat));
		up = DirectX::XMVector3TransformNormal(up, DirectX::XMMatrixRotationQuaternion(quat));
		camera->viewMatrix = DirectX::XMMatrixLookToLH(eye, at, up);
	}

	void EngineCore::UpdateTransforms()
	{
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
		if (m_showImGuiDemo)
			ImGui::ShowDemoWindow(&m_showImGuiDemo);

		rmt_ScopedCPUSample(DrawInterface, 0);

		DrawMainMenuBar();
		if(m_showScene)
			DrawScene();
		if (m_showInspector)
			DrawInspector();
	}

	void EngineCore::DrawScene()
	{
		static float spawnRadius = 100.f;
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
				AddComponent<GraphicsComponent>(id);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Add 1000 GraphicsEntity"))
		{
			for (int i = 0; i < 1000; i++)
			{
				EntityID id = CreateEntity("New entity");
				AddComponent<GraphicsComponent>(id);
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

				LO = -180.f;
				HI = 180.f;

				transform->rotation.x = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));;
				transform->rotation.y = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));
				transform->rotation.z = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));

				LO = 0.5f;
				HI = 1.5f;

				transform->scale.x = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));;
				transform->scale.y = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));
				transform->scale.z = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));
			}
		}

		ImGui::SameLine();
		const bool wantToDeleteEntity = ImGui::IsWindowFocused() && Input::GetKey(KeyCode::Delete);
		if ((ImGui::Button("Remove Entity") || wantToDeleteEntity) && (ID::IsValid(m_selectedEntity)))
		{
			//Check if m_selectedEntity was deleted but not set to invalidID
			Assert(m_entityManager->IsValid(m_selectedEntity));

			RemoveEntity(m_selectedEntity);
			m_selectedEntity = ID::invalidID;
		}

		ImGui::DragFloat("Spawn Radius", &spawnRadius, 1.f, 0, FLT_MAX, "%.0f", 1);

		ImGui::Separator();
		DrawGraph();
		ImGui::End();
	}

	void EngineCore::DrawGraph()
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (ImGui::TreeNodeEx((void*)(uintptr_t)ID::invalidID, flags, "Root"))
		{
			for (const SceneGraph::Node* child : m_sceneGraph.GetRoot()->GetChildren())
			{
				DrawNode(child);
			}
			ImGui::TreePop();
		}
	}

	void EngineCore::DrawNode(const SceneGraph::Node* node)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

		//if selected, highlight it
		if (node->GetSelf() == m_selectedEntity)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		//Create tree node
		bool isOpen = ImGui::TreeNodeEx((void*)(uintptr_t)node->GetSelf(), flags, node->GetName().c_str());

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
		ImGui::Begin("Inspector", &m_showInspector);

		if (ID::IsValid(m_selectedEntity))
		{
			Assert(m_entityManager->IsValid(m_selectedEntity));

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
					AddComponent<GraphicsComponent>(m_selectedEntity);
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
		rmt_ScopedCPUSample(UpdateGraphicsData, 0);

		if (m_modifiedEntities.find(m_cameraID) != m_modifiedEntities.end())
		{
			m_graphicsRenderer->UpdateCamera();
		}

		// should we track modified component instead of modified entities ?
		for (const EntityID entity : m_modifiedEntities)
		{
			if (const GraphicsComponent* graphicsComponent = GetComponent<GraphicsComponent>(entity))
			{
				const TransformComponent* transformComponent = GetComponent<TransformComponent>(entity);
				Assert(transformComponent);

				// TODO : Find when we should upload mesh
				// Once when loaded I guess
				if (!graphicsComponent->mesh->IsUploaded())
				{
					graphicsComponent->mesh->UploadBuffers();
				}

				m_graphicsRenderer->SubmitGraphicsElement(entity, GraphicsElement(graphicsComponent->mesh, graphicsComponent->material, transformComponent->matrix));
			}

			if (const PointLightComponent* pointLightComponent = GetComponent<PointLightComponent>(entity))
			{
				const TransformComponent* transformComponent = GetComponent<TransformComponent>(entity);
				
				m_graphicsRenderer->SubmitPointLight(entity, PointLight(pointLightComponent->color, pointLightComponent->intensity, pointLightComponent->radius, transformComponent->position));
			}

			if (const DirectionalLightComponent* directionalLightComponent = GetComponent<DirectionalLightComponent>(entity))
			{
				const TransformComponent* transformComponent = GetComponent<TransformComponent>(entity);
				
				DirectX::XMVECTOR quat = DirectX::XMQuaternionIdentity();
				DirectX::XMVECTOR x = DirectX::XMQuaternionRotationRollPitchYaw(transformComponent->rotation.x, 0, 0);
				DirectX::XMVECTOR y = DirectX::XMQuaternionRotationRollPitchYaw(0, transformComponent->rotation.y, 0);
				DirectX::XMVECTOR z = DirectX::XMQuaternionRotationRollPitchYaw(0, 0, transformComponent->rotation.z);
				quat = DirectX::XMQuaternionMultiply(y, quat);
				quat = DirectX::XMQuaternionMultiply(x, quat);
				quat = DirectX::XMQuaternionMultiply(z, quat);
				quat = DirectX::XMQuaternionNormalize(quat);
				
				dVec3 dir{ 0.f,0.f,1.f };
				//Apply camera rotation to translation
				DirectX::XMStoreFloat3(&dir,
					DirectX::XMVector3TransformNormal(
						DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir)),
						DirectX::XMMatrixRotationQuaternion(quat)
					)
				);

				m_graphicsRenderer->SubmitDirectionalLight(entity, DirectionalLight(directionalLightComponent->color, directionalLightComponent->intensity, dir));
			}


		}
	}

	void EngineCore::ClearModifiedEntities()
	{
		m_modifiedEntities.clear();
	}

}