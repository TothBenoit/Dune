#pragma once

namespace Dune
{
	class EntityManager
	{
	public:
		EntityManager() = default;
		~EntityManager() = default;
		DISABLE_COPY_AND_MOVE(EntityManager);

		[[nodiscard]] EntityID CreateEntity();
		void RemoveEntity(EntityID entity);
		bool IsValid(EntityID entity) const;

	private:
		dQueue<EntityID> m_freeEntityIDs;
		dVector<ID::GenerationType> m_generationIDs;
	};
}