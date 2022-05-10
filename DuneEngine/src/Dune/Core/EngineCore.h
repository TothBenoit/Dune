#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
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

		static EntityID CreateEntity();
		static void RemoveEntity(EntityID);
		static void DrawGraph();

		inline static bool IsInitialized() { return m_isInitialized; }
		inline static EntityManager* GetEntityManager() { return m_entityManager.get(); }

	private:
		static bool m_isInitialized;
		static std::unique_ptr<EntityManager> m_entityManager;
		static std::unique_ptr<ComponentManager<TransformComponent>> m_transformManager;
		static SceneGraph m_sceneGraph;
	};
}

