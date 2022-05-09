#pragma once

namespace Dune
{
	class EntityManager
	{
	public:
		EntityManager();
		EntityManager(const EntityManager&) = delete;
		EntityManager& operator=(const EntityManager&) = delete;
		EntityID CreateEntity();
		void RemoveEntity(EntityID entity);
		bool IsAlive(EntityID entity) const ;

	private:
		dQueue<EntityID> m_freeEntityIDs;
		dVector<ID::GenerationType> m_generationIDs;
	};
}