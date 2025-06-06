#include "pch.h"
#include "Dune/Core/File.h"
#include <windows.h>

namespace Dune
{
	constexpr DWORD ToAccessMode(File::EAccessMode access) 
	{
		switch (access)
		{
		case File::EAccessMode::Read:
			return GENERIC_READ;
		case File::EAccessMode::Write:
			return GENERIC_WRITE;
		case File::EAccessMode::ReadWrite:
			return GENERIC_READ | GENERIC_WRITE;
		default:
			Assert(false);
		}
		return GENERIC_READ;
	}

	constexpr DWORD ToShareMode(File::EShareMode share)
	{
		switch (share)
		{
		case File::EShareMode::None:
			return 0;
		case File::EShareMode::Read:
			return FILE_SHARE_READ;
		case File::EShareMode::Write:
			return FILE_SHARE_WRITE;
		case File::EShareMode::ReadWrite:
			return FILE_SHARE_READ | FILE_SHARE_WRITE;
		default:
			Assert(false);
		}
		return 0;
	}

	constexpr DWORD ToSeekMode(File::ESeekMode seek)
	{
		switch (seek)
		{
		case File::ESeekMode::Begin:
			return FILE_BEGIN;
		case File::ESeekMode::Current:
			return FILE_CURRENT;
		case File::ESeekMode::End:
			return FILE_END;
		default:
			Assert(false);
		}
		return 0;
	}

	bool File::Open(File& outFile, const char* filename, EAccessMode access, EShareMode share)
	{
		HANDLE handle = CreateFileA(filename, ToAccessMode(access), ToShareMode(share), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		outFile.m_pFile = handle;
		return handle != INVALID_HANDLE_VALUE;
	}

	bool File::Read(void* pBuffer, dU32 byteSize)
	{
		DWORD bytesRead = 0;
		return ReadFile(m_pFile, pBuffer, byteSize, &bytesRead, NULL);
	}

	bool File::Write(void* pData, dU32 byteSize)
	{
		DWORD byteWritten = 0;
		return WriteFile(m_pFile, pData, byteSize, &byteWritten, NULL);
	}

	void File::Seek(dU64 byteSize, ESeekMode mode)
	{
		LARGE_INTEGER move;
		move.QuadPart = byteSize;
		SetFilePointerEx(m_pFile, move, NULL, ToSeekMode(mode));
	}

	dU64 File::Tell()
	{
		LARGE_INTEGER pos;
		SetFilePointerEx(m_pFile, {0}, &pos, FILE_CURRENT);
		return pos.QuadPart;
	}

	dU64 File::GetByteSize()
	{
		LARGE_INTEGER size;
		GetFileSizeEx(m_pFile, &size);
		return size.QuadPart;
;	}

	bool File::Close()
	{
		return CloseHandle(m_pFile);
	}
}