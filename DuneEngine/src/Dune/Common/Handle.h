#pragma once

namespace Dune
{
	template<typename H> 
	class Handle
	{
	public:
		Handle() : m_id(ID::invalidID) {}
		bool IsValid() const { return ID::IsValid(m_id); }
		void Invalidate() { m_id = ID::invalidID; }

	private:
		Handle(ID::IDType id) : m_id(id) {}

		ID::IDType m_id;

		template<typename T, typename H> friend class Pool;
	};
}