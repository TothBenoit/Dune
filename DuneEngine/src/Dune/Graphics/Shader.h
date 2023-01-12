#pragma once

namespace Dune
{
	class Shader
	{
	public:
		Shader(const dString& path);
		DISABLE_COPY_AND_MOVE(Shader);

	private:
		dString m_path;
	};
}