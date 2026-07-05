#include "Logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace pv {

Logger::Logger() {
    m_logFile.open("playvault.log", std::ios::app);
    log(LogLevel::INFO, "=== PlayVault started ===");
}

Logger::~Logger() {
    log(LogLevel::INFO, "=== PlayVault closed ===");
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string levelStr;
    switch (level) {
        case LogLevel::INFO:    levelStr = "[INFO]"; break;
        case LogLevel::WARNING: levelStr = "[WARN]"; break;
        case LogLevel::ERR:     levelStr = "[ERROR]"; break;
        case LogLevel::DEBUG:   levelStr = "[DEBUG]"; break;
    }

    auto now = std::chrono::system_clock::now();
    [[maybe_unused]] auto time = std::chrono::system_clock::to_time_t(now);

    std::string logEntry = levelStr + " " + message;
    
    std::cout << logEntry << std::endl;
    if (m_logFile.is_open()) {
        m_logFile << logEntry << std::endl;
    }
}

} 