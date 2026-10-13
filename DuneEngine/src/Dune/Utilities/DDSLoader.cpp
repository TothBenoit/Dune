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

	constexpr EFormat ToSRGB(EFormat format)
	{
		switch (format)
		{
		case EFormat::R8G8B8A8_UNORM: return EFormat::R8G8B8A8_UNORM_SRGB;
		case EFormat::B8G8R8A8_UNORM: return EFormat::B8G8R8A8_UNORM_SRGB;
		case EFormat::B8G8R8X8_UNORM: return EFormat::B8G8R8X8_UNORM_SRGB;
		case EFormat::BC1_UNORM:      return EFormat::BC1_UNORM_SRGB;
		case EFormat::BC2_UNORM:      return EFormat::BC2_UNORM_SRGB;
		case EFormat::BC3_UNORM:      return EFormat::BC3_UNORM_SRGB;
		case EFormat::BC7_UNORM:      return EFormat::BC7_UNORM_SRGB;
		default:                      return format;
		}
	}

	DDSResult DDSTexture::Load(const char* filePath, DDSTexture& outDDSTexture)
	{
		Assert(!outDDSTexture.m_pFileBuffer);

		File file;
		if (!File::Open(file, filePath, File::EAccessMode::Read, File::EShareMode::None))
			return DDSResult::EFailedOpen;
		
		dU64 byteSize = file.GetByteSize();
		constexpr char magicWord[4] = { 'D', 'D', 'S', ' ' };
		if (byteSize < sizeof(magicWord))
		{
			file.Close();
			return DDSResult::EFailedSize;
		}

		dU8* pFileBuffer = new dU8[byteSize];
		if (!file.Read(reinterpret_cast<char*>(pFileBuffer), byteSize))
		{
			delete[] pFileBuffer;
			file.Close();
			return DDSResult::EFailedRead;
		}

		for (int i = 0; i < 4; i++) 
		{
			if (pFileBuffer[i] != magicWord[i])
			{
				delete[] pFileBuffer;
				file.Close();
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
			{
				delete[] pFileBuffer;
				file.Close();
				return DDSResult::EFailedSize;
			}
			
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
				delete[] pFileBuffer;
				file.Close();
				return DDSResult::EFailedFormat;
			default:
				delete[] pFileBuffer;
				file.Close();
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

	Graphics::Texture DDSTexture::CreateTextureFromFile(Device& device, CommandList& commandList, Buffer& uploadBuffer, const char* filePath, bool sRGB)
	{
		Graphics::DDSTexture ddsTexture;
		Graphics::DDSResult result = ddsTexture.Load(filePath);
		Assert(result == Graphics::DDSResult::ESucceed);
		void* pData = ddsTexture.GetData();
		const Graphics::DDSHeader* pHeader = ddsTexture.GetHeader();
		const Graphics::EFormat format = sRGB ? ToSRGB(ddsTexture.m_format) : ddsTexture.m_format;
		const Graphics::DDSHeaderDXT10* pHeader10 = ddsTexture.GetHeaderDXT10();
		Assert(!pHeader10 || (pHeader10->resourceDimension != DDSTextureDimension::ETexture3D && pHeader10->arraySize <= 1)); // TODO support
		const dU32 mipLevels = std::max(pHeader->mipMapCount, 1u);
		Graphics::Texture texture{};
		texture.Initialize(device, { .usage = Graphics::ETextureUsage::ShaderResource, .dimensions = { pHeader->width, pHeader->height, 1 }, .mipLevels = mipLevels, .format = format, .clearValue = {0.f, 0.f, 0.f, 0.f} });

		dU32 byteSize = (dU32)texture.GetRequiredIntermediateSize(0, mipLevels);
		BufferDesc desc{ L"UploadBuffer", EBufferUsage::Default, EBufferMemory::CPU, byteSize };
		uploadBuffer.Initialize(device, desc);
		commandList.UploadTexture(texture, uploadBuffer, 0, 0, mipLevels, pData);

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
