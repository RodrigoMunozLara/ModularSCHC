#pragma once
#include <memory>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

inline std::shared_ptr<spdlog::logger> get_file_logger() {
    static auto logger = []() {
        try {
            // Se creará una SÓLA VEZ en todo el programa, de forma segura entre hilos
            std::string log_pattern = "[%H:%M:%S.%e][%^%L%$][%t], %v";
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs.txt", true);
            auto log = std::make_shared<spdlog::logger>("file_logger", sink);
            log->set_pattern(log_pattern);
            log->set_level(spdlog::level::trace);
            log->flush_on(spdlog::level::trace);
            return log;
        } catch (...) {
            return std::shared_ptr<spdlog::logger>(nullptr);
        }
    }();
    return logger;
}