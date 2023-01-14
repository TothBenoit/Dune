#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/CameraComponent.h"

namespace Dune
{
	class Window;

	class EngineCore
	{
	public:
		EngineCore() = delete;
		~EngineCore() = delete;

		static void Init(const Window* pWindow);
		static void Shutdown();
		static void Update(float dt);

		static EntityID CreateEntity(const dString& name);
		static void RemoveEntity(EntityID);

		static bool IsAlive(EntityID entity) { return m_entityManager->IsValid(entity); }

		template<typename Component>
		static void AddComponent(EntityID id)
		{
			Assert(m_entityManager->IsValid(id));
			m_modifiedEntities.insert(id);
			ComponentManager<Component>::Create(id);
		}

		template<typename Component>
		static const Component* GetComponent(EntityID id)
		{
			Assert(m_entityManager->IsValid(id));
			return ComponentManager<Component>::GetComponent(id);
		}

		template<typename Component>
		static Component* ModifyComponent(EntityID id)
		{
			Assert(m_entityManager->IsValid(id));
			m_modifiedEntities.insert(id);
			return ComponentManager<Component>::GetComponent(id);
		}

		template<typename Component>
		static void RemoveComponent(EntityID id)
		{
			Assert(m_entityManager->IsValid(id));
			m_modifiedEntities.insert(id);
			ComponentManager<Component>::Remove(id);
		}

		static const CameraComponent* GetCamera()
		{
			return GetComponent<CameraComponent>(m_cameraID);
		}

		static CameraComponent* ModifyCamera()
		{
			return ModifyComponent<CameraComponent>(m_cameraID);
		}

		static EntityID GetCameraID()
		{
			return m_cameraID;
		}

		inline static bool IsInitialized() { return m_isInitialized; }

	private:
		static void UpdateCamera();
		static void UpdateTransforms();
		static void DrawMainMenuBar();
		static void DrawInterface();
		static void DrawScene();
		static void DrawGraph();
		static void DrawNode(const SceneGraph::Node* node);
		static void DrawInspector();
		static void UpdateGraphicsData();
		static void ClearModifiedEntities();
	private:
		static inline bool m_isInitialized = false;
		static inline std::unique_ptr<EntityManager> m_entityManager = nullptr;
		static inline SceneGraph m_sceneGraph;
		static inline EntityID m_selectedEntity = ID::invalidID;
		static inline bool m_showScene = true;
		static inline bool m_showInspector = true;
		static inline bool m_showImGuiDemo = false;
		static inline EntityID m_cameraID = ID::invalidID;
		static inline float m_deltaTime = 0.f;
		static inline dHashSet<EntityID> m_modifiedEntities;
	};
}

