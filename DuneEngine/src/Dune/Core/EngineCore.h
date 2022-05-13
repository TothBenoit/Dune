#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"

namespace Dune
{

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
		inline static ComponentManager<TransformComponent>* const GetTransformManager() { return m_transformManager.get(); }
		inline static ComponentManager<BindingComponent>* const GetBindingManager() { return m_bindingManager.get(); }
		inline static ComponentManager<GraphicsComponent>* const GetGraphicsManager() { return m_graphicsManager.get(); }

	private:
		static void DrawMainMenuBar();
		static void DrawInterface();
		static void DrawScene();
		static void DrawGraph();
		static void DrawNode(const SceneGraph::Node* node);
		static void DrawInspector();
	private:
		static bool m_isInitialized;
		static std::unique_ptr<EntityManager> m_entityManager;

		//Should ComponentManager be handle in entity manager ? EngineCore:: could implement GetComponent<> AddComponent<> ...
		//Pros : - Easy to established a component list - clean interfacing 
		//Cons : - I have no idea how to implement a templated GetComponent
		static std::unique_ptr<ComponentManager<TransformComponent>> m_transformManager;
		static std::unique_ptr<ComponentManager<BindingComponent>> m_bindingManager;
		static std::unique_ptr<ComponentManager<GraphicsComponent>> m_graphicsManager;

		static SceneGraph m_sceneGraph;
		static EntityID m_selectedEntity;
		static bool m_showImGuiDemo;
	};
}

