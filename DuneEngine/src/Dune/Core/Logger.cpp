#include "pch.h"
#include "Logger.h"

namespace Dune
{
	std::thread Logger::m_logThread;
	std::mutex Logger::m_logMutex;
	std::queue<std::pair<Logger::LogLevel, dString>> Logger::m_pendingMessages;
	std::atomic_bool Logger::m_shouldProcess = false;

	void Logger::Init()
	{
		m_shouldProcess = true;
		m_logThread = std::thread(Logger::Update);
	}

	void Logger::Shutdown()
	{
		m_shouldProcess = false;
		m_logThread.join();
	}
	void Logger::Info(const char* msg)
	{
		{
			std::lock_guard lock(m_logMutex);
			m_pendingMessages.push({ LogLevel::Info, msg });
		}
	}
	void Logger::Warning(const char* msg)
	{
		{
			std::lock_guard lock(m_logMutex);
			m_pendingMessages.push({ LogLevel::Warning, msg });
		}
	}
	void Logger::Error(const char* msg)
	{
		{
			std::lock_guard lock(m_logMutex);
			m_pendingMessages.push({ LogLevel::Error, msg });
		}
	}
	void Logger::Critical(const char* msg)
	{
		{
			std::lock_guard lock(m_logMutex);
			m_pendingMessages.push({ LogLevel::Critical, msg });
		}
	}

	void Logger::Log(LogLevel level, const dString& msg)
	{
		switch (level)
		{
		case LogLevel::Info:
			std::cout << "[INFO] " << msg << "\n";
			break;
		case LogLevel::Warning:
			std::cout << "[WARNING] " << msg << "\n";
			break;
		case LogLevel::Error:
			std::cout << "[ERROR] " << msg << "\n";
			break;
		case LogLevel::Critical:
			std::cout << "[CRITICAL] " << msg << "\n";
			break;
		default:
			std::cout << "[UNDEFINED] " << msg << "\n";
			break;
		}
	}

	void Logger::Update()
	{
		while (m_shouldProcess)
		{
			if (!m_pendingMessages.empty())
			{
				std::pair<LogLevel, dString> msg;
				{
					std::lock_guard lock(m_logMutex);
					msg = m_pendingMessages.front();
					m_pendingMessages.pop();
				}
				Log(msg.first, msg.second);
			}
		}
	}

}

