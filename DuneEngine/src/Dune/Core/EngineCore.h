#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/CameraComponent.h"

namespace Dune
{
	class EngineCore
	{
	public:
		EngineCore() = delete;
		~EngineCore() = delete;

		static void Init();
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
		static void SendDataToGraphicsCore();
		static void ClearTransformModifiedEntities();
	private:
		static bool m_isInitialized;
		static std::unique_ptr<EntityManager> m_entityManager;
		static SceneGraph m_sceneGraph;
		static EntityID m_selectedEntity;
		static bool m_showScene;
		static bool m_showInspector;
		static bool m_showImGuiDemo;
		static EntityID m_cameraID;
		static float m_deltaTime;
		//IDEA : map of modified entity containing a flag of which component has been modified
		static dSet<EntityID> m_modifiedEntities;
	};
}

