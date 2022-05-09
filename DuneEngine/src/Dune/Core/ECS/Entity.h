#pragma once

namespace Dune
{
	DEFINE_TYPE_ID(EntityID);

	class Entity
	{
	public:
		constexpr explicit Entity(EntityID id) : m_id(id) {}
		constexpr explicit Entity() : m_id(ID::invalidID) {}

		inline EntityID GetID() const { return m_id; };

	private:
		EntityID m_id;
	};
}