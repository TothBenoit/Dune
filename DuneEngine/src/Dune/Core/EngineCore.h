#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"

namespace Dune
{

	enum class EComponentType
	{
		TransformComponent,
		GraphicsComponent,
		BindingComponent,
	};

	class EngineCore
	{
	public:
		EngineCore() = delete;
		~EngineCore() = delete;

		static void Init();
		static void Shutdown();
		static void Update();

		static EntityID CreateEntity(const dString& name);
		static void RemoveEntity(EntityID);

		inline static bool IsInitialized() { return m_isInitialized; }
		inline static EntityManager* GetEntityManager() { return m_entityManager.get(); }

	private:

		static void DrawMainMenuBar();
		static void DrawSceneGraphInterface();
		static void DrawGraph();
		static void DrawNode(const SceneGraph::Node* node);

	private:
		static bool m_isInitialized;
		static std::unique_ptr<EntityManager> m_entityManager;
		static std::unique_ptr<ComponentManager<TransformComponent>> m_transformManager;
		static std::unique_ptr<ComponentManager<GraphicsComponent>> m_graphicsManager;
		static std::unique_ptr<ComponentManager<BindingComponent>> m_bindingManager;
		static SceneGraph m_sceneGraph;
		static EntityID m_selectedEntity;
		static bool m_showImGuiDemo;
	};
}

