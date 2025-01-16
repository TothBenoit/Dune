#include "pch.h"
#include "Dune/System/Input.h"

namespace Dune
{
	void Input::SetKeyDown(KeyCode key)
	{
		m_keyDown[(int)key] = true;
		m_keyState[(int)key] = true;
	}
	void Input::SetKeyUp(KeyCode key)
	{
		m_keyUp[(int)key] = true;
		m_keyState[(int)key] = false;
	}
	void Input::SetMouseButtonDown(int index)
	{
		m_mouseDown[index] = true;
		m_mouseState[index] = true;
	}
	void Input::SetMouseButtonUp(int index)
	{
		m_mouseUp[index] = true;
		m_mouseState[index] = false;
	}
	void Input::SetMousePosX(float posX)
	{
		m_mouseDeltaX = posX - m_mousePosX;
		m_mousePosX = posX;
	}
	void Input::SetMousePosY(float posY)
	{
		m_mouseDeltaY = posY - m_mousePosY;
		m_mousePosY = posY;
	}
	void Input::SetMouseWheelDelta(float delta)
	{
		m_mouseWheelDelta = delta;
	}
	void Input::BeginFrame()
	{
		memset(m_keyDown, 0, sizeof(m_keyDown));
		memset(m_keyUp, 0, sizeof(m_keyUp));
		memset(m_mouseDown, 0, sizeof(m_mouseDown));
		memset(m_mouseUp, 0, sizeof(m_mouseUp));
		m_mouseDeltaX = 0;
		m_mouseDeltaY = 0;
		m_mouseWheelDelta = 0;
	}
}