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
		[[nodiscard]] ID3D12Resource*			GetResource() { return m_texture; }
		[[nodiscard]] const DescriptorHandle&	GetSRV() { return m_SRV; }
		[[nodiscard]] const DescriptorHandle&	GetRTV(dU32 index = 0) { Assert(m_usage == ETextureUsage::RTV); return m_RTV[index]; }
		[[nodiscard]] const DescriptorHandle&	GetDSV(dU32 index = 0) { Assert(m_usage == ETextureUsage::DSV); return m_DSV[index]; }
	private:
		Texture(const TextureDesc& desc);
		~Texture();
		DISABLE_COPY_AND_MOVE(Texture);

	private:
		friend Pool<Texture, Texture>;

		DescriptorHandle		m_SRV;
		union 
		{
			DescriptorHandle*	m_RTV;
			DescriptorHandle*	m_DSV;
		};

		ID3D12Resource*			m_texture;
		const ETextureUsage		m_usage;
		const dU32				m_dimensions[3];
	};
}
