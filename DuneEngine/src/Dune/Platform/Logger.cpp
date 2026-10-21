#include "pch.h"
#include "Dune/Platform/Logger.h"

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
		{
			std::lock_guard lock(m_logMutex);
			m_shouldProcess = false;
		}
		m_logCondition.notify_one();
		m_logThread.join();
	}

	void Logger::PushLog(LogLevel level, const dString& msg)
	{
		{
			std::lock_guard lock(m_logMutex);
			m_pendingMessages.push({ level, msg });
		}
		m_logCondition.notify_one();
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
		std::unique_lock lock(m_logMutex);
		while (true)
		{
			m_logCondition.wait(lock, [this]() { return !m_pendingMessages.empty() || !m_shouldProcess; });
			if (m_pendingMessages.empty())
				return;

			std::pair<LogLevel, dString> msg = std::move(m_pendingMessages.front());
			m_pendingMessages.pop();
			lock.unlock();
			Log(msg.first, msg.second);
			lock.lock();
		}
	}

}
