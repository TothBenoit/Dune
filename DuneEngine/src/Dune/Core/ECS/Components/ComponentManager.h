#pragma once

//Inspired by WickedEngine

namespace Dune
{
	template<typename Component>
	class ComponentManager
	{
	private:
		static Component& Create(EntityID entity)
		{
			Assert(entity != ID::invalidID);

			// One component of this type per entity
			Assert(m_lookup.find(entity) == m_lookup.end());

			// If those container aren't the same size, something went wrong
			Assert(m_entities.size() == m_components.size());
			Assert(m_lookup.size() == m_components.size());

			// Add the entity to the look up table
			m_lookup[entity] = m_components.size();

			// Create new component
			m_components.emplace_back();

			// Add to the list of entity
			m_entities.push_back(entity);

			return m_components.back();
		}

		static void Remove(EntityID entity)
		{
			auto it = m_lookup.find(entity);
			if (it != m_lookup.end())
			{
				const size_t index = it->second;
				const EntityID entity = m_entities[index];

				if (index < m_components.size() - 1)
				{
					// Swap out the dead element with the last one:
					m_components[index] = std::move(m_components.back());
					m_entities[index] = m_entities.back();

					// Update the lookup table:
					m_lookup[m_entities[index]] = index;
				}

				// Shrink the container:
				m_components.pop_back();
				m_entities.pop_back();
				m_lookup.erase(entity);
			}
		}

		static Component* GetComponent(EntityID entity)
		{
			auto it = m_lookup.find(entity);
			if (it != m_lookup.end())
			{
				return &m_components[it->second];
			}
			return nullptr;
		}

		static bool Contains(EntityID entity)
		{
			return m_lookup.find(entity) != m_lookup.end();
		}

		static void Init()
		{
			Assert(!m_isInitialized);

			constexpr size_t reservedCount = RESERVED_ENTITIES;
			m_components.reserve(reservedCount);
			m_entities.reserve(reservedCount);
			m_lookup.reserve(reservedCount);
			m_isInitialized = true;
		}

		static void Shutdown()
		{
			m_components.clear();
			m_entities.clear();
			m_lookup.clear();
			m_isInitialized = false;
		}

	private:
		friend class EngineCore;
		inline static dVector<Component> m_components;
		inline static dVector<EntityID> m_entities;
		inline static dHashMap<EntityID, size_t> m_lookup;
		inline static bool m_isInitialized{false};
	};
}