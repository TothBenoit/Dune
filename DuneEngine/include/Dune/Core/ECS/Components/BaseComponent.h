#pragma once

namespace Dune
{

	template<typename T>
	struct BaseComponent
	{
		BaseComponent() = delete;
		~BaseComponent() = delete;

		static const dU32 id = Family::Type<T>();
	};
}