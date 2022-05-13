#include "pch.h"
#include "EngineCore.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"

namespace Dune
{
	std::unique_ptr<EntityManager> EngineCore::m_entityManager = nullptr;
	std::unique_ptr<ComponentManager<TransformComponent>> EngineCore::m_transformManager = nullptr;
	std::unique_ptr<ComponentManager<GraphicsComponent>> EngineCore::m_graphicsManager = nullptr;
	std::unique_ptr<ComponentManager<BindingComponent>> EngineCore::m_bindingManager = nullptr;
	bool EngineCore::m_isInitialized = false;
	SceneGraph EngineCore::m_sceneGraph;
	EntityID EngineCore::m_selectedEntity;
	bool EngineCore::m_showImGuiDemo;

	void EngineCore::Init()
	{
		if (m_isInitialized)
		{
			LOG_CRITICAL("Tried to initialize EngineCore which was already initialized");
			return;
		}

		m_entityManager = std::make_unique<EntityManager>();

		m_transformManager = std::make_unique<ComponentManager<TransformComponent>>();
		m_bindingManager = std::make_unique<ComponentManager<BindingComponent>>();
		m_graphicsManager = std::make_unique<ComponentManager<GraphicsComponent>>();

		m_isInitialized = true;
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

	void EngineCore::Update()
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to update EngineCore which was not initialized");
			return;
		}
#endif // _DEBUG


		DrawMainMenuBar();
		DrawSceneGraphInterface();
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

	void EngineCore::DrawSceneGraphInterface()
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
}