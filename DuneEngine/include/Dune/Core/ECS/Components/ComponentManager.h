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
			Assert(!Contains(entity));

			// If those containers aren't the same size, something went wrong
			Assert(m_lookupEntities.size() == m_components.size());

			const ID::IDType index = ID::GetIndex(entity);

			// Add the entity to the look up table
			m_pLookupComponents[index] = (ID::IDType) m_components.size();

			// Create new component
			m_components.emplace_back();

			// Add to the list of entity
			m_lookupEntities.push_back(index);

			return m_components.back();
		}

		static void Remove(EntityID entity)
		{
			Assert(Contains(entity));

			const ID::IDType index = ID::GetIndex(entity);
			const ID::IDType componentIndex = m_pLookupComponents[index];
			m_pLookupComponents[index] = ID::invalidID;

			if (componentIndex < m_components.size() - 1)
			{
				// Swap out the dead element with the last one:
				m_components[componentIndex] = std::move(m_components.back());

				const ID::IDType swappedComponentEntityIndex = m_lookupEntities.back();
				m_lookupEntities[componentIndex] = swappedComponentEntityIndex;

				// Update the lookup table:
				m_pLookupComponents[swappedComponentEntityIndex] = componentIndex;
			}

			// Shrink the container:
			m_components.pop_back();
			m_lookupEntities.pop_back();
		}

		static Component* GetComponent(EntityID entity)
		{
			const ID::IDType componentIndex = m_pLookupComponents[ID::GetIndex(entity)];
			if (ID::IsValid(componentIndex))
			{
				return &m_components[componentIndex];
			}
			return nullptr;
		}

		static Component& GetComponentUnsafe(EntityID entity)
		{
			return m_components[m_pLookupComponents[ID::GetIndex(entity)]];
		}

		static bool Contains(EntityID entity)
		{
			return ID::IsValid(m_pLookupComponents[ID::GetIndex(entity)]);
		}

		static void Init(dU32 reservedCount)
		{
			Assert(!m_isInitialized);

			m_components.reserve(reservedCount);
			m_lookupEntities.reserve(reservedCount);
			m_pLookupComponents = new ID::IDType[MAX_ENTITIES];
			memset(m_pLookupComponents, ID::invalidID, MAX_ENTITIES * sizeof(ID::IDType));

			m_isInitialized = true;
		}

		static void Shutdown()
		{
			m_components.clear();
			m_lookupEntities.clear();
			delete[] m_pLookupComponents;
			m_isInitialized = false;
		}

	private:
		friend class EngineCore;
		inline static dVector<Component>	m_components;
		inline static dVector<EntityID>		m_lookupEntities;
		inline static ID::IDType*			m_pLookupComponents;
		inline static bool m_isInitialized{false};
	};
}