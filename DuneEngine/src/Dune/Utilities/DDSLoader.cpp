#include "pch.h"
#include <fstream>
#include "Dune/Utilities/DDSLoader.h"
#include "Dune/Core/Graphics.h"

namespace Dune::Graphics 
{
	constexpr dU32 MakeFourCC(char ch0, char ch1, char ch2, char ch3) 
	{
		return (dU32(dU8(ch0)) | (dU32(dU8(ch1)) << 8) | (dU32(dU8(ch2)) << 16) | (dU32(dU8(ch3)) << 24));
	}

	DDSResult DDSTexture::Load(const char* filePath, DDSTexture& outDDSTexture)
	{
		Assert(outDDSTexture.m_dds.empty());

		std::ifstream file{ filePath, std::ios_base::binary };
		
		if (!file.is_open())
			return DDSResult::EFailedOpen;
		
		dVector<dU8>& dds = outDDSTexture.m_dds;
		
		file.seekg(0, std::ios_base::beg);
		dSizeT begPos = file.tellg();
		file.seekg(0, std::ios_base::end);
		dSizeT endPos = file.tellg();
		file.seekg(0, std::ios_base::beg);

		dSizeT fileSize = endPos - begPos;
		dds = dVector<dU8>(fileSize);

		file.read(reinterpret_cast<char*>(dds.data()), fileSize);

		if (file.bad())
		{
			dds.clear();
			dds.shrink_to_fit();
			return DDSResult::EFailedRead;
		}

		constexpr char magicWord[4] = { 'D', 'D', 'S', ' ' };

		for (int i = 0; i < 4; i++) 
		{
			if (dds[i] != magicWord[i])
			{
				return DDSResult::EFailedMagicWord;
			}
		}

		if ((sizeof(dU32) + sizeof(DDSHeader)) >= dds.size())
			return DDSResult::EFailedSize;
		
		outDDSTexture.m_pHeader = reinterpret_cast<DDSHeader*>(dds.data() + sizeof(dU32));
		DDSHeader& header = *outDDSTexture.m_pHeader;

		bool dxt10Header = false;
		if ( (header.pixelFormat.flags & dU32(DDSPixelFormatFlagBits::FourCC))  && (MakeFourCC('D', 'X', '1', '0') == header.pixelFormat.fourCC ))
		{
			if ((sizeof(dU32) + sizeof(DDSHeader) + sizeof(DDSHeaderDXT10)) >= dds.size())
				return DDSResult::EFailedSize;
			
			outDDSTexture.m_pHeaderDXT10 = reinterpret_cast<DDSHeaderDXT10*>(dds.data() + sizeof(dU32) + sizeof(DDSHeader));
			dxt10Header = true;
		}

		dU64 offset = sizeof(dU32) + sizeof(DDSHeader) + (dxt10Header ? sizeof(DDSHeaderDXT10) : 0);
		outDDSTexture.m_pData = reinterpret_cast<void*>(dds.data() + offset);

		file.close();

		return DDSResult::ESucceed;
	}

	DDSResult DDSTexture::Load(const char* filePath)
	{
		return Load(filePath, *this);
	}

	Handle<Texture> DDSTexture::CreateTextureFromFile(Device* pDevice, const char* filePath)
	{
		Graphics::DDSTexture ddsTexture;
		Graphics::DDSResult result = ddsTexture.Load(filePath);
		Assert(result == Graphics::DDSResult::ESucceed && ddsTexture.GetHeaderDXT10());
		void* pData = ddsTexture.GetData();
		const Graphics::DDSHeader* pHeader = ddsTexture.GetHeader();
		const Graphics::DDSHeaderDXT10* pHeaderDXT10 = ddsTexture.GetHeaderDXT10();
		Handle<Graphics::Texture> texture = Graphics::CreateTexture(pDevice, { .usage = Graphics::ETextureUsage::SRV, .dimensions = { pHeader->height, pHeader->width, pHeader->depth + 1 }, .mipLevels = pHeader->mipMapCount, .format = pHeaderDXT10->format, .clearValue = {0.f, 0.f, 0.f, 0.f}, .pData = pData });
		ddsTexture.Destroy();
		return texture;
	}

	void DDSTexture::Destroy()
	{
		m_dds.clear();
		m_dds.shrink_to_fit();
		m_pHeader = nullptr;
		m_pData = nullptr;
	}
}

