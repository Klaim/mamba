// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_HPP
#define MAMBA_CORE_LOGGING_HPP

#include <string>
#include <sstream>
#include <vector>

namespace mamba
{

    enum class log_level
    {
        trace,
        debug,
        info,
        warn,
        err,
        critical,
        off
    };

    enum class log_source
    {
        libmamba,
        libcurl,
        libsolv
    };

    auto name_of(log_source source) -> std::string; // string_view? const char*?

    struct LogRecord
    {
        std::string message;
        log_level level;
        log_source source;
    };


    class LogHandler
    {
    public:

        auto start_log_handling(log_level current_level, std::vector<log_source> sources) -> void;
        auto stop_log_handling() -> void;

        auto log(LogRecord record) -> void;

        auto dump_stacktrace() -> void;
        auto flush() -> void;
    };

    auto set_log_handler(LogHandler handler) -> LogHandler;

    auto set_log_level(log_level new_level) -> log_level;

    auto log_stacktrace() -> void;
    auto flush_logs() -> void;


///////////////////////////////////////////////////////
    // PREVIOUS IMPLEMENTATION, MIGT DISAPPEAR
    class MessageLogger
    {
    public:

        MessageLogger(log_level level);
        ~MessageLogger();

        std::stringstream& stream();

        static void activate_buffer();
        static void deactivate_buffer();
        static void print_buffer(std::ostream& ostream);

    private:

        log_level m_level;
        std::stringstream m_stream;

        static void emit(const std::string& msg, const log_level& level);
    };



}

#undef LOG
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL

#define LOG(severity) mamba::MessageLogger(severity).stream()
#define LOG_TRACE LOG(mamba::log_level::trace)
#define LOG_DEBUG LOG(mamba::log_level::debug)
#define LOG_INFO LOG(mamba::log_level::info)
#define LOG_WARNING LOG(mamba::log_level::warn)
#define LOG_ERROR LOG(mamba::log_level::err)
#define LOG_CRITICAL LOG(mamba::log_level::critical)


#endif
