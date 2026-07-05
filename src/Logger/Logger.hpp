#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace pv {

enum class LogLevel { INFO, WARNING, ERR, DEBUG };

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void log(LogLevel level, const std::string& message);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    std::ofstream m_logFile;
    std::mutex m_mutex;
};

#define LOG_INFO(msg) pv::Logger::getInstance().log(pv::LogLevel::INFO, msg)
#define LOG_ERROR(msg) pv::Logger::getInstance().log(pv::LogLevel::ERR, msg)
}