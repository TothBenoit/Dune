#pragma once

namespace Dune
{
	struct ID
	{
		using TypeID = dU32;

		constexpr explicit ID(TypeID id) : m_id(id){}
		constexpr operator TypeID() const { return m_id; }

		inline static bool		IsValid(ID id) { return id.m_id != invalidID; }
		inline static TypeID	GetIndex(ID id) { return id.m_id & indexMask; }
		inline static TypeID	GetGeneration(ID id) { return (id.m_id >> indexBits) & generationMask; }
		inline static ID		NextGeneration(ID id)
		{
			const TypeID generation = id.GetGeneration() + 1;
			assert(!(generation < lastGeneration));
			return ID(id.GetIndex() | (generation << indexBits));
		}

		inline bool				IsValid() const { return m_id != invalidID; }
		inline TypeID			GetIndex() const { return m_id; }
		inline TypeID			GetGeneration() const { return (m_id >> indexBits) & generationMask; }

	public:
	
		static const TypeID invalidID = TypeID(-1);
	
	protected:
		static const TypeID generationBits = 8;
		static const TypeID lastGeneration = (1 << generationBits) - 1;
		static const TypeID indexBits = sizeof(TypeID) * 8 - generationBits;
		static const TypeID indexMask = (TypeID(1) << indexBits) - 1;
		static const TypeID generationMask = (TypeID(1) << generationBits) - 1;
		TypeID m_id;
	};

#if _DEBUG
#define DEFINE_TYPE_ID(name)									 \
	struct name final : public ID								 \
	{															 \
		constexpr explicit name(ID::TypeID id) : ID(id) {}		 \
		constexpr name() : ID(ID::invalidID) {}					 \
	};															 
#else
#define DEFINE_TYPEID(name) using name = ID;
#endif

}

