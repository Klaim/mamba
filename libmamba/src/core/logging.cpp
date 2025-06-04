// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <mamba/core/context.hpp>
#include <mamba/core/logging.hpp>
#include <mamba/core/output.hpp> // TODO: remove
#include <mamba/core/util.hpp>

#include <spdlog/spdlog.h>


import std;

namespace mamba
{

    /*****************
     * MessageLogger *
     *****************/

    struct MessageLoggerData
    {
        static std::mutex m_mutex;
        static bool use_buffer;
        static std::vector<std::pair<std::string, log_level>> m_buffer;
    };

    MessageLogger::MessageLogger(log_level level)
        : m_level(level)
        , m_stream()
    {
    }

    MessageLogger::~MessageLogger()
    {
        if (!MessageLoggerData::use_buffer && Console::is_available())
        {
            emit(m_stream.str(), m_level);
        }
        else
        {
            const std::lock_guard<std::mutex> lock(MessageLoggerData::m_mutex);
            MessageLoggerData::m_buffer.push_back({ m_stream.str(), m_level });
        }
    }

    void MessageLogger::emit(const std::string& msg, const log_level& level)
    {
        auto str = Console::hide_secrets(msg);
        switch (level)
        {
            case log_level::critical:
                SPDLOG_CRITICAL(prepend(str, "", std::string(4, ' ').c_str()));
                if (Console::instance().context().output_params.logging_level != log_level::off)
                {
                    spdlog::dump_backtrace();
                }
                break;
            case log_level::err:
                SPDLOG_ERROR(prepend(str, "", std::string(4, ' ').c_str()));
                break;
            case log_level::warn:
                SPDLOG_WARN(prepend(str, "", std::string(4, ' ').c_str()));
                break;
            case log_level::info:
                SPDLOG_INFO(prepend(str, "", std::string(4, ' ').c_str()));
                break;
            case log_level::debug:
                SPDLOG_DEBUG(prepend(str, "", std::string(4, ' ').c_str()));
                break;
            case log_level::trace:
                SPDLOG_TRACE(prepend(str, "", std::string(4, ' ').c_str()));
                break;
            default:
                break;
        }
    }

    std::stringstream& MessageLogger::stream()
    {
        return m_stream;
    }

    void MessageLogger::activate_buffer()
    {
        MessageLoggerData::use_buffer = true;
    }

    void MessageLogger::deactivate_buffer()
    {
        MessageLoggerData::use_buffer = false;
    }

    void MessageLogger::print_buffer(std::ostream& /*ostream*/)
    {
        decltype(MessageLoggerData::m_buffer) tmp;

        {
            const std::lock_guard<std::mutex> lock(MessageLoggerData::m_mutex);
            MessageLoggerData::m_buffer.swap(tmp);
        }

        for (const auto& [msg, level] : tmp)
        {
            emit(msg, level);
        }

        spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) { l->flush(); });
    }


}