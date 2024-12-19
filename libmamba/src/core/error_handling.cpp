#include "mamba/core/error_handling.hpp"

#include <iostream>
#include <exception>
#include <csignal>

#include "spdlog/spdlog.h"
#include <boost/stacktrace.hpp>

#include "mamba/core/output.hpp"


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
        std::cout << fmt::format(
            "############ \n SIGNAL: SIGSEGV (segfault/access-violation) = {} - ABORTING :\n",
            value
        ) << boost::stacktrace::stacktrace()
                  << std::endl;
        std::abort();
    }

    void on_terminate()
    {
        std::cout << "############ \n std::terminate - ABORTING :\n"
                  << boost::stacktrace::stacktrace() << std::endl;
        std::abort();
    }

    void on_quick_exit()
    {
        std::cout << "############ \n QUICK EXIT:\n"
                  << boost::stacktrace::stacktrace()
                  << std::endl;
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
                std::cout << "##### Installing special failure handlers ...... #####" << std::endl;
                previous_segfault_handler = std::signal(SIGSEGV, on_segfault);
                previous_terminate_handler = std::set_terminate(on_terminate);
                std::at_quick_exit(on_quick_exit);
                std::cout << "##### Installing special failure handlers - DONE #####" << std::endl;
            }

            ~FailureHandlers()
            {
                std::cout << "##### Restoring previous special failure handlers ...... #####"
                          << std::endl;
                std::set_terminate(previous_terminate_handler);
                std::signal(SIGSEGV, previous_segfault_handler);
                std::cout << "##### Restoring previous special failure handlers - DONE #####"
                          << std::endl;
            }
        } failure_handlers;




    }


}
