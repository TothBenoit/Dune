#include "pch.h"
#include "Input.h"

namespace Dune
{
	bool Input::m_keyState[256];
	bool Input::m_keyDown[256];
	bool Input::m_keyUp[256];

	void Input::SetKeyDown(KeyCode key)
	{
		m_keyState[(int)key] = true;
		m_keyDown[(int)key] = true;
	}
	void Input::SetKeyUp(KeyCode key)
	{
		m_keyState[(int)key] = false;
		m_keyUp[(int)key] = true;
	}
	void Input::EndFrame()
	{
		memset(m_keyDown, 0, sizeof(m_keyDown));
		memset(m_keyUp, 0, sizeof(m_keyUp));
	}
}