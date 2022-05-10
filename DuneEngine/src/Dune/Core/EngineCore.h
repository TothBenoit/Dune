#pragma once

#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/Transform.h"

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

		inline static bool IsInitialized() { return isInitialized; }
		inline static EntityManager* GetEntityManager() { return m_entityManager.get(); }

	private:
		static bool isInitialized;
		static std::unique_ptr<EntityManager> m_entityManager;
		static std::unique_ptr<ComponentManager<TransformComponent>> m_transformManager;
	};
}

