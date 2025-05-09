#include "pch.h"
#include "Dune/Utilities/DDSLoader.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/Texture.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Core/File.h"

namespace Dune::Graphics 
{
	constexpr dU32 MakeFourCC(char ch0, char ch1, char ch2, char ch3) 
	{
		return (dU32(dU8(ch0)) | (dU32(dU8(ch1)) << 8) | (dU32(dU8(ch2)) << 16) | (dU32(dU8(ch3)) << 24));
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
			dxt10Header = true;
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

	Graphics::Texture DDSTexture::CreateTextureFromFile(Device* pDevice, CommandList* pCommandList, Buffer& uploadBuffer, const char* filePath)
	{
		Graphics::DDSTexture ddsTexture;
		Graphics::DDSResult result = ddsTexture.Load(filePath);
		Assert(result == Graphics::DDSResult::ESucceed && ddsTexture.GetHeaderDXT10());
		void* pData = ddsTexture.GetData();
		const Graphics::DDSHeader* pHeader = ddsTexture.GetHeader();
		const Graphics::DDSHeaderDXT10* pHeaderDXT10 = ddsTexture.GetHeaderDXT10();
		Graphics::Texture texture = Graphics::Texture();
		texture.Initialize(pDevice, { .usage = Graphics::ETextureUsage::ShaderResource, .dimensions = { pHeader->height, pHeader->width, pHeader->depth + 1 }, .mipLevels = pHeader->mipMapCount, .format = pHeaderDXT10->format, .clearValue = {0.f, 0.f, 0.f, 0.f} });

		dU32 byteSize = (dU32)texture.GetRequiredIntermediateSize(0, pHeader->mipMapCount);
		BufferDesc desc{ L"UploadBuffer",EBufferUsage::Constant, EBufferMemory::CPU, byteSize, byteSize };
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

