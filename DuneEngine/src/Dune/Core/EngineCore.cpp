#include "pch.h"
#include "EngineCore.h"
#include "Dune/Core/Input.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"

#include <DirectXMath.h>

namespace Dune
{
	std::unique_ptr<EntityManager> EngineCore::m_entityManager = nullptr;
	bool EngineCore::m_isInitialized = false;
	SceneGraph EngineCore::m_sceneGraph;
	EntityID EngineCore::m_selectedEntity;
	bool EngineCore::m_showImGuiDemo;
	EntityID EngineCore::m_cameraID;

	void EngineCore::Init()
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

		m_isInitialized = true;

		m_cameraID = CreateEntity("Camera");
		AddComponent<CameraComponent>(m_cameraID);
		CameraComponent* camera = GetComponent<CameraComponent>(m_cameraID);
		TransformComponent* cameraTransform = GetComponent<TransformComponent>(m_cameraID);

		DirectX::XMVECTOR quat = DirectX::XMQuaternionIdentity();
		DirectX::XMVECTOR x = DirectX::XMQuaternionRotationRollPitchYaw(cameraTransform->rotation.x, 0, 0);
		DirectX::XMVECTOR y = DirectX::XMQuaternionRotationRollPitchYaw(0, cameraTransform->rotation.y, 0);
		DirectX::XMVECTOR z = DirectX::XMQuaternionRotationRollPitchYaw(0, 0, cameraTransform->rotation.z);

		quat = DirectX::XMQuaternionMultiply(y, quat);
		quat = DirectX::XMQuaternionMultiply(x, quat);
		quat = DirectX::XMQuaternionMultiply(z, quat);
		quat = DirectX::XMQuaternionNormalize(quat);

		DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&cameraTransform->position);
		DirectX::XMVECTOR at = DirectX::XMVectorSet(0, 0, 1, 0);
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
		at = DirectX::XMVector3TransformNormal(at, DirectX::XMMatrixRotationQuaternion(quat));
		up = DirectX::XMVector3TransformNormal(up, DirectX::XMMatrixRotationQuaternion(quat));

		camera->viewMatrix = DirectX::XMMatrixLookToLH(eye, at, up);

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

		UpdateCamera(dt);
		DrawMainMenuBar();
		DrawInterface();
		if (m_showImGuiDemo)
			ImGui::ShowDemoWindow(&m_showImGuiDemo);
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
		m_sceneGraph.DeleteNode(id);
	}

	void EngineCore::UpdateCamera(float dt)
	{
		dVec3 translate{ 0.f,0.f,0.f };
		dVec3 rotation{ 0.f,0.f,0.f };

		//Get input
		//TODO: bind key to event (Need event system)
		if (Input::GetKey(KeyCode::Q))
		{
			translate.x = -0.1f;
		}
		if (Input::GetKey(KeyCode::D))
		{
			translate.x = 0.1f;
		}

		if (Input::GetKey(KeyCode::A))
		{
			translate.y = -0.1f;
		}
		if (Input::GetKey(KeyCode::E))
		{
			translate.y = 0.1f;
		}

		if (Input::GetKey(KeyCode::Z))
		{
			translate.z = 0.1f;
		}
		if (Input::GetKey(KeyCode::S))
		{
			translate.z = -0.1f;
		}
		if (Input::GetMouseButton(2))
		{
			rotation.x = (float) Input::GetMouseDeltaY() * 0.01f;
			rotation.y = (float) Input::GetMouseDeltaX() * 0.01f;
		}

		CameraComponent* camera = GetComponent<CameraComponent>(m_cameraID);
		TransformComponent* cameraTransform = GetComponent<TransformComponent>(m_cameraID);
		
		//Add rotation
		cameraTransform->rotation.x = std::fmodf(cameraTransform->rotation.x + rotation.x,DirectX::XM_2PI);
		cameraTransform->rotation.y = std::fmodf(cameraTransform->rotation.y + rotation.y, DirectX::XM_2PI);
		cameraTransform->rotation.z = std::fmodf(cameraTransform->rotation.z + rotation.z, DirectX::XM_2PI);
		
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
		const float speed = 3.f;
		cameraTransform->position.x += translate.x * speed * dt;
		cameraTransform->position.y += translate.y * speed * dt;
		cameraTransform->position.z += translate.z * speed * dt;

		//Compute camera view matrix
		DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&cameraTransform->position);
		DirectX::XMVECTOR at = DirectX::XMVectorSet(0, 0, 1, 0);
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
		at = DirectX::XMVector3TransformNormal(at, DirectX::XMMatrixRotationQuaternion(quat));
		up = DirectX::XMVector3TransformNormal(up, DirectX::XMMatrixRotationQuaternion(quat));
		camera->viewMatrix = DirectX::XMMatrixLookToLH(eye, at, up);

		//Compute camera projection matrix
		float aspectRatio = 1600.f / static_cast<float>(900.f);
		camera->projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.f), aspectRatio, 0.1f, 1000.0f);
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
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void EngineCore::DrawInterface()
	{
		DrawScene();
		DrawInspector();
	}

	void EngineCore::DrawScene()
	{
		static ID::IDType counter = 0;

		ImGui::Begin("Scene");
		ImGui::Text("Scene graph");
		ImGui::SameLine();
		if (ImGui::Button("Add Entity"))
		{
			CreateEntity(dStringUtils::printf("%u", counter++).c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove Entity") && (ID::IsValid(m_selectedEntity)))
		{
			RemoveEntity(m_selectedEntity);
			m_selectedEntity = ID::invalidID;
		}
		ImGui::Separator();
		DrawGraph();
		ImGui::End();
	}

	void EngineCore::DrawGraph()
	{
		for (const SceneGraph::Node* child : m_sceneGraph.GetRoot()->GetChildren())
		{
			DrawNode(child);
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
		ImGui::Begin("Inspector");
		if (const SceneGraph::Node* node = m_sceneGraph.GetNode(m_selectedEntity))
		{
			ImGui::Text("%s", node->GetName().c_str());
			ImGui::Separator();
			if (TransformComponent* transform = GetComponent<TransformComponent>(m_selectedEntity))
			{
				ImGui::Text("Transform :");

				dVec3& pos = transform->position;
				float imGuiPos[3] = { pos.x, pos.y, pos.z };
				ImGui::InputFloat3("Position", imGuiPos, "%.2f");
				pos.x = imGuiPos[0];
				pos.y = imGuiPos[1];
				pos.z = imGuiPos[2];

				dVec3& rot = transform->rotation;
				float imGuiRot[3] = { DirectX::XMConvertToDegrees(rot.x),DirectX::XMConvertToDegrees(rot.y), DirectX::XMConvertToDegrees(rot.z) };
				ImGui::InputFloat3("Rotation", imGuiRot, "%.2f");
				rot.x = DirectX::XMConvertToRadians(imGuiRot[0]);
				rot.y = DirectX::XMConvertToRadians(imGuiRot[1]);
				rot.z = DirectX::XMConvertToRadians(imGuiRot[2]);

				dVec3& scale = transform->scale;
				float imGuiScale[3] = { scale.x, scale.y, scale.z };
				ImGui::InputFloat3("Scale", imGuiScale, "%.2f");
				scale.x = imGuiScale[0];
				scale.y = imGuiScale[1];
				scale.z = imGuiScale[2];
			}
		}
		ImGui::End();
	}
}