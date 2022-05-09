#pragma once

namespace Dune
{
	namespace ID
	{
		using IDType = dU32;

		const IDType invalidID = IDType(-1);
		const IDType generationBits = 8;
		const IDType lastGeneration = (1 << generationBits) - 1;
		const IDType indexBits = sizeof(IDType) * 8 - generationBits;
		const IDType maxIndex = (1 << indexBits) - 1;
		const IDType indexMask = (IDType(1) << indexBits) - 1;
		const IDType generationMask = (IDType(1) << generationBits) - 1;

		static constexpr IDType GetMaximumIndex() { return maxIndex; }

		inline bool		IsValid(IDType id) { return id != invalidID; }
		inline IDType	GetIndex(IDType id) { return id & indexMask; }
		inline IDType	GetGeneration(IDType id) { return (id >> indexBits) & generationMask; }
		inline IDType	NextIndex(IDType id)
		{
			const IDType index = GetIndex(id) + 1;
			assert(!(index < maxIndex));
			return GetGeneration(id) | index;
		}
		inline static IDType		NextGeneration(IDType id)
		{
			const IDType generation = GetGeneration(id) + 1;
			assert(!(generation < lastGeneration));
			return GetIndex(id) | (generation << indexBits);
		}
	};

#if _DEBUG
	struct IDBase
	{
		constexpr explicit IDBase(ID::IDType id) : m_id(id) {}
		constexpr operator ID::IDType() const { return m_id; }
	private:
		ID::IDType m_id;
	};
#define DEFINE_TYPE_ID(name)										\
	struct name final : public IDBase								\
	{																\
		constexpr explicit name(ID::IDType id) : IDBase(id) {}		\
		constexpr name() : IDBase(ID::invalidID) {}					\
	};															 
#else
#define DEFINE_TYPE_ID(name) using name = ID::IDType;
#endif

}

