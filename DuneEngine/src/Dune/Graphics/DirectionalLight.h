#pragma once

namespace Dune
{
	struct DirectionalLight
	{
		DirectionalLight(dVec3 color, float intensity, dVec3 dir)
			: m_color(color), m_intensity(intensity), m_dir(dir)
		{}

		dVec3 m_color;
		float m_intensity;
		dVec3 m_dir;
		float m_padding1 = 0.f;
	};
}