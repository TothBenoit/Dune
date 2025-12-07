#include "pch.h"
#include "Dune/Utilities/DDSLoader.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Core/File.h"

namespace Dune::Graphics 
{
	constexpr dU32 MakeFourCC(char ch0, char ch1, char ch2, char ch3) 
	{
		return (dU32(dU8(ch0)) | (dU32(dU8(ch1)) << 8) | (dU32(dU8(ch2)) << 16) | (dU32(dU8(ch3)) << 24));
	}

	constexpr bool HasBitMask( DDSPixelFormat pixelFormat, dU32 r, dU32 g, dU32 b, dU32 a)
	{
		return pixelFormat.RBitMask == r && pixelFormat.GBitMask == g && pixelFormat.BBitMask == b && pixelFormat.ABitMask == a;
	}

	DDSResult DDSTexture::Load(const char* filePath, DDSTexture& outDDSTexture)
	{
		Assert(!outDDSTexture.m_pFileBuffer);

		File file;
		if (!File::Open(file, filePath, File::EAccessMode::Read, File::EShareMode::None))
			return DDSResult::EFailedOpen;
		
		dU64 byteSize = file.GetByteSize();
		dU8* pFileBuffer = new dU8[byteSize];

		if ( !file.Read(reinterpret_cast<char*>(pFileBuffer), (dU32)byteSize) )
		{
			delete[] pFileBuffer;
			file.Close();
			return DDSResult::EFailedRead;
		}

		constexpr char magicWord[4] = { 'D', 'D', 'S', ' ' };

		for (int i = 0; i < 4; i++) 
		{
			if (pFileBuffer[i] != magicWord[i])
			{
				return DDSResult::EFailedMagicWord;
			}
		}

		if ((sizeof(dU32) + sizeof(DDSHeader)) >= byteSize) 
		{
			delete[] pFileBuffer;
			file.Close();
			return DDSResult::EFailedSize;
		}
		
		outDDSTexture.m_pHeader = reinterpret_cast<DDSHeader*>(pFileBuffer + sizeof(dU32));
		DDSHeader& header = *outDDSTexture.m_pHeader;

		bool dxt10Header = false;
		if ( (header.pixelFormat.flags & dU32(DDSPixelFormatFlagBits::FourCC))  && (MakeFourCC('D', 'X', '1', '0') == header.pixelFormat.fourCC ))
		{
			if ((sizeof(dU32) + sizeof(DDSHeader) + sizeof(DDSHeaderDXT10)) >= byteSize)
				return DDSResult::EFailedSize;
			
			outDDSTexture.m_pHeaderDXT10 = reinterpret_cast<DDSHeaderDXT10*>(pFileBuffer + sizeof(dU32) + sizeof(DDSHeader));
			outDDSTexture.m_format = outDDSTexture.m_pHeaderDXT10->format;
			dxt10Header = true;
		}
		else 
		{
			Graphics::EFormat format;
			switch (header.pixelFormat.fourCC)
			{
			case MakeFourCC('B', 'C', '4', 'U'): format = Graphics::EFormat::BC4_UNORM; break;
			case MakeFourCC('D', 'X', 'T', '1'): format = Graphics::EFormat::BC1_UNORM; break;
			case MakeFourCC('D', 'X', 'T', '3'): format = Graphics::EFormat::BC2_UNORM; break;
			case MakeFourCC('D', 'X', 'T', '5'): format = Graphics::EFormat::BC3_UNORM; break;
			case MakeFourCC('B', 'C', '5', 'U'): format = Graphics::EFormat::BC5_UNORM; break;
			case MakeFourCC('A', 'T', 'I', '2'): format = Graphics::EFormat::BC5_UNORM; break;
			case 0:
				if (HasBitMask(header.pixelFormat, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
				{
					format = Graphics::EFormat::R8G8B8A8_UNORM;
					break;
				}
				else if (HasBitMask(header.pixelFormat, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
				{
					format = Graphics::EFormat::B8G8R8A8_UNORM;
					break;
				}
				return DDSResult::EFailedFormat;
			default:
				return DDSResult::EFailedFormat;
			}
			outDDSTexture.m_format = format;
		}

		dU64 offset = sizeof(dU32) + sizeof(DDSHeader) + (dxt10Header ? sizeof(DDSHeaderDXT10) : 0);
		outDDSTexture.m_pData = reinterpret_cast<void*>(pFileBuffer + offset);
		outDDSTexture.m_pFileBuffer = pFileBuffer;

		file.Close();
		return DDSResult::ESucceed;
	}

	DDSResult DDSTexture::Load(const char* filePath)
	{
		return Load(filePath, *this);
	}

	Graphics::Texture DDSTexture::CreateTextureFromFile(Device* pDevice, CommandList* pCommandList, Buffer& uploadBuffer, const char* filePath, bool sRGB)
	{
		Graphics::DDSTexture ddsTexture;
		Graphics::DDSResult result = ddsTexture.Load(filePath);
		Assert(result == Graphics::DDSResult::ESucceed);
		void* pData = ddsTexture.GetData();
		const Graphics::DDSHeader* pHeader = ddsTexture.GetHeader();
		dU32 format = (dU32)ddsTexture.m_format;
		if (sRGB)
			format++;
		Graphics::Texture texture{};
		texture.Initialize(pDevice, { .usage = Graphics::ETextureUsage::ShaderResource, .dimensions = { pHeader->height, pHeader->width, pHeader->depth + 1 }, .mipLevels = pHeader->mipMapCount, .format = (Graphics::EFormat)format, .clearValue = {0.f, 0.f, 0.f, 0.f} });

		dU32 byteSize = (dU32)texture.GetRequiredIntermediateSize(0, pHeader->mipMapCount);
		BufferDesc desc{ L"UploadBuffer", EBufferUsage::Default, EBufferMemory::CPU, byteSize };
		uploadBuffer.Initialize(pDevice, desc);
		pCommandList->UploadTexture(texture, uploadBuffer, 0, 0, pHeader->mipMapCount, pData);

		ddsTexture.Destroy();
		return texture;
	}

	void DDSTexture::Destroy()
	{
		delete[] m_pFileBuffer;
		m_pFileBuffer = nullptr;
		m_pHeader = nullptr;
		m_pData = nullptr;
	}
}

