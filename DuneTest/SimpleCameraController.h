#pragma once

#include "Dune/Game/Component/Transform.h"

namespace Dune
{
	class Input;

	class SimpleCameraController
	{
	public :
		void Update(float deltaTime, const Input& input);
		[[nodiscard]] Transform GetTransform() const;

	private:
		void UpdateFirstPersonControls(float deltaTime, const Input& input);
		void UpdateEditorControls(float deltaTime, const Input& input);

	private:
		dVec3 m_position{ 0.0f, 0.0f, 0.0f };
		dVec2 m_rotation{ 0.0f, 0.0f };
		float m_zoomSpeed = 1.15f;
		float m_minZoom = 0.01f;
		float m_maxZoom = 2e10f;
	};
}
