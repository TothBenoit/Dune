#pragma once

#include <EnTT/entt.hpp>
#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct Transform;

	enum class EntityID : dU32 {};

	template<typename T>
	concept Component = std::is_same_v<T, std::remove_cvref_t<T>> && std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

	template<typename T>
	inline constexpr bool kIsTracked = requires { requires T::kTrackChanges; };

	template<typename T>
	concept TrackedComponent = Component<T> && kIsTracked<T>;

	template<typename T>
	using ComponentAccess = std::conditional_t<kIsTracked<T>, const T, T>;

	inline std::atomic<dU32> g_trackedComponentCount{ 0 };
	template<TrackedComponent T>
	dU32 GetTrackedComponentIndex() { static const dU32 index = g_trackedComponentCount.fetch_add(1, std::memory_order_relaxed); return index; }

	template<typename TView>
	class ComponentView
	{
	public:
		explicit ComponentView(TView view) : m_view{ view } {}

		template<typename TFunction>
		void each(TFunction&& function) const { m_view.each(std::forward<TFunction>(function)); }

	private:
		TView m_view;
	};

	class GameScene
	{
	public:
		GameScene() = default;

		GameScene(const GameScene&) = delete;
		GameScene& operator=(const GameScene&) = delete;
		GameScene(GameScene&&) = delete;
		GameScene& operator=(GameScene&&) = delete;

		EntityID CreateEntity(const char* pName);
		EntityID CreateEntity(const char* pName, const Transform& transform);
		EntityID SpawnMesh(const char* pName, const Transform& transform, MeshHandle mesh, MaterialSetHandle materials);
		void DestroyEntity(EntityID entity);
		void DestroyAllEntities();

		template<Component T>
		void Add(EntityID entity, T component)
		{
			m_registry.emplace<T>(entity, std::move(component));
			LogChange<T>(entity);
		}

		template<Component T>
		void Set(EntityID entity, T component)
		{
			m_registry.replace<T>(entity, std::move(component));
			LogChange<T>(entity);
		}

		template<Component T>
		void Remove(EntityID entity)
		{
			m_registry.remove<T>(entity);
			LogChange<T>(entity);
		}

		template<Component T>
		[[nodiscard]] bool Has(EntityID entity) const { return m_registry.all_of<T>(entity); }

		template<Component T>
		[[nodiscard]] ComponentAccess<T>& Get(EntityID entity) { return m_registry.get<T>(entity); }

		template<Component T>
		[[nodiscard]] const T& Get(EntityID entity) const { return m_registry.get<T>(entity); }

		template<Component T>
		[[nodiscard]] ComponentAccess<T>* TryGet(EntityID entity) { return m_registry.try_get<T>(entity); }

		template<Component T>
		[[nodiscard]] const T* TryGet(EntityID entity) const { return m_registry.try_get<T>(entity); }

		template<Component T, typename TFunction>
		bool TryModify(EntityID entity, TFunction&& function)
		{
			static_assert(std::is_same_v<std::invoke_result_t<TFunction, T&>, bool>);
			T* pComponent = m_registry.try_get<T>(entity);
			if (!pComponent)
				return false;

#ifdef _DEBUG
			alignas(T) dU8 previous[sizeof(T)];
			if constexpr (kIsTracked<T> && std::is_trivially_copyable_v<T>)
				memcpy(previous, pComponent, sizeof(T));
#endif
			const bool isChanged = function(*pComponent);
#ifdef _DEBUG
			if constexpr (kIsTracked<T> && std::is_trivially_copyable_v<T>)
				Assert(isChanged || memcmp(previous, pComponent, sizeof(T)) == 0);
#endif
			if (isChanged)
				LogChange<T>(entity);
			return isChanged;
		}

		template<typename... T>
		[[nodiscard]] auto View()
		{
			auto view = m_registry.view<ComponentAccess<T>...>();
			return ComponentView<decltype(view)>{ view };
		}

		template<typename... T>
		[[nodiscard]] auto View() const
		{
			auto view = m_registry.view<const T...>();
			return ComponentView<decltype(view)>{ view };
		}

		template<TrackedComponent T>
		[[nodiscard]] dSpan<const EntityID> GetChanges() const
		{
			const dU32 index = GetTrackedComponentIndex<T>();
			return index < (dU32)m_changes.size() ? dSpan<const EntityID>(m_changes[index]) : dSpan<const EntityID>(nullptr, 0);
		}

		[[nodiscard]] dSpan<const EntityID> GetDestroyedEntities() const { return m_destroyedEntities; }
		[[nodiscard]] bool HasChanges() const;
		void ClearChanges();

	private:
		template<Component T>
		void LogChange(EntityID entity)
		{
			if constexpr (kIsTracked<T>)
			{
				const dU32 index = GetTrackedComponentIndex<T>();
				if (index >= (dU32)m_changes.size())
					m_changes.resize(index + 1);
				m_changes[index].push_back(entity);
			}
		}

	private:
		entt::basic_registry<EntityID> m_registry;
		dVector<dVector<EntityID>>     m_changes;
		dVector<EntityID>              m_destroyedEntities;
	};
}
