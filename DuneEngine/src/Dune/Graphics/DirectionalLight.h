#pragma once

namespace Dune
{
	struct DirectionalLight
	{
		DirectionalLight(dVec3 color, float intensity, dVec3 dir, dMatrix viewProj)
			: m_color(color), m_intensity(intensity), m_dir(dir), m_viewProj(viewProj)
		{}

		dVec3 m_color;
		float m_intensity;
		dVec3 m_dir;
		float m_padding1 = 0.f;
		dMatrix m_viewProj;
	};
}