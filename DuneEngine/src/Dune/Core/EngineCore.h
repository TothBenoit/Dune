#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Common/Handle.h"

namespace Dune
{
	class Window;
	class Mesh;
	
	struct Camera
	{
		dVec3 position{ 0.f,0.f,0.f };
		dVec3 rotation{ 0.f, 0.f, 0.f };

		dMatrix matrix;

		dMatrix viewMatrix;
		float	verticalFieldOfView{ 85.f };
	};

	class EngineCore
	{
	public:
		EngineCore() = delete;
		~EngineCore() = delete;

		static void Init(const Window* pWindow);
		static void Shutdown();
		static void Update(float dt);

		[[nodiscard]] static EntityID CreateEntity(const dString& name);
		static void RemoveEntity(EntityID);

		[[nodiscard]] static bool IsAlive(EntityID entity) { return m_entityManager.IsValid(entity); }

		template<typename Component>
		static Component& AddComponent(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			m_modifiedEntities.insert(id);
			return ComponentManager<Component>::Create(id);
		}

		template<typename Component>
		[[nodiscard]] static const Component* GetComponent(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			return ComponentManager<Component>::GetComponent(id);
		}

		template<typename Component>
		[[nodiscard]] static Component* ModifyComponent(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			m_modifiedEntities.insert(id);
			return ComponentManager<Component>::GetComponent(id);
		}

		template<typename Component>
		[[nodiscard]] static const Component& GetComponentUnsafe(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			return ComponentManager<Component>::GetComponentUnsafe(id);
		}

		template<typename Component>
		[[nodiscard]] static Component& ModifyComponentUnsafe(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			m_modifiedEntities.insert(id);
			return ComponentManager<Component>::GetComponentUnsafe(id);
		}		

		template<typename Component>
		static void RemoveComponent(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			if (ComponentManager<Component>::Contains(id))
			{
				m_modifiedEntities.insert(id);
				ComponentManager<Component>::Remove(id);
			}
		}

		template<typename Component>
		static void RemoveComponentUnsafe(EntityID id)
		{
			Assert(m_entityManager.IsValid(id));
			m_modifiedEntities.insert(id);
			ComponentManager<Component>::Remove(id);
		}

		[[nodiscard]] static const Camera& GetCamera() { return m_camera; };
		[[nodiscard]] static Camera& ModifyCamera() { return m_camera; };

		[[nodiscard]] static Handle<Mesh> GetDefaultMesh();

		[[nodiscard]] inline static bool IsInitialized() { return m_isInitialized; }

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
		static inline EntityManager m_entityManager;
		static inline SceneGraph m_sceneGraph;
		static inline EntityID m_selectedEntity{ ID::invalidID };
		static inline bool m_showScene = true;
		static inline bool m_showInspector = true;
		static inline bool m_showImGuiDemo = false;
		static inline Camera m_camera;
		static inline float m_deltaTime = 0.f;
		static inline dHashSet<EntityID> m_modifiedEntities;
	};
}

