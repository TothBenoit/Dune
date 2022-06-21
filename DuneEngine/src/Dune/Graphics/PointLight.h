#pragma once

namespace Dune
{
	struct PointLight
	{
		PointLight(dVec3 color, float intensity, float radius, dVec3 pos)
			: m_color(color), m_intensity(intensity), m_radius(radius), m_pos(pos)
		{}

		dVec3 m_color;
		float m_intensity;
		float m_radius;
		dVec3 m_pos;
	};
}