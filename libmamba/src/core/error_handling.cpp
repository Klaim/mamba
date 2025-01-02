#include "mamba/core/error_handling.hpp"

#include <iostream>
#include <exception>
#include <csignal>

#include "spdlog/spdlog.h"
#include <fmt/ranges.h>
#include <fmt/format.h>
#include <boost/stacktrace.hpp>

#include "mamba/core/output.hpp"

template <>
struct fmt::formatter<boost::stacktrace::frame> : formatter<std::string_view>
{
    template <typename FORMAT_CONTEXT>
    auto format(const boost::stacktrace::frame& rhs, FORMAT_CONTEXT& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}:{}\n", rhs.source_file(), rhs.source_line());
    }
};

template <>
struct fmt::formatter<boost::stacktrace::stacktrace> : formatter<std::string_view>
{
    template <typename FORMAT_CONTEXT>
    auto format(const boost::stacktrace::stacktrace& rhs, FORMAT_CONTEXT& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", fmt::join(rhs.as_vector(), "\n"));
    }
};

namespace mamba
{
    namespace
    {
        void maybe_dump_backtrace(mamba_error_code ec)
        {
            if (ec == mamba_error_code::internal_failure)
            {
                spdlog::dump_backtrace();
            }
        }

    }

    mamba_error::mamba_error(const std::string& msg, mamba_error_code ec)
        : base_type(msg)
        , m_error_code(ec)
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error::mamba_error(const char* msg, mamba_error_code ec)
        : base_type(msg)
        , m_error_code(ec)
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error::mamba_error(const std::string& msg, mamba_error_code ec, std::any&& data)
        : base_type(msg)
        , m_error_code(ec)
        , m_data(std::move(data))
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error::mamba_error(const char* msg, mamba_error_code ec, std::any&& data)
        : base_type(msg)
        , m_error_code(ec)
        , m_data(std::move(data))
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error_code mamba_error::error_code() const noexcept
    {
        return m_error_code;
    }

    const std::any& mamba_error::data() const noexcept
    {
        return m_data;
    }

    constexpr const char* mamba_aggregated_error::m_base_message;  // = "Many errors occurred:\n";

    mamba_aggregated_error::mamba_aggregated_error(error_list_t&& error_list)
        : base_type(mamba_aggregated_error::m_base_message, mamba_error_code::aggregated)
        , m_error_list(std::move(error_list))
        , m_aggregated_message()
    {
    }

    const char* mamba_aggregated_error::what() const noexcept
    {
        if (m_aggregated_message.empty())
        {
            m_aggregated_message = m_base_message;

            for (const mamba_error& er : m_error_list)
            {
                m_aggregated_message += er.what();
                m_aggregated_message += "\n";
            }
        }
        return m_aggregated_message.c_str();
    }

    tl::unexpected<mamba_error> make_unexpected(const char* msg, mamba_error_code ec)
    {
        return tl::make_unexpected(mamba_error(msg, ec));
    }

    tl::unexpected<mamba_error> make_unexpected(const std::string& msg, mamba_error_code ec)
    {
        return tl::make_unexpected(mamba_error(msg, ec));
    }

    tl::unexpected<mamba_aggregated_error> make_unexpected(std::vector<mamba_error>&& error_list)
    {
        return tl::make_unexpected(mamba_aggregated_error(std::move(error_list)));
    }


    void on_segfault(int value)
    {
        fmt::print(
            "############ \n SIGNAL: SIGSEGV (segfault/access-violation) = {} - ABORTING :\n{}",
            value,
            boost::stacktrace::stacktrace()
        );
        std::abort();
    }

    void on_terminate()
    {
        fmt::print("############ \n std::terminate - ABORTING :\n{}", boost::stacktrace::stacktrace());
        std::abort();
    }

    void on_quick_exit()
    {
        fmt::print("############ \n QUICK EXIT:\n {}", boost::stacktrace::stacktrace());
    }

    namespace
    {
        struct FailureHandlers
        {
            std::terminate_handler previous_terminate_handler;

            using signal_handler = void (*)(int);
            signal_handler previous_segfault_handler;

            FailureHandlers()
            {
                fmt::print("##### Installing special failure handlers ...... #####");
                previous_segfault_handler = std::signal(SIGSEGV, on_segfault);
                previous_terminate_handler = std::set_terminate(on_terminate);
                std::at_quick_exit(on_quick_exit);
                fmt::print("##### Installing special failure handlers - DONE #####");
            }

            ~FailureHandlers()
            {
                fmt::print("##### Restoring previous special failure handlers ...... #####");
                std::set_terminate(previous_terminate_handler);
                std::signal(SIGSEGV, previous_segfault_handler);
                fmt::print("##### Restoring previous special failure handlers - DONE #####");
            }
        } failure_handlers;




    }


}
