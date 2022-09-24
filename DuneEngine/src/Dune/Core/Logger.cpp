#include "pch.h"
#include "Logger.h"

namespace Dune
{
	Logger* Logger::ms_instance = nullptr;

	void Logger::Init()
	{
		if (!ms_instance)
		{
			ms_instance = new Logger();
		}
		else
		{
			Logger::Error("Tried to initialize Logger which is already initialized");
		}
	}

	void Logger::Shutdown()
	{
		if (ms_instance)
		{
			delete(ms_instance);
		}
	}

	void Logger::Info(const char* msg)
	{
		if (ms_instance)
		{
			ms_instance->PushLog(LogLevel::Info, msg);
		}
	}
	void Logger::Warning(const char* msg)
	{
		if (ms_instance)
		{
			ms_instance->PushLog(LogLevel::Warning, msg);
		}
	}
	void Logger::Error(const char* msg)
	{
		if (ms_instance)
		{
			ms_instance->PushLog(LogLevel::Error, msg);
		}
	}
	void Logger::Critical(const char* msg)
	{
		if (ms_instance)
		{
			ms_instance->PushLog( LogLevel::Critical, msg );
		}
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
		}
	}

}

