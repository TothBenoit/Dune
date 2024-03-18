#pragma once

#include "Dune/Core/Graphics/Camera.h"

namespace Dune
{
	class Input;

	class SimpleCameraController
	{
	public :
		SimpleCameraController( const Graphics::Camera& camera );

		void Update(float deltaTime, const Input* pInput);
		const Graphics::Camera& GetCamera() const { return m_camera; }
		void SetFOV(float fov) { m_camera.fov = fov; };
		void SetAspectRatio(float aspectRatio) { m_camera.aspectRatio = aspectRatio; }
		void SetNear(float near) { m_camera.near = near; }
		void SetFar(float far) { m_camera.far = far; }
	private:
		Graphics::Camera m_camera;
		dVec3 m_rotation;
	};
}