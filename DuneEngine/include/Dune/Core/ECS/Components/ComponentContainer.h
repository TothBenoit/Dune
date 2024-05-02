#pragma once

//Inspired by WickedEngine

namespace Dune
{
	template<typename Component>
	class ComponentContainer
	{
	public:
		void Initialize(dU32 reservedCount)
		{
			Assert(!m_isInitialized);
			m_capacity = m_maxEntity = reservedCount;

			m_pComponents = new Component[m_capacity];
			m_pLookupEntities = new EntityID[m_capacity];
			m_pLookupComponents = new ID::IDType[m_maxEntity];
		#if _DEBUG
			memset(m_pLookupComponents, ID::invalidID, m_maxEntity * sizeof(ID::IDType));
			m_isInitialized = true;
		#endif
		}

		void Destroy()
		{
			delete[] m_pComponents;
			delete[] m_pLookupEntities;
			delete[] m_pLookupComponents;
			m_count = 0;
			m_capacity = 0;
		#if _DEBUG
			m_isInitialized = false;
		#endif
		}

		void Resize(dU32 size)
		{
			Assert(m_capacity != 0);

			Component* pComponents = new Component[size];
			EntityID* pLookupEntities = new EntityID[size];

			m_count = (size < m_count) ? size : m_count;

			memcpy(pComponents, m_pComponents, m_count * sizeof(Component));
			memcpy(pLookupEntities, m_pLookupEntities, m_count * sizeof(dU32));

			delete[] m_pComponents;
			delete[] m_pLookupEntities;

			m_pComponents = pComponents;
			m_pLookupEntities = pLookupEntities;

			m_capacity = size;
		}

		void Add(EntityID entity, const Component& component)
		{
			Assert(entity != ID::invalidID);
			Assert(!Contains(entity));

			const ID::IDType index = ID::GetIndex(entity);

			if (index >= m_maxEntity)
			{
				// TODO : Allocator + reserve / resize maxEntity
				ID::IDType maxEntity = index + 1;
				ID::IDType* pLookupComponents = new ID::IDType[maxEntity];
			#if _DEBUG
				memset(pLookupComponents, ID::invalidID, maxEntity * sizeof(ID::IDType));
			#endif
				memcpy(pLookupComponents, m_pLookupComponents, m_maxEntity * sizeof(ID::IDType));

				delete[] m_pLookupComponents;
				m_pLookupComponents = pLookupComponents;
				m_maxEntity = maxEntity;
			}

			if (m_count + 1 == m_capacity)
			{
				// TODO : Allocator
				Resize(m_capacity + 1);
			}

			m_pLookupComponents[index] = (ID::IDType) m_count;

			m_pComponents[m_count] = component;
			m_pLookupEntities[m_count] = index;

			m_count++;
		}

		void Remove(EntityID entity)
		{
			Assert(Contains(entity));

			const ID::IDType index = ID::GetIndex(entity);
			const ID::IDType componentIndex = m_pLookupComponents[index];
		#if _DEBUG
			m_pLookupComponents[index] = ID::invalidID;
		#endif

			--m_count;			
			m_pComponents[componentIndex] = std::move(m_pComponents[m_count]);
			const ID::IDType swappedComponentEntityIndex = m_pLookupEntities[m_count];
			m_pLookupEntities[componentIndex] = swappedComponentEntityIndex;
			m_pLookupComponents[swappedComponentEntityIndex] = componentIndex;			
		}

		Component* GetComponent(EntityID entity)
		{
			const ID::IDType componentIndex = m_pLookupComponents[ID::GetIndex(entity)];
			if (ID::IsValid(componentIndex))
			{
				return &m_pComponents[componentIndex];
			}
			return nullptr;
		}

		Component& GetComponentUnsafe(EntityID entity)
		{
			return m_pComponents[m_pLookupComponents[ID::GetIndex(entity)]];
		}

		bool Contains(EntityID entity)
		{
			const ID::IDType index = ID::GetIndex(entity);
			return (index >= m_maxEntity) ? false : ID::IsValid(m_pLookupComponents[index]);
		}

	private:
		Component*	m_pComponents;
		EntityID*	m_pLookupEntities;
		ID::IDType*	m_pLookupComponents;
		dU32		m_count;
		dU32		m_capacity;
		ID::IDType	m_maxEntity;
	#if _DEBUG
		bool		m_isInitialized{ false };
	#endif
	};
}