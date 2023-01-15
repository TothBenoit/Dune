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

		bool IsValid() const { return ID::IsValid(m_id); }

	private:
		Handle(ID::IDType id) : m_id(id) {}

		ID::IDType m_id;

		template<typename T, typename H> friend class Pool;
	};
}