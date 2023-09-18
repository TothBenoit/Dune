#pragma once

#include "Dune/Graphics/TextureDesc.h"
#include "Dune/Graphics/DescriptorHeap.h"

namespace Dune
{
	template <typename T, typename H>
	class Pool;

	class Texture
	{
	public:
		[[nodiscard]] ID3D12Resource* GetResource() { return m_texture; }
		[[nodiscard]] DescriptorHandle GetSRV() { return m_SRV; }
		[[nodiscard]] DescriptorHandle GetRTV() { Assert(m_usage == ETextureUsage::RTV); return m_RTV; }
		[[nodiscard]] DescriptorHandle GetDSV() { Assert(m_usage == ETextureUsage::DSV); return m_DSV; }
	private:
		Texture(const TextureDesc& desc);
		~Texture();
		DISABLE_COPY_AND_MOVE(Texture);

	private:
		friend Pool<Texture, Texture>;

		DescriptorHandle		m_SRV;
		union 
		{
			DescriptorHandle	m_RTV;
			DescriptorHandle	m_DSV;
		};

		ID3D12Resource*		m_texture;
		const ETextureUsage	m_usage;
	};
}
