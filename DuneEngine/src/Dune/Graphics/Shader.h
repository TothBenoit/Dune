#pragma once

namespace Dune
{
	class Shader
	{
	public:
		Shader(const dString& path);
	private:
		dString m_path;
	};
}