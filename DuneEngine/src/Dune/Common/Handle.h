#pragma once

namespace Dune
{
	template<typename H> 
	class Handle
	{
	public:
		Handle() : m_id(ID::invalidID) {}
		~Handle() = default;

		Handle(const Handle& other) 
		{ 
			m_id = other.m_id; 
		}

		Handle& operator=(const Handle& other) 
		{ 
			m_id = other.m_id; 
			return *this; 
		}

		Handle(Handle&& other) 
		{
			m_id = other.m_id; 
			other.m_id = ID::invalidID;
		}

		Handle& operator=(Handle&& other) 
		{ 
			m_id = other.m_id;
			other.m_id = ID::invalidID;
			return *this; 
		}

		[[nodiscard]] ID::IDType GetID() const { return m_id; }
		[[nodiscard]] bool IsValid() const { return ID::IsValid(m_id); }

		Handle(ID::IDType id) : m_id(id) {} // Back to private ASAP. I need a hash function asap
	private:

		ID::IDType m_id;

		template<typename T, typename H> friend class Pool;
	};
}