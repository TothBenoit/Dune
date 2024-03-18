#pragma once

namespace Dune
{
	class Input;
}

namespace Dune::Graphics
{
	struct WindowDesc;

	class Window
	{
	public:
		[[nodiscard]] dU32	GetWidth() const { return m_width; }
		[[nodiscard]] dU32	GetHeight() const { return m_height; }
		[[nodiscard]] const Input* GetInput() const { return m_pInput; }
	protected:
		Input* m_pInput{ nullptr };
		dU32 m_width;
		dU32 m_height;
	};
}