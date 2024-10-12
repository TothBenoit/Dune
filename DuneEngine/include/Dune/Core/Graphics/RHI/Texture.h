#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"
#include "Dune/Core/Graphics/RHI/DescriptorHeap.h"
#include <mutex>

namespace Dune::Graphics 
{
	struct Device;
	class Texture;
	class CommandList;

	enum class ETextureUsage
	{
		RTV,
		DSV,
		SRV,
		UAV,
	};

	struct TextureDesc
	{
		const wchar_t* debugName{ L"Texture" };

		ETextureUsage usage{ ETextureUsage::RTV };
		dU32 dimensions[3]{ 1, 1, 1 };
		dU32 mipLevels{ 1 };
		EFormat	format{ EFormat::R8G8B8A8_UNORM };
		float clearValue[4]{ 0.f, 0.f, 0.f, 0.f };
		void* pData{ nullptr };
	};

	class Texture : public Resource
	{
	public:
		Texture(Device* pDeviceInterface, const TextureDesc& desc);
		~Texture();
		DISABLE_COPY_AND_MOVE(Texture);

		[[nodiscard]] const Descriptor& GetSRV() { return m_SRV; }
		[[nodiscard]] const Descriptor& GetRTV(dU32 index = 0) { Assert(m_usage == ETextureUsage::RTV); return m_RTV[index]; }
		[[nodiscard]] const Descriptor& GetDSV(dU32 index = 0) { Assert(m_usage == ETextureUsage::DSV); return m_DSV[index]; }
		[[nodiscard]] const dU32* GetDimensions() { return m_dimensions; }
		[[nodiscard]] const float* GetClearValue() { return m_clearValue; }

		void Transition(CommandList* pCommand, dU32 targetState);
	private:		
		Descriptor		m_SRV;
		union
		{
			Descriptor* m_RTV;
			Descriptor* m_DSV;
		};

		const ETextureUsage		m_usage;
		const dU32				m_dimensions[3];
		float					m_clearValue[4];
		dU32					m_state{0};
		Device*					m_pDeviceInterface{ nullptr };
		std::mutex				m_transitionMutex{};
	};

	[[nodiscard]] Handle<Texture>	CreateTexture(Device* pDevice, const TextureDesc& desc);
	void							ReleaseTexture(Device* pDevice, Handle<Texture> handle);
}
