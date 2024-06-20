#pragma once

namespace Dune
{
	template<typename H> 
	class Handle
	{
	public:
		explicit Handle(ID::IDType id) : m_id(id) {}
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

		bool operator==(const Handle& other) const
		{
			return other.m_id == m_id;
		}

		[[nodiscard]] ID::IDType GetID() const { return m_id; }
		[[nodiscard]] bool IsValid() const { return ID::IsValid(m_id); }
		void Reset() { m_id = ID::invalidID; }

	private:

		ID::IDType m_id;

		template<typename T, typename H, bool ThreadSafe> friend class Pool;
	};
}

template <typename H>
struct std::hash<Dune::Handle<H>>
{
	size_t operator()(const Dune::Handle<H>& handle) const noexcept { return std::hash<unsigned int>()(handle.GetID()); }
};
