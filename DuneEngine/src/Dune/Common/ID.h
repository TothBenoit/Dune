#pragma once

//Inspired by Arash Khatami

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

		using GenerationType = std::conditional_t<generationBits <= 8, dU8, std::conditional_t<generationBits <= 16, dU16, dU32>>;

		static constexpr IDType GetMaximumIndex() { return maxIndex; }

		inline bool				IsValid(IDType id) { return id != invalidID; }
		inline IDType			GetIndex(IDType id) { return id & indexMask; }
		inline GenerationType	GetGeneration(IDType id) { return (id >> indexBits) & generationMask; }
		inline IDType			NextIndex(IDType id)
		{
			const IDType index = GetIndex(id) + 1;
			Assert(index < maxIndex);
			return GetGeneration(id) | index;
		}
		inline static IDType		NextGeneration(IDType id)
		{
			const IDType generation = GetGeneration(id) + 1;
			Assert(generation < lastGeneration);
			return GetIndex(id) | (generation << indexBits);
		}
	};

	using EntityID = ID::IDType;
}

