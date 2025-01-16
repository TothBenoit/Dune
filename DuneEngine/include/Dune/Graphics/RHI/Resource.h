#pragma once

namespace Dune::Graphics
{
	class Resource
	{
	public:
		[[nodiscard]] void* Get() { return m_pResource; }
		[[nodiscard]] const void* Get() const { return m_pResource; }

	protected:
		void* m_pResource;
	};
}