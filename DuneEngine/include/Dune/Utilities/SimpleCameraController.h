#pragma once

#include "Dune/Scene/Camera.h"

namespace Dune
{
	class Input;

	class SimpleCameraController
	{
	public :
		SimpleCameraController() = default;
		SimpleCameraController( const Camera& camera );

		void Update(float deltaTime, const Input& input);
		Camera& GetCamera() { return m_camera; }
		const Camera& GetCamera() const { return m_camera; }
		void SetFOV(float fov) { m_camera.fov = fov; };
		void SetAspectRatio(float aspectRatio) { m_camera.aspectRatio = aspectRatio; }
		void SetNear(float near) { m_camera.near = near; }
		void SetFar(float far) { m_camera.far = far; }
	private:
		
		void UpdateFirstPersonControls(float deltaTime, const Input& input);
		void UpdateEditorControls(float deltaTime, const Input& input);

	private:
		Camera m_camera;
		dVec2 m_rotation;
		float m_zoomSpeed = 1.15f;
		float m_minZoom = 0.01f;
		float m_maxZoom = 2e10f;
	};
}