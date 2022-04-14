#include "Logger.h"

#include <iostream>

void Dune::Logger::Info(const std::string& msg)
{
	Log(LogLevel::Info, msg);
}

void Dune::Logger::Warning(const std::string& msg)
{
	Log(LogLevel::Warning, msg);
}

void Dune::Logger::Error(const std::string& msg)
{
	Log(LogLevel::Error, msg);
}

void Dune::Logger::Critical(const std::string& msg)
{
	Log(LogLevel::Critical, msg);
}

void Dune::Logger::Log(LogLevel level, const std::string& msg)
{
	switch (level)
	{
	case LogLevel::Info:
		std::cout << "[INFO] " << msg.c_str() << std::endl;
			break;
	case LogLevel::Warning:
		std::cout << "[WARNING] " << msg.c_str() << std::endl;
		break;
	case LogLevel::Error:
		std::cout << "[ERROR] " << msg.c_str() << std::endl;
		break;
	case LogLevel::Critical:
		std::cout << "[CRITICAL] " << msg.c_str() << std::endl;
		break;
	default:
		std::cout << "[UNDEFINED] " << msg.c_str() << std::endl;
		break;
	}
}
