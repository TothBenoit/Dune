#include "pch.h"
#include "Dune/Core/Logger.h"

namespace Dune
{


	Logger& Logger::GetInstance()
	{
		static Logger instance{};
		return instance;
	}

	void Logger::Info(const char* msg)
	{
		GetInstance().PushLog(LogLevel::Info, msg);	
	}
	void Logger::Warning(const char* msg)
	{
		GetInstance().PushLog(LogLevel::Warning, msg);
	}
	void Logger::Error(const char* msg)
	{
		GetInstance().PushLog(LogLevel::Error, msg);
	}
	void Logger::Critical(const char* msg)
	{
		GetInstance().PushLog( LogLevel::Critical, msg );
	}

	Logger::Logger()
	{
		m_shouldProcess = true;
		m_logThread = std::thread(&Logger::Update, this);
	}

	Logger::~Logger()
	{
		m_shouldProcess = false;
		m_logThread.join();
	}

	void Logger::PushLog(LogLevel level, const dString& msg)
	{
		{
			std::lock_guard lock(m_logMutex);
			m_pendingMessages.push({ level, msg });
		}
	}

	void Logger::Log(LogLevel level, const dString& msg)
	{
		ProfileFunc();
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
			while (!m_pendingMessages.empty())
			{
				std::pair<LogLevel, dString> msg;
				{
					std::lock_guard lock(m_logMutex);
					msg = m_pendingMessages.front();
					m_pendingMessages.pop();
				}
				Log(msg.first, msg.second);
			}
			std::this_thread::yield();
		}
	}

}

